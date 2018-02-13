//#include <ck_ring.h>
#include "init.h"
#include "manager.h"

#define TLS_SEG_SIZE PAGE_SIZE

extern vaddr_t cos_upcall_entry;

/*
 * save the base VM address of the current chld
 * this is needed to know where to start checkpointing from
 */
vaddr_t this_chld_vm_base;

/*
 * global variables storing the current instance id and NF id
 */
static int this_instance_id = 0;
static int this_nf_id = 0;

/*
 * the ID of this click instance
 */
int click_instance_id = 0;

//pointers to ring buffers
//extern ck_ring_t *input_ring;
//extern ck_ring_t *output_ring;

/* Assembly functions for sinv*/
extern void *__inv_mng_agent(int a, int b, int c, int d);
extern void *__inv_next_call(int a, int b, int c, int d);

/*
 * capability to text and shared memory PTEs
 */
pgtblcap_t text_pgtblcap, shmem_pgtblcap;

/*
 * capability to sinv another component
 */
sinvcap_t next_call_sinv;

/*
 * pointer to the end of read-only data
 * symbol set in the linker script
 */
extern vaddr_t _erodata;

void 
_alloc_tls(struct cos_compinfo *parent_cinfo_l, struct cos_compinfo *chld_cinfo_l, thdcap_t tc, size_t tls_size) 
{
    vaddr_t tls_seg, addr, dst_pg, dst_seg;
    
    tls_seg = (vaddr_t) cos_page_bump_allocn(parent_cinfo_l, tls_size);
    assert(tls_seg);
    dst_seg = cos_mem_alias(chld_cinfo_l, parent_cinfo_l, tls_seg);
    assert(dst_seg);
    for (addr = PAGE_SIZE; addr < tls_size; addr += PAGE_SIZE) {
        dst_pg = cos_mem_alias(chld_cinfo_l, parent_cinfo_l, (tls_seg + addr));
        assert(dst_pg);
    }

    printc("parent %lx child %lx\n", tls_seg, dst_seg);

    cos_thd_mod(parent_cinfo_l, tc, (void *)dst_seg); 
}

/*
 * COS internals for creating a new component
 */
void _click_fork(struct cos_compinfo *parent_cinfo_l,
		struct chld_click_info *fork_info) {
	int ret;
	vaddr_t addr, dst_pg;
	pgtblcap_t ckpt, ckutpt;
	captblcap_t ckct;
	compcap_t ckcc;
	struct cos_compinfo *fork_cinfo = cos_compinfo_get(&fork_info->def_cinfo);
	sinvcap_t mem_alloc_sinv;

	//printc("forking new click component\n");
	ckct = cos_captbl_alloc(parent_cinfo_l);
	assert(ckct);

	ckpt = cos_pgtbl_alloc(parent_cinfo_l);
	assert(ckpt);

	ckutpt = cos_pgtbl_alloc(parent_cinfo_l);
	assert(ckutpt);

	ckcc = cos_comp_alloc(parent_cinfo_l, ckct, ckpt,
			(vaddr_t) &cos_upcall_entry);
	assert(ckcc);

	/*allocate the sinv capability for bump_alloc*/
	mem_alloc_sinv = cos_sinv_alloc(parent_cinfo_l, BOOT_CAPTBL_SELF_COMP,
			(vaddr_t) __inv_mng_agent, 0);
	assert(mem_alloc_sinv > 0);

	cos_meminfo_init(&fork_cinfo->mi, BOOT_MEM_KM_BASE, CK_UNTYPED_SIZE,
			ckutpt);
	cos_compinfo_init(fork_cinfo, ckpt, ckct, ckcc, (vaddr_t) BOOT_MEM_VM_BASE,
			BOOT_CAPTBL_FREE, parent_cinfo_l);
	cos_compinfo_init(&fork_info->click_info.shm_cinfo, ckpt, ckct, ckcc,
			(vaddr_t) CK_SHM_BASE, BOOT_CAPTBL_FREE, parent_cinfo_l);

	printc("\tCopying pgtbl, captbl, component capabilities\n");
	ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_INITHW_BASE,
			parent_cinfo_l, BOOT_CAPTBL_SELF_INITHW_BASE);
	assert(ret == 0);
	ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_INITTCAP_BASE,
			parent_cinfo_l, BOOT_CAPTBL_SELF_INITTCAP_BASE);
	assert(ret == 0);
	ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_CT, parent_cinfo_l, ckct);
	assert(ret == 0);
	ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_PT, parent_cinfo_l, ckpt);
	assert(ret == 0);
	ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_UNTYPED_PT,
			parent_cinfo_l, ckutpt);
	assert(ret == 0);
	ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_COMP, parent_cinfo_l,
			ckcc);
	assert(ret == 0);
	ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SINV_CAP, parent_cinfo_l,
			mem_alloc_sinv);
	assert(ret == 0);
}

/*
 * allocate a memory segment, map it into chld_info_l
 * and memcpy from start_ptr to end_ptr
 */
