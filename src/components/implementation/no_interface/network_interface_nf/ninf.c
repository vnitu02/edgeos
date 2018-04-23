#include <cos_defkernel_api.h>
#include "ninf.h"
#include "ninf_util.h"

#define NUM_MBUFS 8192
#define BURST_SIZE 32
#define RX_MBUF_DATA_SIZE 2048
#define MBUF_SIZE (RX_MBUF_DATA_SIZE + RTE_PKTMBUF_HEADROOM + sizeof(struct rte_mbuf))
#define N_LOOP 2
#define MALLOC_BUMP_SZ (2*PAGE_SIZE)

void *malloc_bump = NULL;
int malloc_left_sz = 0;
extern struct cos_pci_device devices[PCI_DEVICE_NUM];
struct cos_compinfo *ninf_info;

u8_t nb_ports;
struct rte_mempool *mbuf_pool, *temp_pool;
int tx_cb_cnt = 0;

cos_eal_thd_t
cos_eal_thd_curr(void)
{
	/* FIXME: this assumes dpdk does not use any threading stuff, as interrupt is disabled */
	return 0;
}

int
cos_eal_thd_create(cos_eal_thd_t *thd_id, void *(*func)(void *), void *arg)
{
	return 0;
}

void *
cos_map_phys_to_virt(void *paddr, unsigned int size)
{
	return (void *)cos_hw_map(ninf_info, BOOT_CAPTBL_SELF_INITHW_BASE, (paddr_t)paddr, size);
}

void *
cos_map_virt_to_phys(void *addr)
{
	int ret;
	vaddr_t vaddr = (vaddr_t)addr;

	assert((vaddr & 0xfff) == 0);
	ret = call_cap_op(cos_defcompinfo_curr_get()->ci.pgtbl_cap, CAPTBL_OP_INTROSPECT, (vaddr_t)vaddr, 0, 0, 0);
	return ret & 0xfffff000;
}

static inline void *
__cos_dpdk_page_alloc(int sz)
{
	void *addr;

	sz = round_to_page(sz + PAGE_SIZE - 1);
        addr = (void *)cos_page_bump_allocn(&cos_defcompinfo_curr_get()->ci, sz);
	assert(addr);
	return addr;
}

static inline void *
__cos_dpdk_malloc(int sz)
{
	void *ret;

	if (malloc_left_sz < sz) {
		malloc_bump = __cos_dpdk_page_alloc(MALLOC_BUMP_SZ);
		malloc_left_sz = MALLOC_BUMP_SZ;
	}
	ret = malloc_bump;
	malloc_bump += sz;
	malloc_left_sz -= sz;
	return ret;
}

/* type 0 - page alloc; 1 - malloc */
void *
cos_mem_alloc(int size, int type)
{
	if (type && size < MALLOC_BUMP_SZ) return __cos_dpdk_malloc(size);
	return __cos_dpdk_page_alloc(size);
}

void
cos_dpdk_print(char *s, int len)
{
	cos_llprint(s, len);
}

void
cos_tx_cb(void *userdata)
{
	rte_pktmbuf_free((struct rte_mbuf *)userdata);
	tx_cb_cnt++;
	/* int t; */
	/* t = *(int *)userdata; */
	/* *(int *)userdata = t +1; */
}

int
dpdk_init(void)
{
	/* Dummy argc and argv */
	int argc = 3;

	/* single core */
	char arg1[] = "DEBUG", arg2[] = "-l", arg3[] = "0";
	char *argv[] = {arg1, arg2, arg3};

	int ret;
	u8_t i;

	ret = rte_eal_init(argc, argv);
	if (ret < 0) return ret;
	/* printc("\nDPDK EAL init done.\n"); */

	nb_ports = rte_eth_dev_count();
	if (!nb_ports) return -1;
	/* printc("%d ports available.\n", nb_ports); */

	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports, 0, 0, MBUF_SIZE, -1);
	if (!mbuf_pool) return -2;
	temp_pool = rte_pktmbuf_pool_create("TEMP_POOL", NUM_MBUFS * nb_ports, 0, 0, MBUF_SIZE - RX_MBUF_DATA_SIZE, -1);
	if (!temp_pool) return -2;

	//TODO: Configure each port
	/* for (i = 0; i < nb_ports; i++) {} */
	if (rte_eth_dev_cos_setup_ports(nb_ports, mbuf_pool) < 0)
		return -2;
	/* printc("\nPort init done.\n"); */

	return 0;
}

