#ifndef NINF_UTIL_H
#define NINF_UTIL_H

#include <cos_kernel_api.h>
#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_mempool.h>
#include <rte_cycles.h>
#include <rte_ring.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_thash.h>
#include <rte_hash.h>
#include "eos_utils.h"
#include "eos_ring.h"

#define NUM_NIC_PORTS 2
#define NUM_MBUFS 4096
#define BURST_SIZE 32
#define IP_PROTOCOL_TCP 6
#define IP_PROTOCOL_UDP 17

struct pkt_ipv4_5tuple {
        uint32_t src_addr;
        uint32_t dst_addr;
        uint16_t src_port;
        uint16_t dst_port;
        uint8_t  proto;
};

struct ninf_ft {
        struct rte_hash* hash;
        char* data;
        int cnt;
        int entry_size;
};

int dpdk_init(void);
void check_all_ports_link_status(uint8_t port_num, uint32_t port_mask);
/* 
 * Init ports!
 * */
int rte_eth_dev_cos_setup_ports(unsigned nb_ports, struct rte_mempool *mp);
void print_ether_addr(struct rte_mbuf *m);

struct tx_ring * ninf_tx_add_ring(struct eos_ring *r);
void ninf_tx_del_ring(struct tx_ring *r);
struct ipv4_hdr *ninf_pkt_ipv4_hdr(struct rte_mbuf* pkt);
struct tcp_hdr *ninf_pkt_tcp_hdr(struct rte_mbuf* pkt);
struct udp_hdr *ninf_pkt_udp_hdr(struct rte_mbuf* pkt);
uint32_t ninf_rss();
void ninf_ft_init(struct ninf_ft *ft, int cnt, int entry_size);
int ninf_ft_add_key(struct ninf_ft* table, struct pkt_ipv4_5tuple *key, uint32_t rss, void **data);
int ninf_ft_lookup_key(struct ninf_ft* table, struct pkt_ipv4_5tuple *key, uint32_t rss, void **data);

static inline void *
ninf_ft_get_data(struct ninf_ft* table, int32_t index)
{
        return &table->data[index*table->entry_size];
}

static inline int
ninf_pkt_is_ipv4(struct rte_mbuf* pkt)
{
        return ninf_pkt_ipv4_hdr(pkt) != NULL;
}

static inline int
static inline int
ninf_fill_key(struct pkt_ipv4_5tuple *key, struct rte_mbuf *pkt)
{
        struct ipv4_hdr *ipv4_hdr;
        struct tcp_hdr *tcp_hdr;
        struct udp_hdr *udp_hdr;

        ipv4_hdr = ninf_pkt_ipv4_hdr(pkt);
        memset(key, 0, sizeof(struct pkt_ipv4_5tuple));
        key->proto  = ipv4_hdr->next_proto_id;
        key->src_addr = ipv4_hdr->src_addr;
        key->dst_addr = ipv4_hdr->dst_addr;
        if (key->proto == IP_PROTOCOL_TCP) {
                tcp_hdr = ninf_pkt_tcp_hdr(pkt);
                key->src_port = tcp_hdr->src_port;
                key->dst_port = tcp_hdr->dst_port;
        } else if (key->proto == IP_PROTOCOL_UDP) {
                udp_hdr = ninf_pkt_udp_hdr(pkt);
                key->src_port = udp_hdr->src_port;
                key->dst_port = udp_hdr->dst_port;
	} else {
                key->src_port = 0;
                key->dst_port = 0;
        }
        return 0;
}

static inline int
ninf_fill_key_symmetric(struct pkt_ipv4_5tuple *key, struct rte_mbuf *pkt)
{
        if (ninf_fill_key(key, pkt) < 0) {
                assert(0);
        }

	if (key->dst_addr > key->src_addr) {
                uint32_t temp = key->dst_addr;
                key->dst_addr = key->src_addr;
                key->src_addr = temp;
	}

	if (key->dst_port > key->src_port) {
                uint16_t temp = key->dst_port;
                key->dst_port = key->src_port;
                key->src_port = temp;
	}

	return 0;
}

#endif /* NINF_UTIL_H */