void *ck_virtmem_alloc(struct cos_compinfo *parent_cinfo_l,
		struct cos_compinfo *chld_cinfo_l, unsigned long start_ptr,
		unsigned long end_ptr) {
	vaddr_t addr, src_seg, dst_pg;
	size_t vm_range;

	vm_range = (size_t) (end_ptr - start_ptr);
	assert(vm_range > 0);

	printc("start_ptr=%lx; end_ptr=%lx; vm_range = %04x\n", start_ptr, end_ptr,
	 vm_range);

	src_seg = (vaddr_t) cos_page_bump_allocn(parent_cinfo_l, vm_range);
	assert(src_seg);

	for (addr = 0; addr < vm_range; addr += PAGE_SIZE) {

		memcpy((void *) (src_seg + addr), (void *) (start_ptr + addr),
		PAGE_SIZE);

		dst_pg = cos_mem_alias(chld_cinfo_l, parent_cinfo_l, (src_seg + addr));
		assert(dst_pg);
	}

	return src_seg;
}

/*
 * fork a new click component using the configuration file at *conf_str
 */
void click_fork_with_cfg(pgtblcap_t text_pgtblcap_l,
		pgtblcap_t shmem_pgtblcap_l, char *conf_str, void *init_fn) {
	struct cos_compinfo *parent_cinfo = cos_compinfo_get(
			cos_defcompinfo_curr_get());
	struct cos_compinfo *chld_cinfo;
	struct chld_click_info *chld_info;
	struct click_init init_data;
	int *new_comp_inst_id;
	bool *new_comp_exec_click;
	void *new_comp_vm_base;
	int inst_id_offset;
	int exec_click_offset;
	int ret = 0;

	this_instance_id++;
	init_data.conf_str = conf_str;
	init_data.nf_id = this_nf_id++;
	this_chld_vm_base = parent_cinfo->vas_frontier;
	chld_info = &chld_infos[this_instance_id];
	chld_cinfo = cos_compinfo_get(&chld_info->def_cinfo);

	_click_fork(parent_cinfo, chld_info);

	/*allocate and copy the init thread*/
	chld_info->click_info.initthd = cos_thd_alloc(parent_cinfo,
			chld_cinfo->comp_cap, init_fn, &init_data);
	assert(chld_info->click_info.initthd);
	ret = cos_cap_cpy_at(chld_cinfo, BOOT_CAPTBL_SELF_INITTHD_BASE,
			parent_cinfo, chld_info->click_info.initthd);
	assert(ret == 0);

	//expand using the Click .text PTE
	ret = cos_pgtbl_intern_expandwith(chld_cinfo, text_pgtblcap_l,
			BOOT_MEM_VM_BASE);
	assert(ret == 0);

	//expand using the shared memory PTE
	ret = cos_pgtbl_intern_expandwith(&chld_info->click_info.shm_cinfo,
			shmem_pgtblcap_l, CK_SHM_BASE);
	assert(ret == 0);

	/*
	 * Duplicate everything from round_up_to_pgd_page(_erodata) to cos_get_heap_ptr.
	 */
	new_comp_vm_base = ck_virtmem_alloc(parent_cinfo, chld_cinfo,
			round_up_to_pgd_page(_erodata), (vaddr_t) cos_get_heap_ptr());

	/*
	 * set the click_instance_id in the new component's memory
	 */
	inst_id_offset =
			(void*) &click_instance_id - round_up_to_pgd_page(_erodata);
	new_comp_inst_id = new_comp_vm_base + inst_id_offset;
	*new_comp_inst_id = this_instance_id;
    
    _alloc_tls(parent_cinfo, chld_cinfo, chld_info->click_info.initthd, TLS_SEG_SIZE);

	cos_thd_switch(chld_infos[this_instance_id].click_info.initthd);
}

/*
 * fork a new click component using checkpointed memory
 * nf_id is the ID of the component to be cloned
 */
void click_fork_with_mem(pgtblcap_t text_pgtblcap_l,
		pgtblcap_t shmem_pgtblcap_l, int nf_id) {
	struct cos_compinfo *parent_cinfo = cos_compinfo_get(
			cos_defcompinfo_curr_get());
	struct cos_compinfo *chld_cinfo;
	struct chld_click_info *chld_info;
	int ret;
	int *new_comp_inst_id;
	void *new_comp_vm_base;
	int inst_id_offset;

	this_instance_id++;
	chld_info = &chld_infos[this_instance_id];
	chld_cinfo = cos_compinfo_get(&chld_info->def_cinfo);

	_click_fork(parent_cinfo, chld_info);

	/*allocate and copy the init thread*/
	chld_info->click_info.initthd = cos_initthd_alloc(parent_cinfo,
			chld_cinfo->comp_cap);
	assert(chld_info->click_info.initthd);
	ret = cos_cap_cpy_at(chld_cinfo, BOOT_CAPTBL_SELF_INITTHD_BASE,
			parent_cinfo, chld_info->click_info.initthd);
	assert(ret == 0);

	//expand using the Click .text PTE
	ret = cos_pgtbl_intern_expandwith(cos_compinfo_get(&chld_info->def_cinfo),
			text_pgtblcap_l, BOOT_MEM_VM_BASE);
	assert(ret == 0);

	//expand using the shared memory PTE
	ret = cos_pgtbl_intern_expandwith(&chld_info->click_info.shm_cinfo,
			shmem_pgtblcap_l, CK_SHM_BASE);
	assert(ret == 0);

	new_comp_vm_base = ck_virtmem_alloc(parent_cinfo, chld_cinfo,
			templates[nf_id].addr,
			templates[nf_id].addr + templates[nf_id].size);

	/*
	 * set the click_instance_id in the new component's memory
	 */
	inst_id_offset =
			(void*) &click_instance_id - round_up_to_pgd_page(_erodata);
	new_comp_inst_id = new_comp_vm_base + inst_id_offset;
	*new_comp_inst_id = this_instance_id;
    
    _alloc_tls(parent_cinfo, chld_cinfo, chld_info->click_info.initthd, TLS_SEG_SIZE);

	cos_thd_switch(chld_infos[this_instance_id].click_info.initthd);
}

