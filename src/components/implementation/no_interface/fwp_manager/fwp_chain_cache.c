#include "eos_utils.h"
#include "fwp_chain_cache.h"

struct nf_chain *fwp_chain_cache[COUNT][FWP_CORES];

void
fwp_chain_put(struct nf_chain *chain, enum fwp_chain_state state, unsigned short core)
{
	struct nf_chain *t, **h;
	h = &fwp_chain_cache[state][core];
       
	do {
		t = ps_load(h);
		chain->next = t;
	} while (!cos_cas((unsigned long *)h, (unsigned long)t, (unsigned long)chain));
}

struct nf_chain *
fwp_chain_get(enum fwp_chain_state state, unsigned short core)
{
	struct nf_chain *r, *t, **h;

	h = &fwp_chain_cache[state][core];
	assert(*h);
	do {
		r = ps_load(h);
		t = r->next;
	} while (!cos_cas((unsigned long *)h, (unsigned long)r, (unsigned long)t));
	return r;
}
