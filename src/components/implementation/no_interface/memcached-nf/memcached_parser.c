/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2018 George Washington University
 *            2015-2018 University of California Riverside
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * The name of the author may not be used to endorse or promote
 *       products derived from this software without specific prior
 *       written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * memcached_parser -- scans for memcached operations and makes
 * appropriate calls to library.
 ********************************************************************/
#if 0
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_mempool.h>
#include <rte_cycles.h>
#include <rte_ring.h>
#include <rte_ethdev.h>
#include <rte_ether.h>

#include <arpa/inet.h>

//TODO: Build with memcache as a library
#ifdef LIBPCAP
#include <pcap.h>
#endif

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"
#include "onvm_flow_table.h"

#define NF_TAG "memcached"

#define PKTMBUF_POOL_NAME "MProc_pktmbuf_pool"
#define PKT_READ_SIZE  ((uint16_t)32)
#define MAX_PKT_NUM NF_QUEUE_RINGSIZE

#define MAX_KEY_SIZE 250

vaddr_t shmem_addr;
static int port;

/* Struct that contains information about this NF */
struct onvm_nf_info *nf_info;

extern struct port_info *ports;

/* number of package between each print */
static uint16_t destination;

/* Default number of packets: 128; user can modify it by -c <packet_number> in command line */
static uint32_t packet_number = 0;

static int udp_len; // 8 because of mc udp proto

/*
 * Variables needed to replay a pcap file
 */
char *pcap_filename = NULL;

// UDP header info from: https://github.com/memcached/memcached/blob/master/doc/protocol.txt#L51
// The frame header is 8 bytes long, as follows (all values are 16-bit integers
// in network byte order, high byte first):
// 0-1 Request ID
// 2-3 Sequence number
// 4-5 Total number of datagrams in this message
// 6-7 Reserved for future use; must be 0
struct mc_udp_hdr {
    uint16_t req_id;
    uint16_t seq_num;
    uint16_t tot_dgrams;
    uint16_t extra;
} __attribute__((__packed__));

struct mc_hdr {
/*
        uint8_t  magic;         // 0x80 is request, 0x81 is response
        uint8_t  opcode;        // type of command: e.g. GET is 0, SET is 1
        uint16_t key_len;
        uint8_t extras_len;    // specifies length of command specific extras
	uint8_t data_type;
        uint16_t status;        // only used in responses, otherwise RESERVED
        uint32_t body_len;      // key length + value length
        uint32_t opaque;        // unmodified bytes passed btwn client and server
        uint64_t cas;           // data version checking (optional)
*/
	char command[4];
	char key[64];
	uint16_t flags;
	uint16_t expir;
	uint16_t length;
	char value[1024];
} __attribute__((__packed__));

struct mc_pkt {
        struct  mc_hdr hdr;
        uint8_t *extras;
        char    *key;
        char    *value;
};

char op_name[16];
void print_mc_hdr(struct mc_hdr*);

/* TODO: Declare internal function prototypes here, reorder functions with main on top
 * Wait to do these changes until just before the PR because repo masters may not like it */

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage: %s [EAL args] -- [NF_LIB args] -- -d <destination> [-p <print_delay>] "
                        "[-a] [-s <packet_length>] [-m <dest_mac_address>] [-o <pcap_filename>] "
                        "[-c <packet_number>]\n\n", progname);
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c, dst_flag = 0;

        while ((c = getopt(argc, argv, "d:o:c:")) != -1) {
                switch (c) {
                case 'd':
                        destination = strtoul(optarg, NULL, 10);
                        dst_flag = 1;
                        break;
                case 'o':
#ifdef LIBPCAP
                        pcap_filename = strdup(optarg);
                        break;
#else
                        rte_exit(EXIT_FAILURE, "To enable pcap replay follow the README "
                                "instructions\n");
                        break;
#endif
                case 'c':
                        packet_number = strtoul(optarg, NULL, 10);
                        if (packet_number <= 0 || packet_number > MAX_PKT_NUM) {
                                RTE_LOG(INFO, APP, "Illegal packet number(1 ~ %u) %u!\n",
                                        MAX_PKT_NUM, packet_number);
                                return -1;
                        }
                        break;
                case '?':
                        usage(progname);
                        if (optopt == 'd')
                                RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                        else if (optopt == 'o')
                                RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                        else if (optopt == 'c')
                                RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                        else if (isprint(optopt))
                                RTE_LOG(INFO, APP, "Unknown option `-%c'.\n", optopt);
                        else
                                RTE_LOG(INFO, APP, "Unknown option character `\\x%x'.\n", optopt);
                        return -1;
                default:
                        usage(progname);
                        return -1;
                }
        }

        if (!dst_flag) {
                RTE_LOG(INFO, APP, "Speed tester NF requires a destination NF with the -d flag.\n");
                return -1;
        }

        return optind;
}

/*
 * This function displays stats. It uses ANSI terminal codes to clear
 * screen when called. It is called from a single non-master
 * thread in the server process, when the process is run with more
 * than one lcore enabled.
 */
