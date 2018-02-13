#include <stddef.h>
#include <sl.h>
#include "init.h"
#include "manager.h"

#define MNG_CALLS_NUM 378

extern vaddr_t this_chld_vm_base;
void* backing_data[SL_MAX_NUM_THDS];

typedef long (*mng_call_t)(struct cos_compinfo *parent_cinfo, long id, long c, long d);
mng_call_t mng_calls[MNG_CALLS_NUM];

/*
 * sinv to another click component
 */
void next_call_sinv_impl(void) {
	cos_sinv(NEXT_CALL_SINV_CAP, 0, 0, 0, 0);
}

/*
 * core function for checkpointing a Click component
 */
static void mng_checkpoint(struct cos_compinfo *parent_cinfo_l, int id) {
	vaddr_t src_seg, addr;
	size_t vm_range;

	/*
	 * duplicate memory from this_chld_vm_base to vas_frontier
	 */
	vm_range = parent_cinfo_l->vas_frontier - this_chld_vm_base;
	addr = (vaddr_t) cos_page_bump_allocn(parent_cinfo_l, vm_range);
	assert(addr);

	memcpy(addr, this_chld_vm_base, vm_range);

    /*
     * The id represents the COMPONENT ID that is checkpointed. 
     * Example: template[1] represnts the checkpoint of component 1
     */
	templates[id].addr = addr;
	templates[id].size = vm_range;

	/*printc("\tCheckpointing click component's %d data %lx (range:%lx)\n", id,
			templates[id].addr, templates[id].size);*/

	/*
	 * switch to the booter component
	 */
	cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
}

/*
 * core function to allocate memory on behalf of a child component
 */
void *mng_mem_alloc(struct cos_compinfo *parent_cinfo_l, int id, size_t size) {
	vaddr_t addr, src_seg, dst_pg, i;
	struct cos_compinfo *chld_cinfo = cos_compinfo_get(
			&chld_infos[id].def_cinfo);

	src_seg = (vaddr_t) cos_page_bump_allocn(parent_cinfo_l, size);
	assert(src_seg);
	addr = cos_mem_alias(chld_cinfo, parent_cinfo_l, src_seg);
	assert(addr);

	for (i = PAGE_SIZE; i < size; i += PAGE_SIZE) {

		dst_pg = cos_mem_alias(chld_cinfo, parent_cinfo_l, (src_seg + i));
		assert(dst_pg);
	}

	return (void*)addr;
}

void mng_setup_thread_area(struct cos_compinfo *parent_cinfo_l, long id, struct sl_thd *thread, void* data) {
    thdid_t thdid = thread->thdid;

    backing_data[thdid] = data;

    cos_thd_mod(parent_cinfo_l, sl_thd_thdcap(thread), &backing_data[thdid]);
}

/*
 * entry point of all operations executed by the manager
 * on behalf of child components
 */
void *mng_agent(long call_num, long id, long c, long d) {
	vaddr_t addr, src_seg, dst_pg, i;
    printc("mng_agent\n");

	struct cos_compinfo *parent_cinfo = cos_compinfo_get(
			cos_defcompinfo_curr_get());

	return mng_calls[call_num]((long)parent_cinfo, id, c, d);
}

CCTOR static void mng_init(void) {
	mng_calls[0] = mng_mem_alloc;
	mng_calls[1] = mng_checkpoint;
	mng_calls[2] = mng_setup_thread_area;
}