/*
 * initialization operations in the parent before forking new components
 */
static void 
parent_init(void) {
	int cycs, ret;
	vaddr_t addr, dst_pg;
	unsigned long long start, end1, end2, end;
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *parent_cinfo = cos_compinfo_get(defci);
	struct cos_compinfo shm_cinfo;

	/*calibrate timers*/
	cycs = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	printc("\t%d cycles per microsecond\n", cycs);

	/*
	 * allocate the shared memory segment
	 */
	printc("\tAllocating shared-memory (size: %lu)\n",
			(unsigned long) CK_SHM_SZ);
	cos_compinfo_init(&shm_cinfo, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT,
			BOOT_CAPTBL_SELF_COMP, (vaddr_t) CK_SHM_BASE, BOOT_CAPTBL_FREE,
			parent_cinfo);
	addr = (vaddr_t) cos_page_bump_allocn(&shm_cinfo, CK_SHM_SZ);
	assert(addr && addr == CK_SHM_BASE);

	/*
	 * fork a new component which stand as the parent for all new components
	 * this is needed just to have a pointer to text_pgtblcap
	 * this component is never scheduled
	 */
	_click_fork(parent_cinfo, &chld_infos[0]);

	/*
	 * Allocate a new PTE and fill it with mappings to the booter component's text
	 */
	text_pgtblcap = cos_pgtbl_intern_expand(
			cos_compinfo_get(&chld_infos[0].def_cinfo), BOOT_MEM_VM_BASE, 1);
	assert(text_pgtblcap);
	for (addr = BOOT_MEM_VM_BASE; addr < round_up_to_pgd_page(_erodata); addr +=
	PAGE_SIZE) {
		dst_pg = cos_mem_alias(cos_compinfo_get(&chld_infos[0].def_cinfo),
				parent_cinfo, addr);
		assert(dst_pg && dst_pg == addr);
	}

	/*
	 * construct the shared mem PTE and alias it
	 * to the allocated memory in the booter component
	 */
	shmem_pgtblcap = cos_pgtbl_intern_expand(
			&chld_infos[0].click_info.shm_cinfo,
			CK_SHM_BASE, 1);
	assert(shmem_pgtblcap);
	for (addr = CK_SHM_BASE; addr < CK_SHM_BASE + CK_SHM_SZ; addr +=
	PAGE_SIZE) {
		dst_pg = cos_mem_alias(&chld_infos[0].click_info.shm_cinfo, &shm_cinfo,
				addr);
		assert(dst_pg && dst_pg == addr);
	}

	/*TODO this should be done in the manager*/
	/*set the ring buffers and next call*/
	//input_ring = output_ring = (ck_ring_t*) CK_SHM_BASE;
	next_call = &next_call_sinv_impl;
}

/*
 * test function
 */
static void test_click() {
	struct cos_compinfo *parent_cinfo = cos_compinfo_get(
			cos_defcompinfo_curr_get());
	int ret;

	click_fork_with_cfg(text_pgtblcap, shmem_pgtblcap,
			&_binary_conf_file1_start, (void*) click_main);
	click_fork_with_cfg(text_pgtblcap, shmem_pgtblcap,
			&_binary_conf_file2_start, (void*) click_main);

	printc("out\n");

	/*allocate the sinv capability for next_call*/
	next_call_sinv = cos_sinv_alloc(parent_cinfo,
			cos_compinfo_get(&chld_infos[this_instance_id].def_cinfo)->comp_cap,
			(vaddr_t) __inv_next_call, 0);
	assert(next_call_sinv > 0);
	ret = cos_cap_cpy_at(
			cos_compinfo_get(&chld_infos[this_instance_id - 1].def_cinfo),
			NEXT_CALL_SINV_CAP, parent_cinfo, next_call_sinv);
	assert(ret == 0);

	cos_thd_switch(chld_infos[this_instance_id - 1].click_info.initthd);
}

void cos_init(void *args) {
	/*if (likely(click_instance_id)) {
		run_driver();
	} else {
		parent_init();
		test_click();
	}*/

    struct click_init init_data;

    init_data.conf_str = &_binary_conf_file1_start;
    init_data.nf_id = 0;

    printc("cos_init\n");
    click_main(&init_data);
}
