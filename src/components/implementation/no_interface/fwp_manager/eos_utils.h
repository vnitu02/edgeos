#ifndef EOS_INIT_H
#define EOS_INIT_H

#include <cos_kernel_api.h>
#include <cos_types.h>

#define EOS_MAX_CHAIN_NUM 100
#define EOS_MAX_FLOW_NUM  100
#define CURR_CINFO() (cos_compinfo_get(cos_defcompinfo_curr_get()))

/*
 *  * sinv to another click component
 *   */
static void
next_call_sinv(int packet_addr, int packet_length, int port)
{
       cos_sinv(BOOT_CAPTBL_NEXT_SINV_CAP, 0, packet_addr, packet_length, port);
}

#endif /*EOS_INIT_H*/
