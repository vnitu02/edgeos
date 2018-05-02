#include<nf_hypercall.h>

int i=0;

void
cos_init(void *args)
{
       i=1;
       nf_hyp_measure_activate();
       nf_hyp_block();
       while(1){}
}
