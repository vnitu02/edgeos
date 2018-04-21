#include <cos_defkernel_api.h>
#include "eos_pkt.h"
#include "ninf.h"
#include "ninf_util.h"
#include "eos_utils.h"

struct tx_ring {
	struct eos_ring *r;
	int state;
	struct tx_ring *next;
};

struct tx_ring *tx, *fl;
void
ninf_tx_init()
{
	/* FIXME: alloc memory for fl, and init it */
}

void
ninf_tx_add_ring(struct eos_ring *r)
{
}

void
ninf_tx_del_ring(struct tx_ring *r)
{
	r->state = 0;
}

void
ninf_tx_loop()
{
}
