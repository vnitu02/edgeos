#include <cobj_format.h>
#include <sl.h>

#include "fwp_manager.h"
#include "eos_ring.h"
#include "eos_mca.h"

/* Assembly function for sinv from new component */
extern word_t nf_entry_rets_inv(invtoken_t cur, int op, word_t arg1, word_t arg2, word_t *ret2, word_t *ret3);

unsigned long cinfo_offset; /* The ofset of the cos_component_information in the data segment*/
vaddr_t s_addr; /* The address where the .text segment should be mapped */

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
_fwp_fork(struct cos_compinfo *parent_cinfo_l, struct click_info *fork_info, 
              struct mem_seg *ring_seg, vaddr_t vm_base, int conf_file_idx)
{
       struct cos_aep_info *fork_aep = cos_sched_aep_get(&fork_info->def_cinfo);
       struct cos_compinfo *fork_cinfo = cos_compinfo_get(&fork_info->def_cinfo);
       pgtblcap_t ckpt;
       captblcap_t ckct;
       vaddr_t dest;

       //printc("forking new click component\n");
       ckct = cos_captbl_alloc(parent_cinfo_l);
       assert(ckct);

       ckpt = cos_pgtbl_alloc(parent_cinfo_l);
       assert(ckpt);

       cos_compinfo_init(fork_cinfo, ckpt, ckct, 0, vm_base,
                            BOOT_CAPTBL_FREE, parent_cinfo_l);

       if (!cos_pgtbl_intern_alloc(parent_cinfo_l, ckpt, ring_seg->map_at, ring_seg->size)) BUG();
       for (dest = 0; dest < ring_seg->size; dest += PAGE_SIZE) {
             cos_mem_alias_at(fork_cinfo, (ring_seg->map_at + dest), parent_cinfo_l, (ring_seg->addr + dest));
       }

       /* initialize rings*/
       eos_rings_init((void *)ring_seg->addr, (void *)ring_seg->map_at);

       fork_info->conf_file_idx = conf_file_idx;
       fork_info->nf_id = next_nfid;
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

       ret = cos_cap_cpy_at(fork_cinfo, BOOT_CAPTBL_HYP_SINV_CAP, parent_cinfo_l, sinv);
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

       chld_info->booter_vaddr = allocated_data_seg;

       ci = (void *) allocated_data_seg + comp_info_offset;
       ci->cos_this_spd_id = next_nfid;

       ckct = child_cinfo->captbl_cap;
       ckpt = child_cinfo->pgtbl_cap;
       ckcc = cos_comp_alloc(parent_cinfo, ckct, ckpt,
                                   (vaddr_t) ci->cos_upcall_entry);
       assert(ckcc);
       child_cinfo->comp_cap = ckcc;

       chld_info->initaep = sl_thd_initaep_alloc(&chld_info->def_cinfo, NULL, 0, 0, 0);
       assert(chld_info->initaep);
       sl_thd_param_set(chld_info->initaep, sched_param_pack(SCHEDP_PRIO, LOWEST_PRIORITY));

       _alloc_tls(parent_cinfo, child_cinfo, child_aep->thd, PAGE_SIZE);

       /* Create sinv capability from Userspace to Booter components */
       sinv = cos_sinv_alloc(parent_cinfo, parent_cinfo->comp_cap, (vaddr_t)nf_entry_rets_inv, (vaddr_t)chld_info);
       assert(sinv > 0);

       copy_caps(parent_cinfo, child_cinfo, ckct, ckpt, ckcc, sinv, sl_thd_thdcap(chld_info->initaep));

       cos_thd_switch(sl_thd_thdcap(chld_info->initaep));
}

/*
 * fork a new click component using the configuration file at *conf_str
 */
