#ifndef __EOS_PKT_H__
#define __EOS_PKT_H__

#include <util.h>
#include "eos_ring.h"

static inline void *
eos_pkt_allocate(struct eos_ring *ring, int len)
{
	int fh;
	struct eos_ring_node *rn;
	void *ret;
	(void)len;

	fh  = cos_faa(&(ring->free_head), 1);
	rn  = GET_RING_NODE(ring, fh & EOS_RING_MASK);
	assert(rn->state == PKT_FREE);
	ret       = rn->pkt;
	rn->state = PKT_EMPTY;
	rn->pkt   = NULL;
	return ret;
}

static inline void
eos_pkt_free(void *pkt)
{
	(void)pkt;
	printc("pkt free not implemented\n");
	assert(0);
}

static inline void
eos_pkt_send(struct eos_ring *ring, void *pkt, int len)
{
	struct eos_ring_node *rn;

	rn = GET_RING_NODE(ring, ring->sent_tail & EOS_RING_MASK);
	assert(rn->state == PKT_EMPTY);
	rn->pkt     = pkt;
	rn->pkt_len = len;
	rn->state   = PKT_SENT_READY;
	ring->sent_tail++;
}

static inline void *
eos_pkt_recv(struct eos_ring *ring, void *pkt, int *len)
{
	struct eos_ring_node *rn;
	void *ret = NULL;

	while (1) {
		rn = GET_RING_NODE(ring, ring->recv_tail & EOS_RING_MASK);
		if (rn->state != PKT_EMPTY) break;
		ring->recv_tail++;
	}
	if (rn->state == PKT_RECV_READY) {
		assert(rn->pkt);
		assert(rn->pkt_len);
		ret         = rn->pkt;
		*len        = rn->pkt_len;
		rn->pkt     = NULL;
		rn->pkt_len = 0;
		rn->state   = PKT_EMPTY;
		ring->recv_tail++;
	}
	return ret;
}

static inline void
eos_pkt_collect(struct eos_ring *recv, struct eos_ring *sent)
{
	struct eos_ring_node *rn, *sn;

	rn = GET_RING_NODE(recv, recv->recv_head & EOS_RING_MASK);
	sn = GET_RING_NODE(sent, sent->sent_head & EOS_RING_MASK);
	if (rn->state == PKT_EMPTY && sn->state == PKT_SENT_DONE) {
		rn->pkt     = sn->pkt;
		rn->pkt_len = EOS_PKT_MAX_SZ;
		rn->state   = PKT_FREE;
		sn->pkt     = NULL;
		sn->pkt_len = 0;
		rn->state   = PKT_EMPTY;
		recv->recv_head++;
		sent->sent_head++;
	}
}

#endif /* __EOS_PKT_H__ */

