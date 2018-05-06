/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */

/** \file
 * The main memcached header holding commonly used data
 * structures and function prototypes.
 */
#ifndef MEMCACHED_H
#define MEMCACHED_H

#include "config.h"

// if defined, ignore all clock list ops, including locking
#define NO_REPLACEMENT
#define MC_NO_MEM_BALANCE 
/* #define KEY_SKEW 1 */
/*#define MC_IPC       1*/
#define SET_RPC_TEST 1
#define KEY_LENGTH            16
#define V_LENGTH              (32)
#define MC_MEM_EVICT_MAX      1
#define MC_MEM_EVICT_THOLD    99
#define MC_MEM_BALANCE_MAX    512
#define MC_MEM_BALANCE_THOLD  98
#define MC_MEM_BALANCE_PEROID 10000000
#ifdef SET_RPC_TEST
#define MC_HASH_FLUSH_PEROID  90000000
#else
#define MC_HASH_FLUSH_PEROID  27000000
#endif
#define MAX_MC_HASH_TIME      (1054438*2)
/* Initial power multiplier for the hash table */
#define HASHPOWER_DEFAULT     22
#define MC_SLAB_OBJ_SZ        (94)

#define TRACE_FILE "../mc_trace/trace10p_key"

#define N_CLOCK_POWER (6)
#define N_CLOCK (1 << N_CLOCK_POWER)

// for 99 percentile w/ limited memory
#define THRES (3000)

#define CACHE_LINE (64)

#define PARSEC_NOT_USED printc("Warning: should not use this w/ PARSEC!\n")

/** Maximum length of a key. */
#define KEY_MAX_LENGTH 250

/** Size of an incr buf. */
#define INCR_MAX_STORAGE_LEN 24

#define DATA_BUFFER_SIZE 2048
#define UDP_READ_BUFFER_SIZE 65536
#define UDP_MAX_PAYLOAD_SIZE 1400
#define UDP_HEADER_SIZE 8
#define MAX_SENDBUF_SIZE (256 * 1024 * 1024)
/* I'm told the max length of a 64-bit num converted to string is 20 bytes.
 * Plus a few for spaces, \r\n, \0 */
#define SUFFIX_SIZE 24

/** Initial size of list of items being returned by "get". */
#define ITEM_LIST_INITIAL 200

/** Initial size of list of CAS suffixes appended to "gets" lines. */
#define SUFFIX_LIST_INITIAL 20

/** Initial size of the sendmsg() scatter/gather array. */
#define IOV_LIST_INITIAL 400

/** Initial number of sendmsg() argument structures to allocate. */
#define MSG_LIST_INITIAL 10

/** High water marks for buffer shrinking */
#define READ_BUFFER_HIGHWAT 8192
#define ITEM_LIST_HIGHWAT 400
#define IOV_LIST_HIGHWAT 600
#define MSG_LIST_HIGHWAT 100

/* Binary protocol stuff */
#define MIN_BIN_PKT_LENGTH 16
#define BIN_PKT_HDR_WORDS (MIN_BIN_PKT_LENGTH/sizeof(uint32_t))

/*
 * We only reposition items in the LRU queue if they haven't been repositioned
 * in this many seconds. That saves us from churning on frequently-accessed
 * items.
 */
#define ITEM_UPDATE_INTERVAL 60

/* Slab sizing definitions. */
#define POWER_SMALLEST 1
#define POWER_LARGEST  200
#define CHUNK_ALIGN_BYTES 8
#define MAX_NUMBER_OF_SLAB_CLASSES (POWER_LARGEST + 1)

/** How long an object can reasonably be assumed to be locked before
    harvesting it on a low memory condition. Default: disabled. */
#define TAIL_REPAIR_TIME_DEFAULT 0

/* warning: don't use these macros with a function, as it evals its arg twice */
#define ITEM_get_cas(i) (((i)->it_flags & ITEM_CAS) ? \
        (i)->data->cas : (uint64_t)0)

#define ITEM_set_cas(i,v) { \
    if ((i)->it_flags & ITEM_CAS) { \
        (i)->data->cas = v; \
    } \
}

#define ITEM_key(item) (((char*)&((item)->data)) \
         + (((item)->it_flags & ITEM_CAS) ? sizeof(uint64_t) : 0))

#define ITEM_suffix(item) ((char*) &((item)->data) + (item)->nkey + 1 \
         + (((item)->it_flags & ITEM_CAS) ? sizeof(uint64_t) : 0))

#define ITEM_data(item) ((char*) &((item)->data) + (item)->nkey + 1 \
         + (item)->nsuffix \
         + (((item)->it_flags & ITEM_CAS) ? sizeof(uint64_t) : 0))

#define ITEM_ntotal(item) (sizeof(struct _stritem) + (item)->nkey + 1 \
         + (item)->nsuffix + (item)->nbytes \
         + (((item)->it_flags & ITEM_CAS) ? sizeof(uint64_t) : 0))

#define STAT_KEY_LEN 128
#define STAT_VAL_LEN 128

