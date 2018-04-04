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
	char pad1[2 * CACHE_LINE - sizeof(struct eos_ring_node *)];
	int free_head;        /* shared head */
	char pad2[2 * CACHE_LINE - sizeof(int)];
	int mca_head;        /* mca access only */
	char pad3[2 * CACHE_LINE - sizeof(int)];
	int recv_head, recv_tail;    /* nf access only */
	int sent_head, sent_tail;
};

/**
 * shared ring buffer and packet memory layout:    
 *
 * +-----------------------------------------------------------------------+-------PKT MEM SZ----+ 
 * | input ring | EOS_RING_SIZE nodes | output ring | EOS_RING_SIZE nodes  | packet |...| packet |
 * +-----------------------------------------------------------------------+---------------------+ 
 *
 */
static inline struct eos_ring *
get_input_ring(void *rh){
       return (struct eos_ring *)rh;
}

static inline struct eos_ring *
get_output_ring(void *rh){
       struct eos_ring *output_ring = (struct eos_ring *)((char *)rh + sizeof(struct eos_ring) + 
                                          EOS_RING_SIZE * sizeof(struct eos_ring_node));
       return output_ring;
}

static inline void
eos_rings_init(void *rh)
{
       struct eos_ring *input_ring, *output_ring;
	void *pkts, *end_of_rings;
	int i;

	assert(((unsigned long)rh & PAGE_MASK) == 0);
       memset(rh, 0, sizeof(struct eos_ring));

       input_ring = (struct eos_ring *)rh;
       input_ring->ring = (struct eos_ring_node *)((char *)input_ring + sizeof(struct eos_ring));

       output_ring = (struct eos_ring *)((char *)input_ring->ring + EOS_RING_SIZE * sizeof(struct eos_ring_node));
       output_ring->ring = (struct eos_ring_node *)((char *)output_ring + sizeof(struct eos_ring));

       end_of_rings = (struct eos_ring *)((char *)output_ring->ring + EOS_RING_SIZE * sizeof(struct eos_ring_node));
       pkts = (void *)round_up_to_page(end_of_rings);

       for(i=0; i<EOS_RING_SIZE; i++) {
              output_ring->ring[i].state = PKT_EMPTY;

              input_ring->ring[i].pkt_len = EOS_PKT_MAX_SZ;
		input_ring->ring[i].state = PKT_FREE;
		input_ring->ring[i].pkt = pkts;
		pkts += EOS_PKT_MAX_SZ;
	}
}

#endif /* __ESO_RING_H__ */

