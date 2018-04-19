#ifndef FWP_CHAIN_CACHE_H
#define FWP_CHAIN_CACHE_H

#include"fwp_manager.h"

/*the number of cores in the system*/
#define FWP_CORES 1

enum fwp_chain_state
{
       FWP_CHAIN_CLEANED,
       FWP_CHAIN_UNCLEANED,
       COUNT,
};

static struct nf_chain *fwp_chain[COUNT][FWP_CORES];

static void
fwp_chain_put(struct nf_chain *chain, enum fwp_chain_state state, unsigned short core)
{
       struct nf_chain *t;
       
       do {
              t = fwp_chain[state][core];
              chain->next = t;
       } while (!cos_cas((unsigned long *)&fwp_chain[state][core], 
                     (unsigned long)t, (unsigned long)chain));
}

static struct nf_chain *
fwp_chain_get(enum fwp_chain_state state, unsigned short core)
{
       struct nf_chain *r, *t;

       assert(fwp_chain[state][core]);
       do {
              t = fwp_chain[state][core]->next;
              r = fwp_chain[state][core];
       } while (!cos_cas((unsigned long *)&fwp_chain[state][core], 
                     (unsigned long)r, (unsigned long)t));
       return r;
}

#endif /*FWP_CHAIN_CACHE_H*/
