#include <eos_pkt.h>
#include <nf_hypercall.h>
#include "memcached.h"

static vaddr_t shmem_addr;
static int port;

void *
eos_get_packet(int *len)
{
       int err, c = 0;
       void *pkt;
       struct eos_ring *input_ring = get_input_ring((void *)shmem_addr);
       struct eos_ring *ouput_ring = get_output_ring((void *)shmem_addr);

       eos_pkt_collect(input_ring, ouput_ring);
       pkt = eos_pkt_recv(input_ring, len, &port, &err);
       printc("1\n");
       while (!pkt) {
              printc("2\n");
              nf_hyp_block();
              printc("3\n");
              pkt = eos_pkt_recv(input_ring, len, &port, &err);
              printc("4\n");
       }
       return pkt;
}

int
eos_send_packet(void *pdata, int len)
{
       struct eos_ring *output_ring = get_output_ring((void *)shmem_addr);
       int r;

       r = eos_pkt_send(output_ring, pdata, len, port);
       while (r) {
              nf_hyp_block();
              r = eos_pkt_send(output_ring, pdata, len, port);
       }
       return r;
}

void
cos_init(void *args)
{
       void *pkt;
       int len;

       nf_hyp_get_shmem_addr(&shmem_addr); 
       printf ("shmem %p\n", shmem_addr);

       pkt = eos_get_packet(&len);
       printf ("received packet length: %d\n", len);

       hash_init(JENKINS_HASH);
       assoc_init(0, 0);
}

