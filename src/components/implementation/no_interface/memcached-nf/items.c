/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include <nf_hypercall.h>
#include "memcached.h"

#define MCS_CLOCK_LOCK

#ifdef MCS_CLOCK_LOCK
#define LOCK_CLOCK(hv)  ck_spinlock_mcs_context_t me; 	       \
                        ck_pr_store_uint(&(me.locked), false); \
                        ck_pr_store_ptr(&(me.next), NULL);     \
                        ck_spinlock_mcs_lock(&(clocks[hv & hashmask(N_CLOCK_POWER)].l_clock), &me)

#define UNLOCK_CLOCK(hv)  ck_spinlock_mcs_unlock(&(clocks[hv & hashmask(N_CLOCK_POWER)].l_clock), &me)
#else

#define LOCK_CLOCK(hv)    mutex_lock(&cache_lock)
#define UNLOCK_CLOCK(hv)  mutex_unlock(&cache_lock)

#endif
/* Enable this for reference-count debugging. */
#if 0
# define DEBUG_REFCNT(it,op) \
                fprintf(stderr, "item %x refcnt(%c) %d %c%c%c\n", \
                        it, op, it->refcount, \
                        (it->it_flags & ITEM_LINKED) ? 'L' : ' ', \
                        (it->it_flags & ITEM_SLABBED) ? 'S' : ' ')
#else
# define DEBUG_REFCNT(it,op) while(0)
#endif

/**
 * Generates the variable-sized part of the header for an object.
 *
 * key     - The key
 * nkey    - The length of the key
 * flags   - key flags
 * nbytes  - Number of bytes to hold value and addition CRLF terminator
 * suffix  - Buffer for the "VALUE" line suffix (flags, size).
 * nsuffix - The length of the suffix is stored here.
 *
 * Returns the total size of the header.
 */
static size_t
item_make_header(const uint8_t nkey, const int flags, const int nbytes, char *suffix, uint8_t *nsuffix)
{
    /* suffix is defined at 40 chars elsewhere.. */
    *nsuffix = (uint8_t) snprintf(suffix, 40, " %d %d\r\n", flags, nbytes - 2);
    return sizeof(item) + nkey + *nsuffix + nbytes;
}

/*@null@*/
item *
do_item_alloc(int node, char *key, const size_t nkey, const int flags,
                    const rel_time_t exptime, const int nbytes,
                    const uint32_t cur_hv)
{
    uint8_t nsuffix;
    item *it = NULL;
    char suffix[40];
    size_t ntotal = item_make_header(nkey + 1, flags, nbytes, suffix, &nsuffix);

    nf_hyp_malloc(ntotal, (vaddr_t *)&it);
    if (it == NULL) {
        printc(">>>>>>>>>>>>>>>>>>> WARNING: NO memory allocated after eviction!\n");
        return NULL;
    }
    assert(it->slabs_clsid == 0);

    it->next        = it->prev = it->h_next = 0;
    it->slabs_clsid = 0;
    it->it_flags    = 0;
    it->nkey        = nkey;
    it->nbytes      = nbytes;
    it->exptime     = 0; //exptime; /* disable expiration. */
    it->nsuffix     = nsuffix;
    memcpy(ITEM_key(it), key, nkey);
    memcpy(ITEM_suffix(it), suffix, (size_t)nsuffix);

    return it;
}
#if 0
void
item_free(int node, item *it)
{
    it->slabs_clsid = 0;
    parsec_mem_free(node, it);
}
#endif
int
do_item_link(int node, item *it, const uint32_t hv)
{
    assert((it->it_flags & (ITEM_LINKED|ITEM_SLABBED)) == 0);
    it->it_flags |= ITEM_LINKED;
    assoc_insert(node, it, hv);

    return 1;
}

#if 0
void
do_item_unlink(int node, item *it, const uint32_t hv)
{
    assert(it->it_flags & ITEM_LINKED);
    it->it_flags &= ~ITEM_LINKED;
    assoc_delete(node, ITEM_key(it), it->nkey, hv);
    do_item_remove_free(node, it);
}

/* this ignores the refcnt. */
void
do_item_remove_free(int node, item *it)
{
    assert((it->it_flags & ITEM_SLABBED) == 0);

    item_free(node, it);
}

void
do_item_remove(int node, item *it)
{
    PARSEC_NOT_USED;
    assert((it->it_flags & ITEM_SLABBED) == 0);
    assert(it->refcount == 0);
    item_free(node, it);
}

void
do_item_update(int node, item *it)
{
#ifdef NO_REPLACEMENT
    return;
#else
    if (it->recency == 0) it->recency = 1;
#endif
}
#endif
/** wrapper around assoc_find which does the lazy expiration logic */
item *
do_item_get(int node, const char *key, const size_t nkey, const uint32_t hv)
{
    item *it = assoc_find(node, key, nkey, hv);

    return it;
}
#if 0
int
do_item_replace(int node, item *old, item *new, const uint32_t hv)
{
    assert(old->it_flags & ITEM_LINKED);    
    assert((new->it_flags & (ITEM_LINKED|ITEM_SLABBED)) == 0);
    new->it_flags |= ITEM_LINKED;
    assoc_replace(node, old, new, hv);
    old->it_flags &= ~ITEM_LINKED;
    do_item_remove_free(node, old);

    return 1;
}

item *
do_item_touch(int node, const char *key, size_t nkey, uint32_t exptime, const uint32_t hv)
{
    item *it = do_item_get(node, key, nkey, hv);
    if (it != NULL) {
        it->exptime = exptime;
    }
    return it;
}
#endif
