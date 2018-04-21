#include <stdlib.h>
#include <cos_component.h>
#include <cos_debug.h>

#define CLICK_PACKET_NUM (128 + 4) /* close to shared ring buffer size */
#define PKT_ALLOC_SZ (sizeof(struct click_pkt_header) * CLICK_PACKET_NUM)

struct click_pkt_header {
	struct click_pkt_header *nxt;
	void *data;
};
/* To avoid complexity from c++ constructor, here does not directly allocate class memory, 
 * instead we rely on new to allocate memory, and cache its allocation inside packet deallocation */
struct click_pkt_header *click_pkt_fl = NULL, *click_slot_fl = NULL;

static inline void
__fl_push(struct click_pkt_header **l, struct click_pkt_header *n)
{
	n->nxt = *l;
	*l = n;
}

static inline struct click_pkt_header *
__fl_pop(struct click_pkt_header **l)
{
	struct click_pkt_header *h;

	assert(*l);
	h = *l;
	*l = h->nxt;
	return h;
}

static inline void
click_pkt_slot_init()
{
	unsigned int i;
	struct click_pkt_header *h;

	click_slot_fl = malloc(PKT_ALLOC_SZ);
	assert(click_slot_fl);
	for(i=0; i<CLICK_PACKET_NUM-1; i++) {
		click_slot_fl[i].nxt = &click_slot_fl[i+1];
	}
	click_slot_fl[i].nxt = NULL;
}

void *
click_pkt_alloc()
{
	struct click_pkt_header *ret;

	if (unlikely(!click_pkt_fl)) return NULL;
	ret = __fl_pop(&click_pkt_fl);
	__fl_push(&click_slot_fl, ret);
	/* printc("dbg alloc slot %p\n", click_slot_fl); */
	return ret->data;
}

void
click_pkt_free(void *packet)
{
	struct click_pkt_header *h;

	if (unlikely(!click_slot_fl)) click_pkt_slot_init();
	assert(click_slot_fl);
	h = __fl_pop(&click_slot_fl);
	h->data = packet;
	__fl_push(&click_pkt_fl, h);
	/* printc("dbg fr h %p fl %p sl %p\n", h, click_pkt_fl, click_slot_fl); */
}
