#ifndef CK_INIT_H
#define CK_INIT_H

#include <cos_kernel_api.h>
#include <cos_types.h>

/*
 *  * sinv to another click component
 *   */
static int
next_call_sinv(void)
{
       cos_sinv(BOOT_CAPTBL_NEXT_SINV_CAP, 0, 0, 0, 0);
}

#endif /*CK_INIT_H*/
