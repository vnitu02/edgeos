#ifndef FWP_MANAGER_H
#define FWP_MANAGER_H

#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>

#include "eos_ring.h"

#define FWP_MEMSEG_SIZE (round_up_to_page(FWP_RINGS_SIZE) + EOS_RING_SIZE * EOS_PKT_MAX_SZ)

/* Ensure this is the same as what is in sl_mod_fprr.c */
#define SL_FPRR_NPRIOS 32
#define LOWEST_PRIORITY (SL_FPRR_NPRIOS - 1)
#define LOW_PRIORITY (LOWEST_PRIORITY - 1)

#define list_for_each_nf(pos, head) \
       for(pos = (head)->first_nf; pos != NULL; pos = pos->next)

struct mem_seg {
       vaddr_t addr;
       size_t size;
};

struct click_info {
       struct cos_defcompinfo      def_cinfo;
       int                         conf_file_idx;       /*-1 means that we forked from template*/
       int                         nf_id;
       struct sl_thd               *initaep;
       vaddr_t                     booter_vaddr;        /*the address of this component's data segment in the booter*/
       struct click_info           *next;
       vaddr_t                     shmem_addr;
	struct mem_seg *data_seg;
	int template_id;
	int nd_thd, nd_ring, nd_sinv;
};

struct nf_chain {
	int chain_id;
	int tot_nf, active_nf;
	struct click_info *first_nf, *last_nf;
       struct nf_chain *next;
	struct eos_ring *rx_out;
	/* TODO: add mca conn, tx ring for clean up */
};

void fwp_test(struct mem_seg *text_seg, struct mem_seg *data_seg, vaddr_t start_addr, 
              unsigned long comp_info_offset, vaddr_t sinv_next_call);
void fwp_chain_activate(struct nf_chain *chain);

#endif /*FWP_MANAGER_H*/