static void 
fwp_fork(struct click_info *chld_info, struct mem_seg *text_seg, struct mem_seg *data_seg, struct mem_seg *ring_seg, int conf_file_idx)
{
       struct cos_compinfo *parent_cinfo = cos_compinfo_get(cos_defcompinfo_curr_get());
       struct cos_compinfo *child_cinfo = cos_compinfo_get(&chld_info->def_cinfo);
       vaddr_t allocated_data_seg;
      
       assert(text_seg);
       assert(data_seg);
       assert(ring_seg);
 
       _fwp_fork(parent_cinfo, chld_info, ring_seg, s_addr, conf_file_idx);

       allocated_data_seg = _alias_click(parent_cinfo, child_cinfo, text_seg, data_seg, s_addr); 

       _fwp_fork_cont(parent_cinfo, chld_info, allocated_data_seg, cinfo_offset);
}

void
fwp_test(struct mem_seg *text_seg, struct mem_seg *data_seg, vaddr_t start_addr, 
              unsigned long comp_info_offset, vaddr_t sinv_next_call)
{
       struct cos_compinfo *boot_cinfo = cos_compinfo_get(cos_defcompinfo_curr_get());
       struct mem_seg mem1, mem2;
       struct mca_conn *conn;
       struct eos_ring *out1, *in2;
       vaddr_t dest;
       int ret;
       sinvcap_t next_call_sinvcap;

       cinfo_offset = comp_info_offset;
       s_addr = start_addr;

       mca_init(boot_cinfo);
       mca_thd = sl_thd_alloc(mca_run, NULL);
       sl_thd_param_set(mca_thd, sched_param_pack(SCHEDP_PRIO, LOWEST_PRIORITY));

       mem1.addr = (vaddr_t) cos_page_bump_allocn(boot_cinfo, DEFAULT_SHMEM_SIZE);
       mem1.size = DEFAULT_SHMEM_SIZE;
       mem1.map_at = DEFAULT_SHMEM_ADDR1;
       if (!cos_pgtbl_intern_alloc(boot_cinfo, BOOT_CAPTBL_SELF_PT, mem1.map_at, mem1.size)) BUG();
       for (dest = 0; dest < mem1.size; dest += PAGE_SIZE) {
             cos_mem_alias_at(boot_cinfo, (mem1.map_at + dest), boot_cinfo, (mem1.addr + dest));
       }

       mem2.addr = (vaddr_t) cos_page_bump_allocn(boot_cinfo, DEFAULT_SHMEM_SIZE);
       mem2.size = DEFAULT_SHMEM_SIZE;
       mem2.map_at = DEFAULT_SHMEM_ADDR2;
       if (!cos_pgtbl_intern_alloc(boot_cinfo, BOOT_CAPTBL_SELF_PT, mem2.map_at, mem2.size)) BUG();
       for (dest = 0; dest < mem2.size; dest += PAGE_SIZE) {
             cos_mem_alias_at(boot_cinfo, (mem2.map_at + dest), boot_cinfo, (mem2.addr + dest));
       }

       out1 = get_output_ring((void *)mem1.addr);
       in2 = get_input_ring((void *)mem2.addr);
       conn = mca_conn_create(out1, in2);

       chains[0].first_nf = &chld_infos[next_nfid];
       fwp_fork(&chld_infos[next_nfid], text_seg, data_seg, &mem1, 0);
       next_nfid++;

       chld_infos[next_nfid-1].next = &chld_infos[next_nfid];
       fwp_fork(&chld_infos[next_nfid], text_seg, data_seg, &mem2, 1);
       next_nfid++;
       
       /*chld_infos[next_nfid-1].next = &chld_infos[next_nfid];
       fwp_fork(&chld_infos[next_nfid], text_seg, data_seg, &mem2, 2);
       next_nfid++;*/

       /*allocate the sinv capability for next_call*/
       /*next_call_sinvcap = cos_sinv_alloc(boot_cinfo, 
                            cos_compinfo_get(&chld_infos[next_nfid-1].def_cinfo)->comp_cap, 
                            sinv_next_call, 0);
       assert(next_call_sinvcap > 0);
       ret = cos_cap_cpy_at(
                     cos_compinfo_get(&chld_infos[next_nfid-2].def_cinfo),
                     BOOT_CAPTBL_NEXT_SINV_CAP, boot_cinfo, next_call_sinvcap);
       assert(ret == 0);*/

       sl_sched_loop();
       mca_conn_free(conn);
}
