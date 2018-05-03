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
struct click_pkt_header *click_pkt_fl[NUM_CPU] = {NULL}, *click_slot_fl[NUM_CPU] = {NULL};
static volatile int init_lock[NUM_CPU] = {0};

static inline void
alloc_lock_take(int cid)
{
	while (!cos_cas(&init_lock[cid], 0, 1)) ;
}

static inline void
alloc_lock_give(int cid)
{
	/* only give if you've taken! */
	/* TODO: ps_cas outside of assert! */
	assert(cos_cas(&init_lock[cid], 1, 0));
}

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
	cpuid_t cpu = cos_cpuid();

	click_slot_fl[cpu] = malloc(PKT_ALLOC_SZ);
	assert(click_slot_fl[cpu]);
	for(i=0; i<CLICK_PACKET_NUM-1; i++) {
		click_slot_fl[cpu][i].nxt = &click_slot_fl[cpu][i+1];
	}
	click_slot_fl[cpu][i].nxt = NULL;
}

void *
click_pkt_alloc()
{
	struct click_pkt_header *ret;
	cpuid_t cpu = cos_cpuid();

	if (unlikely(!click_pkt_fl[cpu])) return NULL;
	alloc_lock_take(cpu);
	ret = __fl_pop(&click_pkt_fl[cpu]);
	__fl_push(&click_slot_fl[cpu], ret);
	/* printc("dbg alloc slot %p\n", click_slot_fl); */
	alloc_lock_give(cpu);
	return ret->data;
}

void
click_pkt_free(void *packet)
{
	struct click_pkt_header *h;
	cpuid_t cpu = cos_cpuid();

	alloc_lock_take(cpu);
	if (unlikely(!click_slot_fl[cpu])) click_pkt_slot_init();
	assert(click_slot_fl[cpu]);
	h = __fl_pop(&click_slot_fl[cpu]);
	assert(h);
	h->data = packet;
	__fl_push(&click_pkt_fl[cpu], h);
	alloc_lock_give(cpu);
	/* printc("dbg fr h %p fl %p sl %p\n", h, click_pkt_fl, click_slot_fl); */
}
