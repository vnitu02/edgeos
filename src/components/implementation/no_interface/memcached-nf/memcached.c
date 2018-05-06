#include <memcached.h>

int
mc_set_key_rcv(char *key, int nkey, char *data, int nbytes)
{
    item *old_it = NULL, *it = NULL;
    unsigned int hv;

    int node = 0;
    hv = hash(key, nkey);
    it = do_item_alloc(node, key, nkey, 0, 0, nbytes+2, 0);
    if (!it) {
        printc("ERROR: item_alloc failed once?\n");
        return -1;
    }
    memcpy(ITEM_data(it), data, nbytes);
    it->hv = hv;

    old_it = do_item_get(node, key, nkey, hv);
    assert(old_it == NULL);
    do_item_link(node, it, hv);
    return 0;
}

char *
mc_get_key_ext(char *key, int nkey, int *nbyte)
{
    item *it;
    int node;
    uint32_t hv;

    hv   = hash(key, nkey);
    node = 0;
    it = do_item_get(node, key, nkey, hv);
    assert(it);
/* #define UPDATE_RA (50) */
/* #define MODULE (100 - UPDATE_RA) */
/* if ((status[node].get % MODULE) < UPDATE_RA * (NUM_NODE/2)) { */
/*     assoc_flush(hv); */
/*     it = do_item_get(node, key, nkey, hv); */
/*     assert(it); */
/* } */

    if (it) {
        *nbyte = it->nbytes-2;
        return ITEM_data(it);
    } else {
        *nbyte = 0;
        return NULL;
    }
}
