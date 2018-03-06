#ifndef FWP_MANAGER_H
#define FWP_MANAGER_H

#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>

#define CK_SHM_BASE         0x80000000    /* shared memory region */
#define CK_SHM_SZ           (1<<22)       /* Shared memory mapping for each vm = 4MB */
#define MAX_NUM_NFs         10            /* This also includes the booter and the initial component*/

struct mem_seg {
       vaddr_t addr;
       size_t size;
};

struct click_info {
	struct cos_defcompinfo      def_cinfo;
       unsigned int                conf_file_idx;
       struct sl_thd               *initaep;
} chld_infos[MAX_NUM_NFs];

void fwp_fork(struct mem_seg *text_seg, struct mem_seg *data_seg, vaddr_t start_addr, unsigned long comp_info_offset);

#endif /*FWP_MANAGER_H*/