#if 0
static void
do_stats_display(struct rte_mbuf* pkt) {
        // TODO: Make stats display relevant to memcached
        static uint64_t last_cycles;
        static uint64_t cur_pkts = 0;
        static uint64_t last_pkts = 0;
        const char clr[] = { 27, '[', '2', 'J', '\0' };
        const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };
        (void)pkt;

        uint64_t cur_cycles = rte_get_tsc_cycles();
        //cur_pkts += print_delay;

        /* Clear screen and move to top left */
        printf("%s%s", clr, topLeft);

        printf("Total packets: %9"PRIu64" \n", cur_pkts);
        printf("TX pkts per second: %9"PRIu64" \n", (cur_pkts - last_pkts)
                * rte_get_timer_hz() / (cur_cycles - last_cycles));
        printf("Initial packets created: %u\n", packet_number);

        last_pkts = cur_pkts;
        last_cycles = cur_cycles;

        printf("\n\n");
}
#endif
/*
char *
get_opcode_name(uint8_t opcode) {
        switch (opcode) {
                case 0:
                        strncpy(op_name, "GET", sizeof(op_name));
                        break;
                case 1:
                        strncpy(op_name, "SET", sizeof(op_name));
                        break;
                default:
                        strncpy(op_name, "UNKNOWN", sizeof(op_name));
        }
        return op_name;
}
*/

void
print_mc_hdr(struct mc_hdr *hdr) {
	printf("Command %s\n", hdr->command);
	printf("Key %s\n", hdr->key);      /*
	printf("0x%" PRIu8 " [magic]\n", hdr->magic);
        printf("%" PRIu8 " [opcode]\n", hdr->opcode);
        printf("%" PRIu16 " [key length]\n", rte_be_to_cpu_16(hdr->key_len));
        printf("%" PRIu8 " [extras length]\n", hdr->extras_len);
        printf("%" PRIu8 " [data type]\n", hdr->data_type);
        printf("%" PRIu16 " [status]\n", rte_be_to_cpu_16(hdr->status));
        printf("%" PRIu32 " [body length]\n", rte_be_to_cpu_32(hdr->body_len));
        printf("%" PRIu32 " [opaque]\n", rte_be_to_cpu_32(hdr->opaque));
        printf("%" PRIu64 " [cas]\n", rte_be_to_cpu_64(hdr->cas));
*/
}

/*
static int
is_memcached_proto(struct rte_mbuf *pkt, char ** data_p) {
         //TODO: implement memcached proto checking 
         // it will be slow because packet data access
         // however, by checking UDP first we can optimize a little 
         return 0;
} 
*/

static void
reform_response(char *pkt_data, char *key, int key_len)
{
	int inc;
	inc = strlen("VALUE ");
	strncpy(pkt_data, "VALUE ", inc);
	pkt_data += inc;
	udp_len += inc;

	strncpy(pkt_data, key, key_len);
	pkt_data += key_len;
	udp_len += key_len;

	// flag = 0, byte length = 100 and cas = 0
	inc = strlen("0 100\r\n") + 1;
	strncpy(pkt_data, " 0 100\r\n", inc);
	pkt_data += inc;
	udp_len += inc;

	memset(pkt_data, ' ', 8);
	memset(pkt_data + 8, 0, 100 - 8);
	pkt_data += 100;
	udp_len += 100;
	
	inc = strlen("\r\nEND\r\n");
	strncpy(pkt_data, "\r\nEND\r\n", inc);
	udp_len += inc;
	//printf("Reformed data:\n %s\n", pkt_data);
}

//struct mc_hdr *
static int
parse_mc_pkt(struct rte_mbuf *pkt, char** pkt_data_p, char *ret_key, int *keylen_p) {
	/*
	uint32_t payload_len = rte_pktmbuf_pkt_len(pkt) - sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) - sizeof(struct udp_hdr);
	printf("Len ether %lu, ipv4 %lu, udp %lu\n", sizeof(struct ether_hdr), sizeof(struct ipv4_hdr), sizeof(struct udp_hdr));
	*/

	/* skip ether, ipv4, udp, and the 8 bytes header */
	// struct mc_hdr *req = (struct mc_hdr *) (rte_pktmbuf_mtod(pkt, uint8_t*)
	// 			+ sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) 
	// 	         	+ sizeof(struct udp_hdr))
	// 			+ 8;
    struct mc_hdr req;
    int key_len = 0;
    struct mc_udp_hdr* udp_hdr = (struct mc_udp_hdr*) (rte_pktmbuf_mtod(pkt, uint8_t*)
             + sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) 
             + sizeof(struct udp_hdr));
    // This doesn't print right, I think because of network ordering issues
    //udp_hdr->req_id = ntohs(udp_hdr->req_id);
    //udp_hdr->seq_num = ntohs(udp_hdr->seq_num);
    //udp_hdr->tot_dgrams = ntohs(udp_hdr->tot_dgrams);
    printf("id=%" PRIu16 ",seq num=%" PRIu16 ", dgrams=%" PRIu16 " \n", ntohs(udp_hdr->req_id), ntohs(udp_hdr->seq_num), ntohs(udp_hdr->tot_dgrams));
    int i;
