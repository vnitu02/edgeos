#include <cos_defkernel_api.h>
#include "eos_pkt.h"
#include "ninf.h"
#include "ninf_util.h"
#include "fwp_chain_cache.h"
#include "eos_sched.h"

#define NO_FLOW_ISOLATION
/* #define PER_FLOW_CHAIN */
/* #define SINGLE_PING_CHURN_TEST */
/* #define FIXED_NF_CHAIN */
#define DPDK_PKT_OFF 256
#define NF_PER_CORE_BATCH 1
#define DPDK_PKT2MBUF(pkt) ((struct rte_mbuf *)((void *)(pkt) - DPDK_PKT_OFF))
#define IN2OUT_PORT(port) (!(port))

struct rte_mempool *rx_mbuf_pool;
struct eos_ring *ninf_ft_data[EOS_MAX_FLOW_NUM];
static struct ninf_ft ninf_ft;
static struct rte_mbuf *rx_batch_mbufs[BURST_SIZE];
static int major_core_id, minor_core_id;
static struct nf_chain *global_chain = NULL;
static struct eos_ring *global_rx_out;
static struct eos_ring *fix_rx_outs[EOS_MAX_FLOW_NUM] = {NULL};
static int chain_idx = 0;

static inline int
ninf_pkt_collect(struct eos_ring *r)
{
	struct eos_ring_node *n;
	n = GET_RING_NODE(r, r->head & EOS_RING_MASK);
	if (n->state == PKT_SENT_DONE) {
		rte_pktmbuf_free(DPDK_PKT2MBUF(n->pkt));
		n->state   = PKT_EMPTY;
		ps_cc_barrier();
		n->pkt     = NULL;
		n->pkt_len = 0;
		r->head++;
		return 1;
	}
	return 0;
}
static inline int
ninf_flow2_core(struct rte_mbuf *mbuf, struct pkt_ipv4_5tuple *key, uint32_t rss)
{
	int r;

#ifdef NO_FLOW_ISOLATION
	return NF_MIN_CORE;
#else
	r = major_core_id;
	minor_core_id++;
	if (minor_core_id == NF_PER_CORE_BATCH) {
		minor_core_id = 0;
		major_core_id++;
	}
	if (major_core_id == NF_MAX_CORE) major_core_id = NF_MIN_CORE;
	return r;
#endif
}

static inline struct eos_ring *
ninf_flow_tbl_lkup(struct rte_mbuf *mbuf, struct pkt_ipv4_5tuple *key, uint32_t rss)
{
	struct eos_ring **ret = NULL;
	int idx = 0;

	idx = ninf_ft_lookup_key(&ninf_ft, key, rss, (void **)&ret);
	if (idx < 0) return NULL;
	else return *ret;
}

static inline void
ninf_flow_tbl_add(struct pkt_ipv4_5tuple *key, uint32_t rss, struct eos_ring *data)
{
	struct eos_ring **ret = NULL;
	ninf_ft_add_key(&ninf_ft, key, rss, (void **)&ret);
	*ret = (struct eos_ring *)data;
}

static inline struct eos_ring *
ninf_setup_new_chain(struct nf_chain *chain)
{
	struct eos_ring *nf_in_ring, *rx_out, *nf_out;
	struct mca_conn *conn;

	nf_in_ring = get_input_ring((void *)(chain->first_nf->shmem_addr));
	rx_out = cos_page_bump_allocn(CURR_CINFO(), round_up_to_page(FWP_RINGS_SIZE));
	assert(rx_out);
	eos_rings_init((void *)rx_out);
	nf_out = get_output_ring((void *)(chain->last_nf->shmem_addr));
	ninf_tx_add_ring(nf_out);
	rx_out = get_output_ring((void *)rx_out);
	fwp_chain_activate(chain);
	conn = mca_conn_create(rx_out, nf_in_ring);
	return rx_out;
}

static inline void
ninf_proc_new_flow(struct rte_mbuf *mbuf, struct pkt_ipv4_5tuple *key, uint32_t rss)
{
	int cid;
	struct eos_ring *rx_out;
	struct nf_chain *new_chain;

	cid = ninf_flow2_core(mbuf, key, rss);
	new_chain = fwp_chain_get(FWP_CHAIN_CLEANED, cid);
	rx_out = ninf_setup_new_chain(new_chain);
	ninf_flow_tbl_add(key, rss, rx_out);
}

