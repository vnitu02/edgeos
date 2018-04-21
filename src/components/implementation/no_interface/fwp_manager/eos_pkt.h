#ifndef __EOS_PKT_H__
#define __EOS_PKT_H__

#include <util.h>
#include "eos_ring.h"

/* static inline void */
/* dbg_state(struct eos_ring *ring) */
/* { */
/* 	struct eos_ring_node *sn; */

/* 	sn = GET_RING_NODE(ring, ring->head & EOS_RING_MASK); */
/* 	printc("dbg stat pkt sr %p rh %d sh %d\n", ring, ring->head & EOS_RING_MASK, sn->state); */
/* } */

static inline void *
eos_pkt_allocate(struct eos_ring *ring, int len)
{
	int fh;
	struct eos_ring_node *rn;
	void *ret;
	(void)len;

	fh  = cos_faa(&(ring->free_head), 1);
	rn  = GET_RING_NODE(ring, fh & EOS_RING_MASK);
	/* printc("dbg pkt alloc ring %p pkt %p free head %d sta %d\n", ring, rn, ring->free_head & EOS_RING_MASK, rn->state); */
	if (rn->state != PKT_FREE) return NULL;
	ret       = rn->pkt;
	rn->state = PKT_EMPTY;
	rn->pkt   = NULL;
	return ret;
}

static inline void
eos_pkt_free(struct eos_ring *ring, void *pkt)
{
	struct eos_ring_node *n;

	n = GET_RING_NODE(ring, ring->head & EOS_RING_MASK);
	/* printc("dbg pkt free ring %p pkt %p head %d stat %d\n", ring, pkt, ring->head & EOS_RING_MASK, n->state); */
	if (n->state == PKT_EMPTY) {
		n->pkt     = pkt;
		n->pkt_len = EOS_PKT_MAX_SZ;
		n->state   = PKT_FREE;
		ring->head++;
	}
}

static inline void
eos_pkt_send(struct eos_ring *ring, void *pkt, int len)
{
	struct eos_ring_node *rn;

	rn = GET_RING_NODE(ring, ring->tail & EOS_RING_MASK);
	assert(rn->state == PKT_EMPTY);
	rn->pkt     = pkt;
	rn->pkt_len = len;
	rn->state   = PKT_SENT_READY;
	ring->tail++;
	/* printc("dbg pkt send ring %p pkt %p tail %d sta %d\n", ring, rn, ring->tail & EOS_RING_MASK, rn->state); */
}

static inline void *
eos_pkt_recv(struct eos_ring *ring, int *len)
{
	struct eos_ring_node *rn;
	void *ret = NULL;

	while (1) {
		rn = GET_RING_NODE(ring, ring->tail & EOS_RING_MASK);
		if (rn->state != PKT_EMPTY) break;
		ring->tail++;
	}
	if (rn->state == PKT_RECV_READY) {
		/* printc("dbg pkt recv ring %p pkt %p tail %d sta %d\n", ring, rn, ring->tail & EOS_RING_MASK, rn->state); */
		assert(rn->pkt);
		assert(rn->pkt_len);
		ret         = rn->pkt;
		*len        = rn->pkt_len;
		rn->pkt     = NULL;
		rn->pkt_len = 0;
		rn->state   = PKT_EMPTY;
		ring->tail++;
	}
	return ret;
}

static inline void
eos_pkt_collect(struct eos_ring *recv, struct eos_ring *sent)
{
	struct eos_ring_node *rn, *sn;

	rn = GET_RING_NODE(recv, recv->head & EOS_RING_MASK);
	sn = GET_RING_NODE(sent, sent->head & EOS_RING_MASK);
	if (rn->state == PKT_EMPTY && sn->state == PKT_SENT_DONE) {
		/* printc("dbg pkt col rr %p sr %p rh %d sh %d\n", recv, sent, recv->head & EOS_RING_MASK, sent->head & EOS_RING_MASK); */
		rn->pkt     = sn->pkt;
		rn->pkt_len = EOS_PKT_MAX_SZ;
		rn->state   = PKT_FREE;
		sn->pkt     = NULL;
		sn->pkt_len = 0;
		sn->state   = PKT_EMPTY;
		recv->head++;
		sent->head++;
	}
}

#endif /* __EOS_PKT_H__ */