/*
    for (i = 0; i < sizeof(struct mc_udp_hdr); i++) {
	printf ("mchdr: %d %c\n", ((char *)udp_hdr)[i], ((char *)udp_hdr)[i]); 
    }
*/
    char* ascii_req = (char*) (rte_pktmbuf_mtod(pkt, uint8_t*)
             + sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr) 
             + sizeof(struct udp_hdr)) + sizeof(struct mc_udp_hdr);
    printf("request string:\n%s***", ascii_req);
    if(ascii_req[0] == 'g') {
        printf("Get request\n");
        strcpy(req.command, "GET");
    }
    else if(ascii_req[0] == 's') {
        printf("Set request\n");
        strcpy(req.command, "SET");
    }
    else if(ascii_req[0] == 'V' || ascii_req[0] == 'v') {
	printf("Server response: Won't be forwarded, for debugging\n");
	return;
/*
	while (*ascii_req != '\n') ascii_req++;
	ascii_req++;
	while(strncmp(ascii_req, "END\r\n", sizeof("END\r\n"))) {
		printf("%c ", *ascii_req);
		ascii_req++;
        }
	return;
*/
    }

    else {
        printf("Not get or set or response\n");
        return;
    }
    
    *pkt_data_p = ascii_req;

    /* Step through request until next space (ignore first 4 characters 'GET/SET '*/
    for(key_len=0; key_len < MAX_KEY_SIZE; key_len++) {
        if(ascii_req[key_len+4] == ' ' || ascii_req[key_len+4] == '\r') {
            break;
        }
    }
    strncpy(req.key, ascii_req + 4, key_len);
    strncpy(ret_key, ascii_req + 4, key_len);
    req.key[key_len] = '\0';
    *keylen_p = key_len;

    print_mc_hdr(&req);
   return 0;
}

static int
packet_handler(struct rte_mbuf* pkt, struct onvm_pkt_meta* meta) {
/*
        static uint32_t counter = 0;


        if (counter++ == print_delay) {
                do_stats_display(pkt);
                counter = 0;
        }
*/

/*
        if (!is_memcached_proto(pkt, &data)) {
                meta->action = ONVM_NF_ACTION_DROP;
        } else {
*/
                /* one of our memcached packets to send to make calls */
	char *pkt_data;
	char key[1024];
	int key_len;

        meta->destination = destination;
        meta->action = ONVM_NF_ACTION_TONF;
	if (onvm_pkt_is_udp(pkt)) {
       		parse_mc_pkt(pkt, &pkt_data, key, &key_len);
		//testing response to mcblaster
		
	        if(pkt_data[0] == 'g') {
			/*
			printf("buf size %d \n", pkt->buf_len);
			printf("RTE_PKTMBUF_APPEND %p\n", append);
			printf("buf size %d \n", pkt->buf_len);
			*/
			struct udp_hdr *udp;
			udp = onvm_pkt_udp_hdr(pkt);
			udp_len = 16;
    			reform_response(pkt_data, key, key_len);
			char * append = rte_pktmbuf_append(pkt, udp_len - ntohs(udp->dgram_len));
		
			printf("Responding to 'GET'\n");
			onvm_pkt_swap_src_mac_addr(pkt, pkt->port, ports);
			struct ipv4_hdr *ip_hdr;
			uint32_t tmp;
			uint16_t tmp_udp;
			ip_hdr = onvm_pkt_ipv4_hdr(pkt);
			tmp = ip_hdr->src_addr;
			ip_hdr->src_addr = ip_hdr->dst_addr;
			ip_hdr->dst_addr = tmp;
			printf("UDP Ports: src %d dst %d\n", 
				ntohs(udp->src_port), ntohs(udp->dst_port)); 
			tmp_udp = udp->src_port;
			udp->src_port = udp->dst_port;
			udp->dst_port = tmp_udp;

			//udp->dgram_len = htons(udp_len);
			printf("dg size %d \n", ntohs(udp->dgram_len));
			printf("ip size %d \n", ntohs(ip_hdr->total_length));
			udp->dgram_len = htons(udp_len);
			ip_hdr->total_length = htons(20 + udp_len);
			//ip_hdr->total_length = htons(ntohs(ip_hdr->total_length + 105));
			
			//ip_hdr->total_length += 100;
			onvm_pkt_set_checksums(pkt);
			meta->destination = pkt->port;
			meta->action = ONVM_NF_ACTION_OUT;	
    		}
		
	}
	printf("\n");
  //      }
        return 0;
}

int main(int argc, char *argv[]) {
        int arg_offset;

        const char *progname = argv[0];

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG)) < 0)
                return -1;

        argc -= arg_offset;
        argv += arg_offset;

        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop();
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }

        onvm_nflib_run(nf_info, &packet_handler);
        printf("If we reach here, program is ending\n");
        return 0;
}
#endif
