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

void fwp_chain_put(struct nf_chain *chain, enum fwp_chain_state state, unsigned short core);
struct nf_chain *fwp_chain_get(enum fwp_chain_state state, unsigned short core);

#endif /*FWP_CHAIN_CACHE_H*/
