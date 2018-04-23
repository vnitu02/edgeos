#include "ninf.h"
#include "ninf_util.h"

#define RX_RING_SIZE    512
#define TX_RING_SIZE    512
#define MBUF_CACHE_SIZE 250

extern struct eos_ring *ninf_ft_data[EOS_MAX_FLOW_NUM];

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
};

uint8_t rss_symmetric_key[40] = { 0x6d, 0x5a, 0x6d, 0x5a,
				  0x6d, 0x5a, 0x6d, 0x5a,
				  0x6d, 0x5a, 0x6d, 0x5a,
				  0x6d, 0x5a, 0x6d, 0x5a,
				  0x6d, 0x5a, 0x6d, 0x5a,
				  0x6d, 0x5a, 0x6d, 0x5a,
				  0x6d, 0x5a, 0x6d, 0x5a,
				  0x6d, 0x5a, 0x6d, 0x5a,
				  0x6d, 0x5a, 0x6d, 0x5a,
				  0x6d, 0x5a, 0x6d, 0x5a,};

/* basicfwd.c: Basic DPDK skeleton forwarding example. */

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;
	int retval;
	uint16_t q;

	if (port >= rte_eth_dev_count()) return -1;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0) return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (retval != 0) return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
				rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0) return retval;
	}

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, nb_txd,
				rte_eth_dev_socket_id(port), NULL);
		if (retval < 0) return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0) return retval;

	/* Display the port MAC address. */
	struct ether_addr addr;
	rte_eth_macaddr_get(port, &addr);
	printc("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			(unsigned)port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	/* Enable RX in promiscuous mode for the Ethernet device. */
	rte_eth_promiscuous_enable(port);

	return 0;
}

int
rte_eth_dev_cos_setup_ports(unsigned nb_ports,
		struct rte_mempool *mp)
{
	uint8_t portid;
	for (portid = 0; portid < nb_ports; portid++) {
		if (port_init(portid, mp) != 0) {
			return -1;
		}
	}

	return 0;
}

void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	/* printc("\nChecking link status"); */
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if ((port_mask & (1 << portid)) == 0) continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status) {
					printc("Port %d Link Up - speed %u "
					       "Mbps - %s\n", (uint8_t)portid,
					       (unsigned)link.link_speed,
					       (link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					       ("full-duplex") : ("half-duplex\n"));
				} else {
					printc("Port %d Link Down\n", (uint8_t)portid);
				}
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == ETH_LINK_DOWN) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1) break;

		if (all_ports_up == 0) {
			printc(".");
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printc("done\n");
		}
	}
}

void
print_ether_addr(struct rte_mbuf *m)
{
	struct ether_hdr *eth_hdr;
	char buf[ETHER_ADDR_FMT_SIZE];

	eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
	ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, &eth_hdr->s_addr);
	printc("src MAX: %s ", buf);
	ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, &eth_hdr->d_addr);
	printc("dst MAC: %s\n" ,buf);
}

struct ipv4_hdr*
ninf_pkt_ipv4_hdr(struct rte_mbuf* pkt)
{
        struct ipv4_hdr* ipv4 = (struct ipv4_hdr*)(rte_pktmbuf_mtod(pkt, uint8_t*) + sizeof(struct ether_hdr));

        /* In an IP packet, the first 4 bits determine the version.
         * The next 4 bits are called the Internet Header Length, or IHL.
         * DPDK's ipv4_hdr struct combines both the version and the IHL into one uint8_t.
         */
        uint8_t version = (ipv4->version_ihl >> 4) & 0b1111;
        if (unlikely(version != 4)) {
                return NULL;
        }
        return ipv4;
}

struct tcp_hdr*
ninf_pkt_tcp_hdr(struct rte_mbuf* pkt)
{
        struct ipv4_hdr* ipv4 = ninf_pkt_ipv4_hdr(pkt);

        if (unlikely(ipv4 == NULL)) {  // Since we aren't dealing with IPv6 packets for now, we can ignore anything that isn't IPv4
                return NULL;
        }

        if (ipv4->next_proto_id != IP_PROTOCOL_TCP) {
                return NULL;
        }

        uint8_t* pkt_data = rte_pktmbuf_mtod(pkt, uint8_t*) + sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr);
        return (struct tcp_hdr*)pkt_data;
}

struct udp_hdr*
ninf_pkt_udp_hdr(struct rte_mbuf* pkt)
{
        struct ipv4_hdr* ipv4 = ninf_pkt_ipv4_hdr(pkt);

        if (unlikely(ipv4 == NULL)) {  // Since we aren't dealing with IPv6 packets for now, we can ignore anything that isn't IPv4
                return NULL;
        }

        if (ipv4->next_proto_id != IP_PROTOCOL_UDP) {
                return NULL;
        }

        uint8_t* pkt_data = rte_pktmbuf_mtod(pkt, uint8_t*) + sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr);
        return (struct udp_hdr*)pkt_data;
}

/*software caculate RSS*/
uint32_t
ninf_softrss(struct pkt_ipv4_5tuple *key)
{
	union rte_thash_tuple tuple;
	uint8_t rss_key_be[RTE_DIM(rss_symmetric_key)];
	uint32_t rss_l3l4;

	rte_convert_rss_key((uint32_t *)rss_symmetric_key, (uint32_t *)rss_key_be,
			    RTE_DIM(rss_symmetric_key));

	tuple.v4.src_addr = rte_be_to_cpu_32(key->src_addr);
	tuple.v4.dst_addr = rte_be_to_cpu_32(key->dst_addr);
	tuple.v4.sport = rte_be_to_cpu_16(key->src_port);
	tuple.v4.dport = rte_be_to_cpu_16(key->dst_port);

	rss_l3l4 = rte_softrss_be((uint32_t *)&tuple, RTE_THASH_V4_L4_LEN, rss_key_be);

	return rss_l3l4;
}

void
ninf_ft_init(struct ninf_ft *ft, int cnt, int entry_size)
{
        struct rte_hash* hash;
        struct rte_hash_parameters ipv4_hash_params = {
		.name = NULL,
		.entries = cnt,
		.key_len = sizeof(struct pkt_ipv4_5tuple),
		.hash_func = NULL,
		.hash_func_init_val = 0,
        };

        char s[64];
        /* create ipv4 hash table. use core number and cycle counter to get a unique name. */
        ipv4_hash_params.name = s;
        ipv4_hash_params.socket_id = rte_socket_id();
        snprintf(s, sizeof(s), "ninf_ft_%d-%"PRIu64, rte_lcore_id(), rte_get_tsc_cycles());
        hash = rte_hash_create(&ipv4_hash_params);
	assert(hash);
        ft->hash = hash;
        ft->cnt = cnt;
        ft->entry_size = entry_size;
        ft->data = (void *)ninf_ft_data;
}

int
ninf_ft_add_key(struct ninf_ft* table, struct pkt_ipv4_5tuple *key, uint32_t rss, void **data)
{
        int32_t tbl_index;

        tbl_index = rte_hash_add_key_with_hash(table->hash, (const void *)key, rss);
        assert(tbl_index >= 0);
	*data = ninf_ft_get_data(table, tbl_index);

        return tbl_index;
}

int
ninf_ft_lookup_key(struct ninf_ft* table, struct pkt_ipv4_5tuple *key, uint32_t rss, void **data)
{
        int32_t tbl_index;

        tbl_index = rte_hash_lookup_with_hash(table->hash, (const void *)key, rss);
	if (tbl_index >= 0) {
                *data = ninf_ft_get_data(table, tbl_index);
        }

	return tbl_index;
}
