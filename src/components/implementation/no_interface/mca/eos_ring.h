#ifndef __ESO_RING_H__
#define __ESO_RING_H__

#include <cos_debug.h>
#include <consts.h>
#include <cos_types.h>

#define EOS_PKT_MAX_SZ 1500
#define EOS_RING_SIZE 1024
#define EOS_RING_MASK (EOS_RING_SIZE - 1)
#define GET_RING_NODE(r, h) (&((r)->ring[(h)]))

typedef enum {
	PKT_EMPTY = 0,
	PKT_FREE,
	PKT_RECV_READY,
	PKT_SENT_READY,
	PKT_SENT_DONE,
} pkt_states_t;

struct eos_ring_node {
	pkt_states_t state;
	void *pkt;
	int pkt_len;
};

struct eos_ring {
	struct eos_ring_node *ring;  /* read only */
	char pad1[CACHE_LINE - sizeof(struct eos_ring_node *)];
	int free_head;        /* shared head */
	char pad2[CACHE_LINE - sizeof(int)];
	int mca_head;        /* mca access only */
	char pad3[CACHE_LINE - sizeof(int)];
	int recv_head, recv_tail;    /* nf access only */
	int sent_head, sent_tail;
};

/** 
 * shared ring buffer and packet memory layout:    
 *
 * +-------------------ONE PAGE-----------------+-------PKT MEM SZ----+ 
 * | struct eos_ring | ring_node |...|ring_node | packet |...| packet |
 * +--------------------------------------------+---------------------+ 
 *
 */
static inline void
eos_ring_init(struct eos_ring *rh, int recv)
{
	void *pkts;
	int i;

	assert(((unsigned long)rh & PAGE_MASK) == 0);
	memset(rh, 0, sizeof(struct eos_ring));
	pkts = (void *)rh + PAGE_SIZE;
	rh->ring = (struct eos_ring_node *)pkts;
	if (recv) {
		for(i=0; i<EOS_RING_SIZE; i++) {
			rh->ring[i].pkt_len = EOS_PKT_MAX_SZ;
			rh->ring[i].state = PKT_FREE;
			rh->ring[i].pkt = pkts;
			pkts += EOS_PKT_MAX_SZ;
		}
	} else {
		for(i=0; i<EOS_RING_SIZE; i++) {
			rh->ring[i].state = PKT_EMPTY;
		}
	}
}

#endif /* __ESO_RING_H__ */