static inline void
process_batch(int port, struct rte_mbuf **mbufs, int cnt)
{
	struct rte_mbuf *tx_mbufs[BURST_SIZE];
	int i;

	if (rte_pktmbuf_alloc_bulk(temp_pool, tx_mbufs, cnt)) {
		assert(0);
	}
	/* cnt = 0; */
	for(i=0; i<cnt; i++) {
		tx_mbufs[i]->buf_addr     = mbufs[i]->buf_addr;
		tx_mbufs[i]->buf_physaddr = mbufs[i]->buf_physaddr;
		tx_mbufs[i]->data_len = mbufs[i]->data_len;
		/* tx_mbufs[i]->userdata     = &tx_cb_cnt; */
		/* tx_mbufs[i]->pkt_len = 64; */
		tx_mbufs[i]->userdata     = (void *)mbufs[i];
		/* tx_mbufs[i]->buf_len = mbufs[i]->buf_len; */
	}
	rte_eth_tx_burst(port, 0, tx_mbufs, cnt);
}

static void
ninf_bride(void)
{
	struct rte_mbuf *mbufs[BURST_SIZE];
	u8_t port;
	int tot_rx = 0, tot_tx = 0;

	while (1) {
		for(port=0; port<2; port++) {
			const u16_t nb_rx = rte_eth_rx_burst(port, 0, mbufs, BURST_SIZE);
			tot_rx += nb_rx;
			/* printc("Total RX: %d %d\n", tot_rx, g_dbg); */

			/* struct ether_hdr *ehdr; */
			/* mbufs_init[0] = rte_pktmbuf_alloc(mbuf_pool); */
			/* ehdr = (struct ether_hdr *)rte_pktmbuf_append(mbufs_init[0], ETHER_HDR_LEN); */
			/* rte_eth_macaddr_get(0, &ehdr->s_addr); */
			/* rte_eth_macaddr_get(0, &ehdr->d_addr); */
			/* ehdr->ether_type = ETHER_TYPE_IPv4; */
			/* mbufs_init[0]->port = port; */
			if (nb_rx) {
				/* printc("Port %d received %d packets\n", port, nb_rx); */
				/* rte_pktmbuf_dump(stdout, mbufs[0], 20); */
				/* printc("dbg ninf p %d pl %d dl %d\n", mbufs[0]->port, mbufs[0]->pkt_len, mbufs[0]->data_len); */
				/* printc("Total RX: %d \n", tot_rx); */
				/* print_ether_addr(mbufs[0]); */
				process_batch(!port, mbufs, nb_rx);
				/* const u16_t nb_tx = rte_eth_tx_burst(!port, 0, mbufs, nb_rx); */
				/* assert(nb_tx != 0); */
				/* tot_tx += nb_tx; */
			}
		}
	}
	printc("going to SPIN\n");
	SPIN();
}

static void
ninf_pktgen_client(void)
{
	struct rte_mbuf *mbufs[BURST_SIZE];
	u8_t port;
	u16_t nb_rx, nb_tx;

	while (1) {
		for(port=0; port<2; port++) {
			nb_rx = rte_eth_rx_burst(port, 0, mbufs, BURST_SIZE);
			if (nb_rx) {
				nb_tx = rte_eth_tx_burst(port, 0, mbufs, nb_rx);
				assert(nb_tx != 0);
			}
		}
	}
	printc("going to SPIN\n");
	SPIN();
}

void
cos_init(void)
{
	int ret;
	/* struct rte_mbuf *mbufs_init[BURST_SIZE]; */
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);

	cos_defcompinfo_init();
	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	ninf_info = cos_compinfo_get(cos_defcompinfo_curr_get());
	ret = dpdk_init();
	if (ret < 0) {
		printc("DPDK EAL init return error %d \n", ret);
		goto halt;
	}
	if (nb_ports < 2) {
		printc("Too few ports\n");
		goto halt;
	}

	/* if(rte_pktmbuf_alloc_bulk_cos(mbuf_pool, mbufs_init, BURST_SIZE)) { */
	/* 	printc("Couldn't allocate packets\n"); */
	/* 	goto halt; */
	/* } */
	check_all_ports_link_status(nb_ports, 3);

	ninf_bride();
	/* ninf_pktgen_client(); */

halt:
	printc("going to SPIN\n");
	SPIN();
	return ;
}

