#include <cobj_format.h>
#include <sl.h>

#include "fwp_manager.h"
#include "fwp_chain_cache.h"
#include "eos_ring.h"
#include "eos_utils.h"
#include "eos_mca.h"
#include "eos_sched.h"
#include "ninf.h"

extern volatile int init_core_done;
/* Assembly function for sinv from new component */
extern word_t nf_entry_rets_inv(invtoken_t cur, int op, word_t arg1, word_t arg2, word_t *ret2, word_t *ret3);

/* 
 * IDs one and two are given to the booter 
 * component and the first loaded component
 */
long next_nfid = 2, next_chain_id = 0, next_shmem_id = 0, next_template_id = 0;

vaddr_t shmem_addr, heap_ptr;
volatile unsigned long long start, end;

struct mem_seg *t_seg, *d_seg;
unsigned long cinfo_offset;
vaddr_t s_addr, shmem_inv_addr;
struct mem_seg templates[EOS_MAX_NF_TYPE_NUM];
static struct nf_chain chains[EOS_MAX_CHAIN_NUM];
struct click_info chld_infos[EOS_MAX_NF_NUM];

volatile unsigned long long activate_cycles[MAX_NUM_THREADS];

#define COMP_CAP_SLOTS 100
static unsigned int idx = 0;
static compcap_t ckcc_slots[COMP_CAP_SLOTS];

static void
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
    
	cos_thd_mod(parent_cinfo_l, tc, (void *)dst_seg); 
}

static int
fwp_ci_get(struct cobj_header *h, vaddr_t *comp_info)
{
	int i = 0;

	for (i = 0; i < (int)h->nsymb; i++) {
		struct cobj_symb *symb;

		symb = cobj_symb_get(h, i);
		assert(symb);

		if (symb->type == COBJ_SYMB_COMP_INFO) {
			*comp_info = symb->vaddr;
			return 0;
		}

	}
	return 1;
}

/*
 * COS internals for creating a new component
 */
/* 
 * assumption and complexity of forking.
 * 1. To make copying from template easier, we try to keep post-init data segment of a NF (data segment + dynamic allocation) 
 * continuous in fwp_mgr's space. This requires the data segment allocation (see _alias_click) to be the last virtual memory
 * allocation in fwp_mgr before it switches to the NF for check-pointing. 
 * 2. The shared pkt memory complex the memory layout of a NF, which becomes when do check point:
 * | text | data | ... shared memory ... | TLS | dynamic memory (heap) |
 * text and shmem can be aliased, while others should be copied, which are not continuous in NF space.
 * Thus, we need to map them into forked NF separately (see heap_sz check in _alias_click).
 * 3. TLS page should be handled specially. As the template already includes the TLS page, we do not need to allocate 
 * it again (see the check in _fwp_fork_cont). Mapping TLS also need expand NF's page table.
 * 4. heap pointer of the forked NF should be updated to the post-init state.
 * 5. Some global memory of forked NF also need to be updated properly (mainly for shared ring address).
 * 6. if we do not map shmem into template, and use post-iint template to calculate shmem addr, might simplify some problems.
 */
static void
_fwp_fork(struct cos_compinfo *parent_cinfo_l, struct click_info *fork_info, 
	  struct mem_seg *text_seg, struct mem_seg *data_seg, 
	  struct mem_seg *ring_seg, int conf_file_idx, vaddr_t start_addr)
{
	struct cos_aep_info *fork_aep = cos_sched_aep_get(&fork_info->def_cinfo);
	struct cos_compinfo *fork_cinfo = cos_compinfo_get(&fork_info->def_cinfo);
	pgtblcap_t ckpt;
	captblcap_t ckct;
	vaddr_t dest, addr;
	unsigned long size;

	//printc("forking new click component\n");
	ckct = cos_captbl_alloc_at(parent_cinfo_l, BOOT_CAPTBL_FIX_CAPTBL);
	assert(ckct == BOOT_CAPTBL_FIX_CAPTBL);

	ckpt = cos_pgtbl_alloc(parent_cinfo_l);
	assert(ckpt);

	cos_compinfo_init(fork_cinfo, ckpt, ckct, 0, heap_ptr,
			  BOOT_CAPTBL_FREE, parent_cinfo_l);

