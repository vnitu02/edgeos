#ifndef EOS_INIT_H
#define EOS_INIT_H

#include <cos_kernel_api.h>
#include <cos_types.h>

#define CURR_CINFO() (cos_compinfo_get(cos_defcompinfo_curr_get()))

/*
 *  * sinv to another click component
 *   */
static inline void
next_call_sinv(void)
{
       cos_sinv(BOOT_CAPTBL_NEXT_SINV_CAP, 0, 0, 0, 0);
}

#endif /*EOS_INIT_H*/
