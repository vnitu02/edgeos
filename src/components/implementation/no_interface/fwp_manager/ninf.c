#include <cos_defkernel_api.h>
#include "ninf.h"
#include "ninf_util.h"

#define RX_MBUF_DATA_SIZE 2048
#define MBUF_SIZE (RX_MBUF_DATA_SIZE + RTE_PKTMBUF_HEADROOM + sizeof(struct rte_mbuf))
#define MALLOC_BUMP_SZ (2*PAGE_SIZE)

void *malloc_bump = NULL;
int malloc_left_sz = 0;
extern struct cos_pci_device devices[PCI_DEVICE_NUM];
u8_t nb_ports;
struct rte_mempool *mbuf_pool;
extern struct ninf_ft ninf_ft;

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
	return (void *)cos_hw_map(CURR_CINFO(), BOOT_CAPTBL_SELF_INITHW_BASE, (paddr_t)paddr, size);
}

void *
cos_map_virt_to_phys(void *addr)
{
	int ret;
	vaddr_t vaddr = (vaddr_t)addr;

	assert((vaddr & 0xfff) == 0);
	ret = call_cap_op(CURR_CINFO()->pgtbl_cap, CAPTBL_OP_INTROSPECT, (vaddr_t)vaddr, 0, 0, 0);
	return ret & 0xfffff000;
}

static inline void *
__cos_dpdk_page_alloc(int sz)
{
	void *addr;

	sz = round_to_page(sz + PAGE_SIZE - 1);
        addr = (void *)cos_page_bump_allocn(CURR_CINFO(), sz);
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
	assert(nb_ports == NUM_NIC_PORTS);
	/* printc("%d ports available.\n", nb_ports); */

	mbuf_pool = rte_pktmbuf_pool_create("RX_MBUF_POOL", NUM_MBUFS * nb_ports, 0, 0, MBUF_SIZE, -1);
	if (!mbuf_pool) return -2;

	if (rte_eth_dev_cos_setup_ports(nb_ports, mbuf_pool) < 0)
		return -2;
	/* printc("\nPort init done.\n"); */

	return 0;
}

/* static void */
/* ninf_pktgen_client(void) */
/* { */
/* 	struct rte_mbuf *mbufs[BURST_SIZE]; */
/* 	u8_t port; */
/* 	u16_t nb_rx, nb_tx; */

/* 	while (1) { */
/* 		for(port=0; port<2; port++) { */
/* 			nb_rx = rte_eth_rx_burst(port, 0, mbufs, BURST_SIZE); */
/* 			if (nb_rx) { */
/* 				nb_tx = rte_eth_tx_burst(port, 0, mbufs, nb_rx); */
/* 				assert(nb_tx != 0); */
/* 			} */
/* 		} */
/* 	} */
/* 	printc("going to SPIN\n"); */
/* 	SPIN(); */
/* } */

void
ninf_init(void)
{
	int ret;
	/* struct rte_mbuf *mbufs_init[BURST_SIZE]; */
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
	ninf_ft_init(&ninf_ft, EOS_MAX_FLOW_NUM, sizeof(struct eos_ring *));
	/* ninf_bride(); */
	/* ninf_pktgen_client(); */

halt:
	printc("going to SPIN\n");
	SPIN();
	return ;
}