	if (!cos_pgtbl_intern_alloc(parent_cinfo_l, ckpt, start_addr, text_seg->size + data_seg->size)) BUG();
	addr = round_to_pgd_page(ring_seg->addr);
	size = ring_seg->size + (ring_seg->addr - addr);
	if (!cos_pgtbl_intern_alloc(parent_cinfo_l, ckpt, addr, size)) BUG();

	for (dest = 0; dest < ring_seg->size; dest += PAGE_SIZE) {
		cos_mem_alias_at(fork_cinfo, (ring_seg->addr + dest), parent_cinfo_l, (ring_seg->addr + dest));
	}

	/* initialize rings*/
	if (fork_info->nd_ring) eos_rings_init((void *)ring_seg->addr);
	fork_info->conf_file_idx = conf_file_idx;
}

static vaddr_t
_alias_click(struct cos_compinfo *parent_cinfo, struct cos_compinfo *child_cinfo, 
	     struct mem_seg *text_seg, struct mem_seg *data_seg, vaddr_t start_addr)
{
	unsigned long offset, text_offset;
	vaddr_t allocated_data_seg;
	int data_sz, heap_sz;

	/*
	 * Alias the text segment in the new component
	 */
	for (offset = 0; offset < text_seg->size; offset += PAGE_SIZE) {
		cos_mem_alias_at(child_cinfo, start_addr + offset, 
				 parent_cinfo, text_seg->addr + offset);
	}
	text_offset = offset;

	/*
	 * allocate mem for the data segment and populate it
	 */ 
	allocated_data_seg = (vaddr_t) cos_page_bump_allocn(parent_cinfo, data_seg->size);
	assert(allocated_data_seg);
	memcpy((void *)allocated_data_seg, (void *) data_seg->addr, data_seg->size);

	/*
	 * Alias the data segment in the new component
	 */
	data_sz = d_seg->size;
	heap_sz = data_seg->size - data_sz;
	for (offset = 0; offset < data_sz; offset += PAGE_SIZE) {
		cos_mem_alias_at(child_cinfo, start_addr + text_offset + offset,
				 parent_cinfo, allocated_data_seg + offset);
	}
	if (heap_sz > 0) {
		if (!cos_pgtbl_intern_alloc(parent_cinfo,  child_cinfo->pgtbl_cap, heap_ptr, heap_sz)) BUG();
		for(; offset < data_seg->size; offset += PAGE_SIZE) {
			cos_mem_alias_at(child_cinfo, heap_ptr + offset - data_sz,
					 parent_cinfo, allocated_data_seg + offset);
		}
		child_cinfo->vas_frontier = heap_ptr + offset - data_sz;
	}
	/* printc("dbg click data end addr %x data sz %d hep %d\n", start_addr + text_offset+offset, data_sz, heap_sz); */

	return allocated_data_seg;
}

static void
copy_caps(struct cos_compinfo *parent_cinfo_l, struct cos_compinfo *fork_cinfo, 
	  captblcap_t ckct, pgtblcap_t  ckpt, compcap_t ckcc, 
	  sinvcap_t sinv, thdcap_t initthd_cap)
{
	int ret;

	/* printc("\tCopying pgtbl, captbl, component capabilities\n"); */
	ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_CT, parent_cinfo_l, ckct);
	assert(ret == 0);

	ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_PT, parent_cinfo_l, ckpt);
	assert(ret == 0);

	ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_COMP, parent_cinfo_l, ckcc);
	assert(ret == 0);

	ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_HYP_SINV_CAP, parent_cinfo_l, sinv);
	assert(ret == 0);

	ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_INITHW_BASE,
			     parent_cinfo_l, BOOT_CAPTBL_SELF_INITHW_BASE);
	assert(ret == 0);

	ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_INITTCAP_BASE,
			     parent_cinfo_l, BOOT_CAPTBL_SELF_INITTCAP_BASE);
	assert(ret == 0);

	if (initthd_cap) {
		ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_INITTHD_BASE,
				     parent_cinfo_l, initthd_cap);
		assert(ret == 0);
	}
}

