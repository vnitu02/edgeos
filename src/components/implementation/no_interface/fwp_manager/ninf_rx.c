#include <cos_defkernel_api.h>
#include "eos_pkt.h"
#include "ninf.h"
#include "ninf_util.h"

#define BURST_SIZE 32
#define DPDK_PKT_OFF 256
#define DPDK_PKT2MBUF(pkt) ((struct rte_mbuf *)((void *)(pkt) - DPDK_PKT_OFF))

struct ninf_ft ninf_ft;

static inline void
ninf_pkt_collect(struct eos_ring *r)
{
	struct eos_ring_node *n;
	n = GET_RING_NODE(r, r->head & EOS_RING_MASK);
	if (n->state == PKT_SENT_DONE) {
		rte_pktmbuf_free(DPDK_PKT2MBUF(n->pkt));
		n->pkt     = NULL;
		n->pkt_len = 0;
		n->state   = PKT_EMPTY;
		r->head++;
	}
}

static inline struct eos_ring *
ninf_flow_tbl_lkup(struct rte_mbuf *mbuf, struct pkt_ipv4_5tuple *key, uint32_t rss)
{
	struct eos_ring **ret = NULL;
	ninf_ft_lookup_key(&ninf_ft, key, rss, (void **)&ret);
	return *ret;
}

static inline void
ninf_proc_new_flow(struct rte_mbuf *mbuf, struct pkt_ipv4_5tuple *key, uint32_t rss)
{
	return ;
}

static inline void
ninf_rx_proc_mbuf(struct rte_mbuf *mbuf, int in_port)
{
	struct pkt_ipv4_5tuple pkt_key;
	struct eos_ring *ninf_ring;
	uint32_t rss;
	void *raw_pkt;

	assert(mbuf);

	ninf_fill_key_symmetric(&pkt_key, mbuf);
	rss = ninf_rss(&pkt_key);
	ninf_ring = ninf_flow_tbl_lkup(mbuf, &pkt_key, rss);
	if (!ninf_ring) {
		ninf_proc_new_flow(mbuf, &pkt_key, rss);
		ninf_ring = ninf_flow_tbl_lkup(mbuf, &pkt_key, rss);
	}
	assert(ninf_ring);
	ninf_pkt_collect(ninf_ring);

	eos_pkt_send(ninf_ring, rte_pktmbuf_mtod(mbuf, void *), rte_pktmbuf_data_len(mbuf));
	/* FIXME: add port to ring buffer; add pkt cnt to ring buffer */
	/* eos_pkt_send(ninf_ring, rte_pktmbuf_mtod(mbuf, void *), rte_pktmbuf_data_len(mbuf), !in_port); */
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
	struct rte_mbuf *mbufs[BURST_SIZE];
	int tot_rx = 0, port;

	while (1) {
		for(port=0; port<2; port++) {
			const u16_t nb_rx = rte_eth_rx_burst(port, 0, mbufs, BURST_SIZE);
			tot_rx += nb_rx;
			if (nb_rx) ninf_rx_proc_batch(mbufs, nb_rx, port);
		}
	}
}
