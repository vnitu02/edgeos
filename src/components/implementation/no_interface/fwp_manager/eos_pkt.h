#ifndef __EOS_PKT_H__
#define __EOS_PKT_H__

#include <util.h>
#include "eos_ring.h"

#define EBLOCK  1
#define ECOLLET 2

#ifndef ps_cc_barrier
#define ps_cc_barrier() __asm__ __volatile__ ("" : : : "memory")
#endif
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

	assert(len <= EOS_PKT_MAX_SZ);
	fh  = cos_faa(&(ring->free_head), 1);
	rn  = GET_RING_NODE(ring, fh & EOS_RING_MASK);
	/* printc("dbg pkt alloc ring %p pkt %p free head %d sta %d\n", ring, rn, ring->free_head & EOS_RING_MASK, rn->state); */
	if (rn->state != PKT_FREE) return NULL;
	ret       = rn->pkt;
	rn->state = PKT_EMPTY;
	ps_cc_barrier();
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
		ps_cc_barrier();
		n->state   = PKT_FREE;
		ring->head++;
	}
}

static inline int
eos_pkt_send(struct eos_ring *ring, void *pkt, int len, int port)
{
	struct eos_ring_node *rn;

	rn = GET_RING_NODE(ring, ring->tail & EOS_RING_MASK);
	assert(rn);
	if (!rn) printc("dbg pkt sent\n");
	if (rn->state == PKT_SENT_DONE) return -ECOLLET;
	else if (rn->state != PKT_EMPTY) {printc("dbg drop %d \n", rn->state); return -EBLOCK;}
	/* else if (rn->state != PKT_EMPTY) return -EBLOCK; */
	rn->pkt     = pkt;
	rn->pkt_len = len;
	rn->port    = port;
	ps_cc_barrier();
	rn->state   = PKT_SENT_READY;
	ring->tail++;
	/* printc("S\n"); */
	return 0;
}

static inline void *
eos_pkt_recv(struct eos_ring *ring, int *len, int *port, int *err)
{
	struct eos_ring_node *rn;
	void *ret = NULL;

	while (1) {
		rn = GET_RING_NODE(ring, ring->tail & EOS_RING_MASK);
		if (rn->state != PKT_EMPTY) break;
		ring->tail++;
	}
	if (rn->state == PKT_RECV_READY) {
		/* printc("R\n"); */
		assert(rn->pkt);
		assert(rn->pkt_len);
		ret         = rn->pkt;
		*len        = rn->pkt_len;
		*port       = rn->port;
		rn->pkt     = NULL;
		rn->pkt_len = 0;
		ps_cc_barrier();
		rn->state   = PKT_RECV_DONE;
		ring->tail++;
		/* cos_faa(&(ring->pkt_cnt), -1); */
	} else if (rn->state == PKT_RECV_DONE) {
		/* printc("! tail %d stat %d\n", ring->tail, rn->state); */
		/* printc("!\n"); */
		*err = -ECOLLET;
	} else {
		/* printc("@\n"); */
		/* printc("@ tail %d stat %d\n", ring->tail, rn->state); */
		*err = -EBLOCK;
	}
	return ret;
}

static inline void
eos_pkt_collect(struct eos_ring *recv, struct eos_ring *sent)
{
	struct eos_ring_node *rn, *sn;

collect:
	rn = GET_RING_NODE(recv, recv->head & EOS_RING_MASK);
	sn = GET_RING_NODE(sent, sent->head & EOS_RING_MASK);
	if ((rn->state == PKT_EMPTY || rn->state == PKT_RECV_DONE) && sn->state == PKT_SENT_DONE) {
		rn->pkt     = sn->pkt;
		rn->pkt_len = EOS_PKT_MAX_SZ;
		sn->state   = PKT_EMPTY;
		ps_cc_barrier();
		sn->pkt     = NULL;
		sn->pkt_len = 0;
		rn->state   = PKT_FREE;
		recv->head++;
		sent->head++;
		goto collect;
	}
}

#endif /* __EOS_PKT_H__ */