/** Append a simple stat with a stat name, value format and value */
#define APPEND_STAT(name, fmt, val) \
    append_stat(name, add_stats, c, fmt, val);

/** Append an indexed stat with a stat name (with format), value format
    and value */
#define APPEND_NUM_FMT_STAT(name_fmt, num, name, fmt, val)          \
    klen = snprintf(key_str, STAT_KEY_LEN, name_fmt, num, name);    \
    vlen = snprintf(val_str, STAT_VAL_LEN, fmt, val);               \
    add_stats(key_str, klen, val_str, vlen, c);

/** Common APPEND_NUM_FMT_STAT format. */
#define APPEND_NUM_STAT(num, name, fmt, val) \
    APPEND_NUM_FMT_STAT("%d:%s", num, name, fmt, val)

#define IS_UDP(x) (x == udp_transport)

#define NREAD_ADD 1
#define NREAD_SET 2
#define NREAD_REPLACE 3
#define NREAD_APPEND 4
#define NREAD_PREPEND 5
#define NREAD_CAS 6

/** Time relative to server start. Smaller than time_t on 64-bit systems. */
typedef unsigned int rel_time_t;

#define MAX_VERBOSITY_LEVEL 2

#define ITEM_LINKED 1
#define ITEM_CAS 2

/* temp */
#define ITEM_SLABBED 4

#define ITEM_FETCHED 8

// the following disables LRU.
#define CLOCK_REPLACEMENT

/**
 * Structure for storing items within memcached.
 */
typedef struct _stritem {
    struct _stritem *next;
    struct _stritem *prev;
    struct _stritem *h_next;    /* hash chain next */
#ifdef CLOCK_REPLACEMENT
    uint8_t         recency;   /* QW: recent used bit in CLOCK replacement */
#endif
    unsigned int    hv;       /* least recent access */
    unsigned int    exptime;    /* expire time */
    int             nbytes;     /* size of data */
    unsigned short  refcount;
    uint8_t         nsuffix;    /* length of flags-and-length string */
    uint8_t         it_flags;   /* ITEM_* above */
    uint8_t         slabs_clsid;/* which slab class we're in */
    uint8_t         nkey;       /* key length, w/terminating null and padding */
    /* this odd type prevents type-punning issues when we do
     * the little shuffle to save space when not using CAS. */
    union {
        uint64_t cas;
        char end;
    } data[];
    /* if it_flags & ITEM_CAS we have 8 bytes CAS */
    /* then null-terminated key */
    /* then " flags length\r\n" (no terminating null) */
    /* then data with terminating \r\n (no terminating null; it's binary!) */
} item;

#define mutex_unlock(x) pthread_mutex_unlock(x)

#include "ps_plat.h"
#include "ps_global.h"
#include "ps_slab.h"
//#include "micro_booter.h"
//#include "server_stub.h"
//#include "test.h"

#include "assoc.h"
#include "items.h"
#include "hash.h"

/* If supported, give compiler hints for branch prediction. */
#if !defined(__GNUC__) || (__GNUC__ == 2 && __GNUC_MINOR__ < 96)
#define __builtin_expect(x, expected_value) (x)
#endif

#ifndef likely
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)
#endif
#if 0
extern char *load_key;
struct ps_slab *mc_alloc_pages(struct ps_mem *m, size_t sz, coreid_t coreid);
void mc_free_pages(struct ps_mem *m, struct ps_slab *s, size_t sz, coreid_t coreid);
void mem_free_pages(struct ps_mem *m, struct ps_slab *s, size_t sz, coreid_t coreid);
void *parsec_mem_alloc(int node, int size);
void parsec_mem_free(int node, void *item);
char *mc_get_key_ext(char *key, int nkey, int *nbyte);
int mc_set_key_ext(int caller, int node, char *key, int nkey, char *data, int nbytes, uint32_t hv);
void *mc_register(int node, int type, int npage);   /* set up shared page for key, data and return */
int mc_set_key(int node, int nkey, int nbytes);
void *mc_get_key(int node, int nkey);
void mc_server_start(int cur);
void mc_print_status(void);
int mc_hashtbl_flush(int cur);
void mc_preload_key(int cur);
void mc_disconnect(int caller, int server);
void mc_test(int cur);
DECLARE_INTERFACE(mc_register)
DECLARE_INTERFACE(mc_set_key)
DECLARE_INTERFACE(mc_get_key)
DECLARE_INTERFACE(mc_init)
DECLARE_INTERFACE(mc_server_start)
DECLARE_INTERFACE(mc_print_status)
DECLARE_INTERFACE(mc_hashtbl_flush)
DECLARE_INTERFACE(mc_preload_key)
DECLARE_INTERFACE(mc_test)
DECLARE_INTERFACE(mc_disconnect)

static inline int
__mc_hv2node(uint32_t hv)
{ return (hv & hashmask(hashpower)) > (hashmask(hashpower) / (NUM_NODE/2)); }
#endif
#endif