static inline struct eos_ring *
ninf_get_nf_ring(struct rte_mbuf *mbuf)
{
#ifdef NO_FLOW_ISOLATION
	if (!global_chain) {
		printc("dbg new flow\n");
		global_chain = fwp_chain_get(FWP_CHAIN_CLEANED, NF_MIN_CORE);
		assert(global_chain);
		global_rx_out = ninf_setup_new_chain(global_chain);
		fix_rx_outs[chain_idx++] = global_rx_out;
	}
	return global_rx_out;
#endif

#ifdef SINGLE_PING_CHURN_TEST
	int cid;
	struct eos_ring *ninf_ring;
	struct nf_chain *ping_chain;

	cid = ninf_flow2_core(NULL, NULL, 0);
	ping_chain = fwp_chain_get(FWP_CHAIN_CLEANED, cid);
	assert(ping_chain);
	ninf_ring = ninf_setup_new_chain(ping_chain);
	return ninf_ring;
#endif

#ifdef PER_FLOW_CHAIN
	uint32_t rss;
	struct eos_ring *ninf_ring;
	struct pkt_ipv4_5tuple pkt_key;

	ninf_fill_key_symmetric(&pkt_key, mbuf);
	rss = ninf_rss(&pkt_key);
	ninf_ring = ninf_flow_tbl_lkup(mbuf, &pkt_key, rss);
	if (!ninf_ring) {
		ninf_proc_new_flow(mbuf, &pkt_key, rss);
		ninf_ring = ninf_flow_tbl_lkup(mbuf, &pkt_key, rss);
		fix_rx_outs[chain_idx++] = ninf_ring;
	}
	return ninf_ring;
#endif

#ifdef FIXED_NF_CHAIN
	uint32_t rss;
	struct eos_ring *ninf_ring;       
	struct nf_chain *fix_chain;
	struct pkt_ipv4_5tuple pkt_key;
	int cid;

	ninf_fill_key_symmetric(&pkt_key, mbuf);
	rss = ninf_rss(&pkt_key);
	ninf_ring = ninf_flow_tbl_lkup(mbuf, &pkt_key, rss);

	if (!ninf_ring) {
		if (!fix_rx_outs[chain_idx]) {
			cid = ninf_flow2_core(NULL, NULL, 0);
			fix_chain = fwp_chain_get(FWP_CHAIN_CLEANED, cid);
			assert(fix_chain);

			fix_rx_outs[chain_idx] = ninf_ring = ninf_setup_new_chain(fix_chain);
		} else {
			ninf_ring = fix_rx_outs[chain_idx];
		}
		chain_idx = (chain_idx + 1) % EOS_MAX_FLOW_NUM;
		ninf_flow_tbl_add(&pkt_key, rss, ninf_ring);
		ninf_ring = ninf_flow_tbl_lkup(mbuf, &pkt_key, rss);
		assert(ninf_ring);    
        }
	return ninf_ring;
#endif   
}

static inline void
ninf_rx_proc_mbuf(struct rte_mbuf *mbuf, int in_port)
{
	struct eos_ring *ninf_ring;
	int r;

	assert(mbuf);

        if (unlikely(!ninf_pkt_is_ipv4(mbuf))) {
		rte_eth_tx_burst(!in_port, 0, &mbuf, 1);
                return ;
	}
	ninf_ring = ninf_get_nf_ring(mbuf);
	assert(ninf_ring);
	do {
		ninf_pkt_collect(ninf_ring);
		r = eos_pkt_send(ninf_ring, rte_pktmbuf_mtod(mbuf, void *), rte_pktmbuf_data_len(mbuf), IN2OUT_PORT(in_port));
	} while (r == -ECOLLET);
	/* drop pkts */
	if (r) {
		/* printc("D %d\n", ninf_ring->tail & EOS_RING_MASK); */
		rte_pktmbuf_free(mbuf);
	}
}

static inline void
ninf_rx_proc_batch(struct rte_mbuf **mbufs, int nbuf, int in_port)
{
	int i;
	for(i=0; i<nbuf; i++) {
		ninf_rx_proc_mbuf(mbufs[i], in_port);
	}
}

void
ninf_rx_loop()
{
	int tot_rx = 0, port, i=0;

	while (1) {
		if (fix_rx_outs[i]) ninf_pkt_collect(fix_rx_outs[i]);
		i = (i+1) % EOS_MAX_FLOW_NUM;
		for(port=0; port<NUM_NIC_PORTS; port++) {
			const u16_t nb_rx = rte_eth_rx_burst(port, 0, rx_batch_mbufs, BURST_SIZE);
			tot_rx += nb_rx;
			if (nb_rx) ninf_rx_proc_batch(rx_batch_mbufs, nb_rx, port);
		}
	}
}

void
ninf_rx_init()
{
	major_core_id = NF_MIN_CORE;
	minor_core_id = 0;
	global_chain = NULL;
	ninf_ft_init(&ninf_ft, EOS_MAX_FLOW_NUM, sizeof(struct eos_ring *));
}
