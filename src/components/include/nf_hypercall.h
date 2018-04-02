#ifndef LLBOOT_H
#define LLBOOT_H

#include <cos_kernel_api.h>

typedef unsigned short int nfid_t;

enum llboot_cntl {
       NF_MALLOC = 0,
       NF_CONFIDX_GET,
       NF_CHECKPOINT,
       NF_CLEAN,
};

static inline int
nf_hyp_malloc(size_t size, vaddr_t *vaddr)
{
       word_t unused = 0;

       return cos_sinv_rets(BOOT_CAPTBL_HYP_SINV_CAP, 0, NF_MALLOC, size, 0, vaddr, &unused);
}

static inline int
nf_hyp_confidx_get(int *conf_file_idx)
{
       word_t unused = 0;       

       return cos_sinv_rets(BOOT_CAPTBL_HYP_SINV_CAP, 0, NF_CONFIDX_GET, 0, 0, (word_t *)conf_file_idx, &unused);
}

static inline int
nf_hyp_checkpoint(unsigned int nfid)
{
       return cos_sinv(BOOT_CAPTBL_HYP_SINV_CAP, 0, NF_CHECKPOINT, nfid, 0);
}

static inline int
nf_hyp_clean(void)
{
       return cos_sinv(BOOT_CAPTBL_HYP_SINV_CAP, 0, NF_CLEAN, 0, 0);
}

#endif /* LLBOOT_H */
