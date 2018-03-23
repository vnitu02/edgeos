#ifndef FWP_MANAGER_H
#define FWP_MANAGER_H

#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>

#define DEFAULT_SHMEM_ADDR1 (0x80000000)
#define DEFAULT_SHMEM_ADDR2 (0x81000000)
#define DEFAULT_SHMEM_SIZE (1<<22)
#define MAX_NUM_NFs         10            /* This also includes the booter and the initial component*/
#define MAX_NUM_CHAINS      2             /* The maximum number of chains in the system*/

struct mem_seg {
       vaddr_t addr;
       size_t size;
       vaddr_t map_at;
};

struct click_info {
       struct cos_defcompinfo      def_cinfo;
       int                         conf_file_idx;       /*-1 means that we forked from template*/
       int                         nf_id;
       struct sl_thd               *initaep;
       vaddr_t                     booter_vaddr;        /*the address of this component's data segment in the booter*/
       struct click_info           *next;
} chld_infos[MAX_NUM_NFs];

struct nf_chain {
       struct click_info *first_nf;
} chains[MAX_NUM_CHAINS];

struct mem_seg templates[MAX_NUM_NFs];

void fwp_test(struct mem_seg *text_seg, struct mem_seg *data_seg, vaddr_t start_addr, 
              unsigned long comp_info_offset, vaddr_t sinv_next_call);

#endif /*FWP_MANAGER_H*/
