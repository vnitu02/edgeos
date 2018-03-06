#include <cobj_format.h>
#include <sl.h>

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
static void
_fwp_fork(struct cos_compinfo *parent_cinfo_l, struct click_info *fork_info, vaddr_t vm_base)
{
       struct cos_aep_info *fork_aep = cos_sched_aep_get(&fork_info->def_cinfo);
       struct cos_compinfo *fork_cinfo = cos_compinfo_get(&fork_info->def_cinfo);

       pgtblcap_t ckpt;
       captblcap_t ckct;

       //printc("forking new click component\n");
       ckct = cos_captbl_alloc(parent_cinfo_l);
       assert(ckct);

       ckpt = cos_pgtbl_alloc(parent_cinfo_l);
       assert(ckpt);

       cos_compinfo_init(fork_cinfo, ckpt, ckct, 0, vm_base,
                            BOOT_CAPTBL_FREE, parent_cinfo_l);

}

static vaddr_t
_alias_click(struct cos_compinfo *parent_cinfo, struct cos_compinfo *child_cinfo, 
              struct mem_seg *text_seg, struct mem_seg *data_seg, vaddr_t start_addr)
{
       unsigned long offset;
       vaddr_t allocated_data_seg;
       vaddr_t dest;

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
       
       return allocated_data_seg;
}

static void
copy_caps(struct cos_compinfo *parent_cinfo_l, struct cos_compinfo *fork_cinfo, 
              captblcap_t ckct, pgtblcap_t  ckpt, compcap_t ckcc, 
              sinvcap_t sinv, thdcap_t initthd_cap)
{
       int ret;

       printc("\tCopying pgtbl, captbl, component capabilities\n");
       ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_CT, parent_cinfo_l, ckct);
       assert(ret == 0);

       ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_PT, parent_cinfo_l, ckpt);
       assert(ret == 0);

       ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_COMP, parent_cinfo_l,
                            ckcc);
       assert(ret == 0);

       ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SINV_CAP, parent_cinfo_l, sinv);
       assert(ret == 0);

       ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_INITHW_BASE,
                            parent_cinfo_l, BOOT_CAPTBL_SELF_INITHW_BASE);
       assert(ret == 0);

       ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_INITTCAP_BASE,
                            parent_cinfo_l, BOOT_CAPTBL_SELF_INITTCAP_BASE);
       assert(ret == 0);

       ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_SELF_INITTHD_BASE,
                            parent_cinfo_l, initthd_cap);
       assert(ret == 0);

}

static void
_fwp_fork_cont(struct cos_compinfo *parent_cinfo, struct click_info *chld_info, 
                     vaddr_t allocated_data_seg, unsigned long comp_info_offset)
{
       struct cos_compinfo *child_cinfo = cos_compinfo_get(&chld_info->def_cinfo);
       struct cos_aep_info *child_aep = cos_sched_aep_get(&chld_info->def_cinfo);
       struct cos_component_information *ci;
       compcap_t ckcc;
       captblcap_t ckct;
       pgtblcap_t  ckpt;
       sinvcap_t sinv;

       ci = (void *) allocated_data_seg + comp_info_offset;
       ci->cos_this_spd_id = next_nfid;

       ckct = child_cinfo->captbl_cap;
       ckpt = child_cinfo->pgtbl_cap;
       ckcc = cos_comp_alloc(parent_cinfo, ckct, ckpt,
                                   (vaddr_t) ci->cos_upcall_entry);
       assert(ckcc);
       child_cinfo->comp_cap = ckcc;

       child_aep->thd = cos_initthd_alloc(parent_cinfo, child_cinfo->comp_cap);
       assert(child_aep->thd);
       _alloc_tls(parent_cinfo, child_cinfo, child_aep->thd, PAGE_SIZE);

       chld_info->initaep = sl_thd_comp_init(&(chld_info->def_cinfo), 0);
       assert(chld_info->initaep);

       /* Create sinv capability from Userspace to Booter components */
       sinv = cos_sinv_alloc(parent_cinfo, parent_cinfo->comp_cap, (vaddr_t)llboot_entry_inv, (vaddr_t)chld_info);
       assert(sinv > 0);

       copy_caps(parent_cinfo, child_cinfo, ckct, ckpt, ckcc, sinv, sl_thd_thdcap(chld_info->initaep));

       next_nfid++;
       cos_thd_switch(sl_thd_thdcap(chld_info->initaep));
}

/*
 * fork a new click component using the configuration file at *conf_str
 */
void 
fwp_fork(struct mem_seg *text_seg, struct mem_seg *data_seg, vaddr_t start_addr, unsigned long comp_info_offset)
{
       struct cos_compinfo *parent_cinfo = cos_compinfo_get(cos_defcompinfo_curr_get());
       struct cos_compinfo *child_cinfo = cos_compinfo_get(&chld_infos[next_nfid].def_cinfo);
       vaddr_t allocated_data_seg;
       
       _fwp_fork(parent_cinfo, &chld_infos[next_nfid], start_addr);

       allocated_data_seg = _alias_click(parent_cinfo, child_cinfo, text_seg, data_seg, start_addr); 

       _fwp_fork_cont(parent_cinfo, &chld_infos[next_nfid], allocated_data_seg, comp_info_offset);
}