static void
_fwp_fork_cont(struct cos_compinfo *parent_cinfo, struct click_info *chld_info,
	       vaddr_t allocated_data_seg, unsigned long comp_info_offset, int coreid)
{
	struct cos_compinfo *child_cinfo = cos_compinfo_get(&chld_info->def_cinfo);
	struct cos_aep_info *child_aep = cos_sched_aep_get(&chld_info->def_cinfo);
	struct cos_component_information *ci;
	compcap_t ckcc;
	captblcap_t ckct;
	pgtblcap_t  ckpt;
	sinvcap_t sinv;
	int r;

	chld_info->booter_vaddr = allocated_data_seg;

	ci = (void *) allocated_data_seg + comp_info_offset;
	ci->cos_this_spd_id = chld_info->nf_id;

	ckct = child_cinfo->captbl_cap;
	ckpt = child_cinfo->pgtbl_cap;

       if (idx < COMP_CAP_SLOTS){
              ckcc = cos_comp_alloc(parent_cinfo, ckct, ckpt,
                                          (vaddr_t) ci->cos_upcall_entry);
              assert(ckcc);
              ckcc_slots[idx] = ckcc;
       } else {
	       cos_cap_drop(parent_cinfo, ckcc_slots[idx % COMP_CAP_SLOTS]);
              ckcc = cos_comp_alloc_at(parent_cinfo, ckct, ckpt, (vaddr_t) ci->cos_upcall_entry, 
                                          ckcc_slots[idx % COMP_CAP_SLOTS]);
              assert(ckcc == ckcc_slots[idx % COMP_CAP_SLOTS]);
       } 
       *(volatile compcap_t *)(&(child_cinfo->comp_cap)) = ckcc;
       idx++;
       ps_cc_barrier();

	if (chld_info->nd_thd || chld_info->conf_file_idx != -1) {
		r = eos_initaep_alloc(coreid, &chld_info->def_cinfo, NULL, 0, 0, 0, &(chld_info->initaep));
		assert(!r);
		if (!coreid) {
			eos_thd_param_set(coreid, &(chld_info->initaep), sched_param_pack(SCHEDP_PRIO, LOWEST_PRIORITY));
			_alloc_tls(parent_cinfo, child_cinfo, child_aep->thd, PAGE_SIZE);
		} else {
			eos_thd_param_set(coreid, &(chld_info->initaep), sched_param_pack(SCHEDP_PRIO_BLOCK, LOWEST_PRIORITY));
			r = eos_thd_set_tls(coreid, parent_cinfo, &(chld_info->def_cinfo.sched_aep[coreid].thd), (void *)heap_ptr);
			assert(!r);
		}
	} else {
		chld_info->initaep = NULL;
	}

	/* Create sinv capability from Userspace to Booter components */
	sinv = cos_sinv_alloc_at(parent_cinfo, parent_cinfo->comp_cap, (vaddr_t)nf_entry_rets_inv, (vaddr_t)chld_info, BOOT_CAPTBL_FIX_SINV);
	assert(sinv == BOOT_CAPTBL_FIX_SINV);

	if (chld_info->initaep) {
		copy_caps(parent_cinfo, child_cinfo, ckct, ckpt, ckcc, sinv, sl_thd_thdcap(chld_info->initaep));
	} else {
		copy_caps(parent_cinfo, child_cinfo, ckct, ckpt, ckcc, sinv, 0);
	}

	//if (chld_info->conf_file_idx != -1) cos_thd_switch(sl_thd_thdcap(chld_info->initaep));
       //while (!ps_load(&(chld_info->initaep))){}
}

static void
_fwp_for_drop_cap(struct cos_compinfo *parent_cinfo)
{
	cos_cap_drop(parent_cinfo, BOOT_CAPTBL_FIX_SINV);
	cos_cap_drop(parent_cinfo, BOOT_CAPTBL_FIX_CAPTBL);
}

/*
 * fork a new click component using the configuration file at *conf_str
 */
static void 
fwp_fork(struct click_info *chld_info, struct mem_seg *text_seg, struct mem_seg *data_seg,
	 struct mem_seg *ring_seg, int conf_file_idx, unsigned long cinfo_offset, vaddr_t start_addr, int coreid)
{
	struct cos_compinfo *parent_cinfo = cos_compinfo_get(cos_defcompinfo_curr_get());
	struct cos_compinfo *child_cinfo = cos_compinfo_get(&chld_info->def_cinfo);
	vaddr_t allocated_data_seg = NULL;
      
	assert(text_seg);
	assert(data_seg);
	assert(ring_seg);
 
