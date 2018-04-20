#ifndef CK_INIT_H
#define CK_INIT_H

#include <cos_kernel_api.h>
#include <cos_types.h>

/*
 *  * sinv to another click component
 *   */
static int
next_call_sinv(int packet_addr, int packet_length)
{
       cos_sinv(BOOT_CAPTBL_NEXT_SINV_CAP, 0, packet_addr, packet_length, 0);
}

#endif /*CK_INIT_H*/
