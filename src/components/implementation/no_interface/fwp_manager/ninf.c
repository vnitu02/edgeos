#include <cos_defkernel_api.h>
#include "ninf.h"
#include "ninf_util.h"

#define MALLOC_BUMP_SZ (2*PAGE_SIZE)

void *malloc_bump = NULL;
int malloc_left_sz = 0;
extern struct cos_pci_device devices[PCI_DEVICE_NUM];
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

void
cos_tx_cb(void *userdata)
{
	struct eos_ring_node *n;

	n = (struct eos_ring_node *)userdata;
	n->state = PKT_SENT_DONE;
}

void
ninf_init(void)
{
	int ret;

	ret = dpdk_init();
	if (ret < 0) {
		printc("DPDK EAL init return error %d \n", ret);
		goto halt;
	}

	check_all_ports_link_status(NUM_NIC_PORTS, 3);
	ninf_ft_init(&ninf_ft, EOS_MAX_FLOW_NUM, sizeof(struct eos_ring *));

halt:
	printc("going to SPIN\n");
	SPIN();
	return ;
}

