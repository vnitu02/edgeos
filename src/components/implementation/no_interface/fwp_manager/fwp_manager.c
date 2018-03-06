#include <cobj_format.h>

#include "fwp_manager.h"

/* Assembly function for sinv from new component */
extern void *llboot_entry_inv(int a, int b, int c);

extern vaddr_t cos_upcall_entry;

/* 
 * IDs one and two are given to the booter 
 * component and the first loaded component
 */
long next_nfid = 2;

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
static thdcap_t
_fwp_fork(struct cos_compinfo *parent_cinfo_l, struct click_info *fork_info, vaddr_t vm_base)
{
       struct cos_aep_info *fork_aep = cos_sched_aep_get(&fork_info->def_cinfo);
       struct cos_compinfo *fork_cinfo = cos_compinfo_get(&fork_info->def_cinfo);

       pgtblcap_t ckpt;
       captblcap_t ckct;
       compcap_t ckcc;
       sinvcap_t sinv;
       int ret;

       //printc("forking new click component\n");
       ckct = cos_captbl_alloc(parent_cinfo_l);
       assert(ckct);

       ckpt = cos_pgtbl_alloc(parent_cinfo_l);
       assert(ckpt);

       ckcc = cos_comp_alloc(parent_cinfo_l, ckct, ckpt,
                     (vaddr_t) &cos_upcall_entry);
       assert(ckcc);

       cos_compinfo_init(fork_cinfo, ckpt, ckct, ckcc, vm_base,
                            BOOT_CAPTBL_FREE, parent_cinfo_l);

       fork_aep->thd = cos_initthd_alloc(parent_cinfo_l, fork_cinfo->comp_cap);
       assert(fork_aep->thd);
       _alloc_tls(parent_cinfo_l, fork_cinfo, fork_aep->thd, PAGE_SIZE);

       /*allocate the sinv capability for bump_alloc*/
       sinv = cos_sinv_alloc(parent_cinfo_l, parent_cinfo_l->comp_cap, 
                                   (vaddr_t)llboot_entry_inv, (vaddr_t)fork_info);
       assert(sinv > 0);

       printc("\tCopying pgtbl, captbl, component capabilities\n");
       ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_INITHW_BASE,
                            parent_cinfo_l, BOOT_CAPTBL_SELF_INITHW_BASE);
       assert(ret == 0);

       ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_CT, parent_cinfo_l, ckct);
       assert(ret == 0);

       ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_INITTCAP_BASE,
                            parent_cinfo_l, BOOT_CAPTBL_SELF_INITTCAP_BASE);
       assert(ret == 0);

       ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_INITTHD_BASE,
                     parent_cinfo_l, fork_aep->thd);
       assert(ret == 0);

       ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_PT, parent_cinfo_l, ckpt);
       assert(ret == 0);

       ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_COMP, parent_cinfo_l,
                            ckcc);
       assert(ret == 0);

       return fork_aep->thd;
}

/*
 * fork a new click component using the configuration file at *conf_str
 */
void 
fwp_fork(struct mem_seg *text_seg, struct mem_seg *data_seg, vaddr_t start_addr, unsigned long comp_info_offset)
{
       struct cos_compinfo *parent_cinfo = cos_compinfo_get(cos_defcompinfo_curr_get());
       struct cos_compinfo *child_cinfo = cos_compinfo_get(&chld_infos[next_nfid].def_cinfo);
       vaddr_t dest;
       unsigned long offset;
       struct cos_component_information *ci;
       vaddr_t allocated_data_seg;
       thdcap_t initthd; 
       
       initthd = _fwp_fork(parent_cinfo, &chld_infos[next_nfid], start_addr);

       /*
       * Alias the text segment in the new component
       */ 
       for (offset = 0; offset < text_seg->size; offset += PAGE_SIZE) {
              dest = cos_mem_alias(child_cinfo, parent_cinfo, text_seg->addr + offset);
              assert(dest);
       }
       
       /*
       * allocate mem for the data segment and populate it
       */ 
       allocated_data_seg = (vaddr_t) cos_page_bump_allocn(parent_cinfo, data_seg->size);
       assert(allocated_data_seg);
       memcpy((void *)allocated_data_seg, (void *) data_seg->addr, data_seg->size);

       /*
       * Alias the data segment in the new component
       */
       for (offset = 0; offset < data_seg->size; offset += PAGE_SIZE) {
              dest = cos_mem_alias(child_cinfo, parent_cinfo, allocated_data_seg + offset);
              assert(dest);
       }
      
       ci = (void *) allocated_data_seg + comp_info_offset;
       ci->cos_this_spd_id = next_nfid++;

       cos_thd_switch(initthd);
}