	_fwp_fork(parent_cinfo, chld_info, text_seg, data_seg, ring_seg, conf_file_idx, start_addr);
	allocated_data_seg = _alias_click(parent_cinfo, child_cinfo, text_seg, data_seg, start_addr);
	_fwp_fork_cont(parent_cinfo, chld_info, allocated_data_seg, cinfo_offset, coreid);
	_fwp_for_drop_cap(parent_cinfo);
}

void
fwp_chain_activate(struct nf_chain *chain)
{
	struct eos_ring *out1, *in2;
	struct click_info *this_nf, *new_nf;
	struct mca_conn *conn;
	(void)conn;

	list_for_each_nf(this_nf, chain) {
		new_nf = this_nf->next;
		if (!new_nf || !new_nf->nd_ring) continue;
		out1 = get_output_ring((void *)this_nf->shmem_addr);
		in2  = get_input_ring((void *)new_nf->shmem_addr);
		conn = mca_conn_create(out1, in2);
	}
	/* TODO: activate threads */
	list_for_each_nf(this_nf, chain) {
		if (this_nf->nd_thd) {
			assert(this_nf->initaep);
			assert(this_nf->nd_ring);
			in2         = get_input_ring((void *)this_nf->shmem_addr);
			in2->coreid = this_nf->core_id;
			in2->thdid  = sl_thd_thdid(this_nf->initaep);
                     start = ps_tsc(); 
			eos_thd_wakeup(this_nf->core_id, sl_thd_thdid(this_nf->initaep));
		}
	}
}

static void *
fwp_get_shmem(int shmem_idx)
{
	char *seg;
	assert(shmem_idx < EOS_MAX_MEMSEGS_NUM);
	seg = (char *)shmem_addr + shmem_idx * FWP_MEMSEG_SIZE;
	return (void *)seg;
}

static void
fwp_allocate_shmem(vaddr_t start_addr)
{
	vaddr_t empty_page, next_pgd, map_shmem_at, addr;
	struct cos_compinfo *boot_cinfo = cos_compinfo_get(cos_defcompinfo_curr_get());

	/*FIXME An ugly hack*/
	/*
	 * We assume that the binary mapped by the llbooter goes in an PGD(4MB)
	 * amd we map the SHMEM after this PGD
	 */
	next_pgd = round_up_to_pgd_page(start_addr + 1);
	map_shmem_at = ((vaddr_t)cos_get_heap_ptr() > next_pgd) ? (vaddr_t)cos_get_heap_ptr() : next_pgd;
	empty_page = (vaddr_t)cos_page_bump_alloc(boot_cinfo);
	while (addr < map_shmem_at)
		addr = cos_mem_alias(boot_cinfo, boot_cinfo, empty_page);

	shmem_addr = (vaddr_t)cos_page_bump_allocn(boot_cinfo, EOS_MAX_MEMSEGS_NUM * FWP_MEMSEG_SIZE);
	heap_ptr = round_up_to_pgd_page(shmem_addr + EOS_MAX_MEMSEGS_NUM * FWP_MEMSEG_SIZE);
	printc("dbg empty %x shmem addr %lx\n", empty_page, shmem_addr);
}

void
fwp_allocate_shmem_sinv(struct cos_compinfo *src_comp, compcap_t dest_compcap)
{
	struct cos_compinfo *boot_cinfo = cos_compinfo_get(cos_defcompinfo_curr_get());
	sinvcap_t next_call_sinvcap;
	int ret;

	/*allocate the sinv capability for next_call*/
	next_call_sinvcap = cos_sinv_alloc_at(boot_cinfo, dest_compcap, shmem_inv_addr, 0, BOOT_CAPTBL_FIX_SINV);
	assert(next_call_sinvcap == BOOT_CAPTBL_FIX_SINV);
	ret = cos_cap_move_at(src_comp, BOOT_CAPTBL_NEXT_SINV_CAP,  boot_cinfo, next_call_sinvcap);
	assert(ret == 0);
}

/*
 * actually allocate the chain NFs
 */
