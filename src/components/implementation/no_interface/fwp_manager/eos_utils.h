#ifndef EOS_INIT_H
#define EOS_INIT_H

#include <cos_kernel_api.h>
#include <cos_types.h>

/* Assume 16 cores in total, and 4 of them reserved for system  */
#define EOS_MAX_NF_TYPE_NUM    10 /* number of chain type, need such number of templates */
#define EOS_MAX_CHAIN_NUM_PER_CORE 128
#define EOS_MAX_CHAIN_NUM (EOS_MAX_CHAIN_NUM_PER_CORE * (NUM_CPU - 4))
#define EOS_MAX_NF_NUM_PER_CORE   (1 * EOS_MAX_CHAIN_NUM_PER_CORE) /* chain length * #chains */
#define EOS_MAX_NF_NUM    (2 + EOS_MAX_NF_NUM_PER_CORE * (NUM_CPU - 4))        /* This also includes the booter and the initial component*/
#define EOS_MAX_FLOW_NUM  100 	/* number of client flow, should equal to number of chain if we have a chain per flow */
/*we suppose having one shared sgment per chain*/
#define EOS_MAX_MEMSEGS_NUM EOS_MAX_CHAIN_NUM
#define FWP_RINGS_SIZE (2 * sizeof(struct eos_ring) + 2 * EOS_RING_SIZE * sizeof(struct eos_ring_node))
#define CURR_CINFO() (cos_compinfo_get(cos_defcompinfo_curr_get()))

enum {
	FWP_MGR_CORE = 0,
	MCA_CORE     = 1,
	NINF_RX_CORE = 2,
	NINF_TX_CORE = 3,
	NF_MIN_CORE  = 4,
	NF_MAX_CORE  = NUM_CPU,
};

/*
 *  * sinv to another click component
 *   */
static void
next_call_sinv(int packet_addr, int packet_length, int port)
{
       cos_sinv(BOOT_CAPTBL_NEXT_SINV_CAP, 0, packet_addr, packet_length, port);
}

#endif /*EOS_INIT_H*/
