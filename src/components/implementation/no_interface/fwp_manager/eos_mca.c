#include <stdlib.h>
#include <string.h>
#include <cos_kernel_api.h>
#include "eos_mca.h"
#include "eos_pkt.h"

#define MCA_CONN_MAX_NUM 1024

static struct mca_conn *fl, *lh;

static inline void
mca_push_l(struct mca_conn **l, struct mca_conn *c)
{
	struct mca_conn *t;

	do {
		t       = *l;
		c->next = t;
	} while (!cos_cas((unsigned long *)l, (unsigned long)t, (unsigned long)c));
}

static inline struct mca_conn *
mca_conn_alloc(void)
{
	struct mca_conn *r, *t;

	assert(fl);
	do {
		t = fl->next;
		r = fl;
	} while (!cos_cas((unsigned long *)&fl, (unsigned long)r, (unsigned long)t));
	return r;
}

static inline void
mca_conn_collect(struct mca_conn *conn)
{
	mca_push_l(&fl, conn);
}

static inline void
mca_copy(void *dst, void *src, int sl)
{
	memcpy(dst, src, sl);
}

static inline void
mca_process(struct mca_conn *conn)
{
	struct eos_ring *src, *dst;
	struct eos_ring_node *rn, *sn;
	int fh;

	src = conn->src_ring;
	dst = conn->dst_ring;
	sn = GET_RING_NODE(src, src->mca_head & EOS_RING_MASK);
	if (sn->state != PKT_SENT_READY) return ;
	assert(sn->pkt);
	assert(sn->pkt_len);
	fh  = cos_faa(&(dst->free_head), 1);
	rn  = GET_RING_NODE(dst, fh & EOS_RING_MASK);
	if (rn->state != PKT_FREE) return;
	/* printc("dbg 1 mca stat %d free %d\n", rn->state, PKT_FREE); */
	assert(rn->pkt);
	mca_copy(rn->pkt, sn->pkt, sn->pkt_len);
	rn->pkt_len = sn->pkt_len;
	sn->state = PKT_SENT_DONE;
	rn->state = PKT_RECV_READY;
	src->mca_head++;
}

static inline void
mca_scan(struct mca_conn **list)
{
	struct mca_conn **p, *c;

	p = list;
	c = *p;
	while (c) {
		if (c->used) {
			p = &(c->next);
			mca_process(c);
		} else {
			*p = c->next;
			mca_conn_collect(c);
		}
		c = *p;
	}
}

void
mca_run(void *d)
{
	while (1) {
		mca_scan(&lh);
	}
}

struct mca_conn *
mca_conn_create(struct eos_ring *src, struct eos_ring *dst)
{
	struct mca_conn *conn;

	conn = mca_conn_alloc();
	assert(conn);
	conn->src_ring = src;
	conn->dst_ring = dst;
	conn->used     = 1;
	mca_push_l(&lh, conn);
	return conn;
}

void
mca_conn_free(struct mca_conn *conn)
{
	conn->used = 0;
}

void
mca_init(struct cos_compinfo *parent_cinfo)
{
	int i, total_conn_sz;

	lh = NULL;
	total_conn_sz = sizeof(struct mca_conn) * MCA_CONN_MAX_NUM;
	fl = (struct mca_conn *)cos_page_bump_allocn(parent_cinfo, total_conn_sz);
	assert(fl);
	memset(fl, 0, total_conn_sz);
	for(i=0; i<MCA_CONN_MAX_NUM-1; i++) {
		fl[i].next = &fl[i+1];
	}
}