static struct nf_chain *
fwp_allocate_chain(struct nf_chain *chain, int is_template, int coreid)
{
	struct click_info *this_nf, *new_nf, **last_nf;
	int nfid, ncid, shmemid, last_shmem_id;
	struct nf_chain *ret_chain;
	struct mem_seg *nf_data_seg;

	if (!is_template) {
		ncid = next_chain_id++;
		assert(ncid < EOS_MAX_CHAIN_NUM);
		ret_chain = &chains[ncid];
		*ret_chain = *chain;
		ret_chain->chain_id = ncid;
		last_nf = &(ret_chain->first_nf);

		list_for_each_nf(this_nf, chain) {
			nfid                  = next_nfid++;
			assert(nfid < EOS_MAX_NF_NUM);
			new_nf                = &chld_infos[nfid];
			*last_nf              = new_nf;
			new_nf->conf_file_idx = -1;
			new_nf->nf_id         = nfid;
			new_nf->nd_thd        = this_nf->nd_thd;
			new_nf->nd_ring       = this_nf->nd_ring;
			new_nf->nd_sinv       = this_nf->nd_sinv;
			new_nf->data_seg      = this_nf->data_seg;
			new_nf->core_id       = coreid;
			new_nf->next          = NULL;
			ret_chain->last_nf    = new_nf;
			last_nf               = &(new_nf->next);
		}
		chain = ret_chain;
	}

	list_for_each_nf(this_nf, chain) {
		struct mem_seg mem_seg;

		if (this_nf->nd_ring) shmemid = next_shmem_id++;
		else shmemid = last_shmem_id;
		assert(shmemid < EOS_MAX_NF_NUM);
		last_shmem_id = shmemid;
		this_nf->shmem_addr = (vaddr_t)fwp_get_shmem(shmemid);
		mem_seg.addr = this_nf->shmem_addr;
		mem_seg.size = FWP_MEMSEG_SIZE;
		if (1 || is_template) nf_data_seg = d_seg;
		else nf_data_seg = this_nf->data_seg;
              //start = ps_tsc();
		fwp_fork(this_nf, t_seg, nf_data_seg, &mem_seg, this_nf->conf_file_idx, cinfo_offset, s_addr, coreid);
              //end = ps_tsc();
              //printc("fork cycles: %lld\n", end - start);
	}

	if (!is_template) {
		list_for_each_nf(this_nf, chain) {
			if (this_nf->nd_sinv) {
				fwp_allocate_shmem_sinv(cos_compinfo_get(&(this_nf->def_cinfo)),
							cos_compinfo_get(&(this_nf->next->def_cinfo))->comp_cap);
			}
		}
              fwp_chain_put(chain, FWP_CHAIN_CLEANED, coreid);
	}
       return chain;
}

static void
fwp_init(void)
{
	fwp_allocate_shmem(s_addr);
}

static void
fwp_mgr_loop()
{
	while (1) {
		/* TODO: clean up chain, put them back to cache */
		/* TODO: if not enough fwp in cache, fork more */
		/* fwp_clean_chain(&chains[0]); */;
	}
}

static struct nf_chain *fwp_create_chain_bridge();
static struct nf_chain *fwp_create_chain1(void); /* create a chain of two NFs linked by MCA */
static struct nf_chain *fwp_create_chain2(int conf_file1, int conf_file2); /* create a chain of two NFs linked by shmem */

void
fwp_test(struct mem_seg *text_seg, struct mem_seg *data_seg, vaddr_t start_addr,
	 unsigned long comp_info_offset, vaddr_t sinv_next_call)
{
	struct nf_chain *chain;
	int i, j;

	t_seg = text_seg;
	d_seg = data_seg;
	cinfo_offset = comp_info_offset;
	s_addr = start_addr;
	shmem_inv_addr = sinv_next_call;

	fwp_init();
	cos_faa(&init_core_done, 1);
	while (init_core_done != NF_MIN_CORE) {
		__asm__ __volatile__("rep;nop": : :"memory");
	}


	/* chain = fwp_create_chain1(); */
	/* chain = fwp_create_chain2(3, 4); */
	chain = fwp_create_chain_bridge();
	//fwp_allocate_chain(chain, 1, 0);

int dt = 0;
       for(j=0; j<EOS_MAX_CHAIN_NUM_PER_CORE; j++) {
	       for(i=NF_MIN_CORE; i<NF_MAX_CORE; i++) {
                     dt = j*(i-NF_MIN_CORE-1);
			if (dt % 10 == 0)printc("core %d j %d tot %d cap fronteers: %lu %lu heap %x untype %x fonter %x\n", i, j, dt, CURR_CINFO()->cap_frontier, CURR_CINFO()->caprange_frontier, CURR_CINFO()->vas_frontier, CURR_CINFO()->mi.untyped_ptr, CURR_CINFO()->mi.untyped_frontier);
			//printc("core %d tot %d\n", i, dt);
			fwp_allocate_chain(chain, 0, i);
		}
	}
	printc("dbg chain alloca done\n");

       for(j=0; j<EOS_MAX_CHAIN_NUM_PER_CORE; j++) {
              for(i=NF_MIN_CORE; i<NF_MAX_CORE; i++) {
                     chain = fwp_chain_get(FWP_CHAIN_CLEANED, i);
                     fwp_chain_activate(chain);
                     while (ps_load(&start)){}
              }
       }
       for (i=0; i<MAX_NUM_THREADS; i++)
              printc("activate_cycles: %lld\n", activate_cycles[i]);
	fwp_mgr_loop();
}



