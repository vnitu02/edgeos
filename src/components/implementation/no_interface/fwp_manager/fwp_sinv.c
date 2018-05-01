#ifndef FWP_SINV_H
#define FWP_SINV_H

#include <nf_hypercall.h>
#include <sl.h>
#include "eos_utils.h"
#include "fwp_manager.h"

extern struct mem_seg templates[EOS_MAX_NF_TYPE_NUM];

struct cos_compinfo *boot_spd_compinfo_get(spdid_t spdid);

static vaddr_t
fwp_malloc(struct click_info *ci, size_t size)
{
	/*FIXME to get compinfo from click_info*/
	struct cos_compinfo *boot_info = boot_spd_compinfo_get(0);
	struct cos_compinfo *chld_info = cos_compinfo_get(&ci->def_cinfo);
	vaddr_t vaddr, src_seg, dst_pg, i;

	src_seg = (vaddr_t) cos_page_bump_allocn(boot_info, size);
	assert(src_seg);
	vaddr = cos_mem_alias(chld_info, boot_info, src_seg);
	assert(vaddr);

	for (i = PAGE_SIZE; i < size; i += PAGE_SIZE) {
		dst_pg = cos_mem_alias(chld_info, boot_info, (src_seg + i));
		assert(dst_pg);
	}

	return vaddr;
}

static int
fwp_confidx_get(void *token)
{
	struct click_info *child_info = (struct click_info *) token;

	return child_info->conf_file_idx;
}

static vaddr_t
fwp_shmem_get(void *token)
{
	struct click_info *child_info = (struct click_info *) token;

	return child_info->shmem_addr;
}

/*
 *  * core function for checkpointing a Click component
 *   */
static void
fwp_checkpoint(void *token, unsigned int nfid)
{
	vaddr_t src_seg, addr;
	size_t vm_range;
	struct cos_compinfo *parent_cinfo_l = boot_spd_compinfo_get(0);
	struct click_info *child_info = (struct click_info *) token;

	/*
	 *        * duplicate memory from this_chld_vm_base to vas_frontier
	 *               */
	vm_range = parent_cinfo_l->vas_frontier - child_info->booter_vaddr;
	addr = (vaddr_t) cos_page_bump_allocn(parent_cinfo_l, vm_range);
	assert(addr);

	memcpy((void *)addr, (void *)child_info->booter_vaddr, vm_range);

	/*
	 *        * The id represents the COMPONENT ID that is checkpointed.
	 *               * Example: template[1] represnts the checkpoint of component 1
	 *                      */
	child_info->data_seg->addr = addr;
	child_info->data_seg->size = vm_range;

	/* printc("\tCheckpointing click component's %d data %lx (range:%04x)\n", nfid, child_info->data_seg->addr, child_info->data_seg->size); */

	/*
	 *        * switch to the booter component
	 *               */
	/* sl_thd_block(0); */
	cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
}

static void
fwp_clean_chain(struct nf_chain *chain)
{
	//iterate over all nfs
	//copy its data segment
	//how can we unroll the memory allocated afterwards - there is no memory
	struct click_info *this_ci;

	for(this_ci = chain->first_nf; this_ci != NULL; this_ci = this_ci->next) {
		memcpy((void *)this_ci->booter_vaddr, (void *)(this_ci->data_seg->addr), 
		       this_ci->data_seg->size);
		printc("nfid: %d\n", this_ci->nf_id);
	}
}

u32_t
nf_entry(word_t *ret2, word_t *ret3, int op, word_t arg3, word_t arg4)
{
	u32_t ret1 = 0;
	u32_t error = (1 << 16) - 1;
	void *token = (void *)cos_inv_token();

	switch(op) {
	case NF_MALLOC:
	{
		vaddr_t vaddr;
		vaddr = fwp_malloc((struct click_info *)token, arg3);
		*ret2 = (u32_t)vaddr;

		break;
	}
	case NF_CONFIDX_GET:
	{
		int conf_file_idx;
		conf_file_idx = fwp_confidx_get(token);
		*ret2 = (u32_t)conf_file_idx;

		break;
	}
	case NF_SHMEM_GET:
	{
		vaddr_t shmem_addr;
		shmem_addr = fwp_shmem_get(token);
		*ret2 = (u32_t)shmem_addr;

		break;
	}
	case NF_CHECKPOINT:
	{
		fwp_checkpoint(token, arg3);

		break;
	}
	case NF_CLEAN:
	{
		/* fwp_clean_chain(&chains[0]); */

		break;
	}
	case NF_BLOCK:
	{
		sl_thd_block(0);
		/* sl_thd_yield(0); */
		break;
	}
	default:
	{
		assert(0);

		ret1 = error;
	}
	}

	return ret1;
}
#endif /*FWP_MANAGER_H*/