/* create a chain with single bridge nf */
static struct nf_chain *
fwp_create_chain_bridge()
{
	/* the first nf_id in our system is 2*/
	int nfid, ncid, shmemid;
	struct click_info *nf1;
	struct nf_chain *ret_chain;

	ncid = next_chain_id++;
	ret_chain = &chains[ncid];
	ret_chain->chain_id = ncid;
	ret_chain->tot_nf = 1;
	ret_chain->active_nf = 0;
	ret_chain->next = NULL;

	nfid = next_nfid++;
	nf1 = &chld_infos[nfid];
	nf1->next = NULL;
	nf1->conf_file_idx = 2;
	nf1->nf_id = nfid;
	nf1->nd_thd = 1;
	nf1->nd_ring = 1;
	nf1->nd_sinv = 0;
	nf1->data_seg = &templates[next_template_id++];
	ret_chain->first_nf = ret_chain->last_nf = nf1;

	return ret_chain;
}

static struct nf_chain *
fwp_create_chain1(void)
{
	int nfid, ncid, shmemid, temid;
	struct click_info *nf1, *nf2;
	struct nf_chain *ret_chain;

	ncid = next_chain_id++;
	ret_chain = &chains[ncid];
	ret_chain->chain_id = ncid;
	ret_chain->tot_nf = 2;
	ret_chain->active_nf = 0;
	ret_chain->next = NULL;

	nfid = next_nfid++;
	nf1 = &chld_infos[nfid];
	nf1->conf_file_idx = 0;
	nf1->nf_id = nfid;
	nf1->nd_thd = 1;
	nf1->nd_ring = 1;
	nf1->nd_sinv = 0;
	nf1->data_seg = &templates[next_template_id++];

	nfid = next_nfid++;
	nf2 = &chld_infos[nfid];
	nf2->next = NULL;
	nf2->conf_file_idx = 1;
	nf2->nf_id = nfid;
	nf2->nd_thd = 1;
	nf2->nd_ring = 1;
	nf2->nd_sinv = 0;
	nf2->data_seg = &templates[next_template_id++];
	nf1->next = nf2;

	ret_chain->first_nf = nf1;
	ret_chain->last_nf = nf2;

	return ret_chain;
}

static struct nf_chain *
fwp_create_chain2(int conf_file1, int conf_file2)
{

	int nfid, ncid, shmemid;
	struct click_info *nf1, *nf2;
	struct nf_chain *ret_chain;

	ncid = next_chain_id++;
	ret_chain = &chains[ncid];
	ret_chain->chain_id = ncid;
	ret_chain->tot_nf = 2;
	ret_chain->active_nf = 0;
	ret_chain->next = NULL;

	nfid = next_nfid++;
	nf1 = &chld_infos[nfid];
	nf1->conf_file_idx = conf_file1;
	nf1->nf_id = nfid;
	nf1->nd_thd = 1;
	nf1->nd_ring = 1;
	nf1->nd_sinv = 1;
	nf1->data_seg = &templates[next_template_id++];

	nfid = next_nfid++;
	nf2 = &chld_infos[nfid];
	nf2->next = NULL;
	nf2->conf_file_idx = conf_file2;
	nf2->nf_id = nfid;
	nf2->nd_thd = 0;
	nf2->nd_ring = 0;
	nf2->nd_sinv = 0;
	nf2->data_seg = &templates[next_template_id++];
	nf1->next = nf2;

	ret_chain->first_nf = nf1;
	ret_chain->last_nf = nf2;

	return ret_chain;
}
