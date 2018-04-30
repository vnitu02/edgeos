#include<nf_hypercall.h>

#include"init.h"

extern vaddr_t cos_upcall_entry;

/*
 * the ID of this click instance
 */
nfid_t this_nf_id = 0;

/*
 * The index of the conf file to be loaded
 */
int conf_file_idx = 0;
vaddr_t shmem_addr;
int packet_ptr;
int packet_length;
int packet_port;
int global_first = 0;

#define MAX_CONF_FILES 5
char *conf_files[MAX_CONF_FILES];

void
click_block()
{
	nf_hyp_block();
}

int
click_shmem(int packet_p, int packet_l, int port_l)
{
       /*FIXME an ugly hack to pass a packet to the click code*/
	if (!global_first) {
		global_first = 1;
		nf_hyp_get_shmem_addr(&shmem_addr);
	}
       packet_ptr    = packet_p;
       packet_length = packet_l;
       packet_port   = port_l;
       run_driver_once();
       packet_ptr    = NULL;
       packet_length = 0;
       packet_port   = -1;
}

static void 
setup_conf_files(void)
{
       conf_files[0] = &_binary_conf_file1_start;
       conf_files[1] = &_binary_conf_file2_start;
       conf_files[2] = &_binary_conf_file3_start;
       conf_files[3] = &_binary_conf_file4_start;
       conf_files[4] = &_binary_conf_file5_start;

       /*
       * Mark the end of the configuration file.
       * Without this the Click parser can continue on the 
       * next configuration file if they are liked contiguously.
       */
       *(char *)(&_binary_conf_file1_end - 1) = '\0';
       *(char *)(&_binary_conf_file2_end - 1) = '\0';
       *(char *)(&_binary_conf_file3_end - 1) = '\0';
       *(char *)(&_binary_conf_file4_end - 1) = '\0';
       *(char *)(&_binary_conf_file5_end - 1) = '\0';
}

void 
cos_init(void *args)
{
       struct click_init init_data;

       nf_hyp_measure_activate();
       nf_hyp_confidx_get(&conf_file_idx);
       nf_hyp_get_shmem_addr(&shmem_addr);

       /*
       * idx=-1 means that the conf file is already parsed
       * (fork from template)
       */
       if (conf_file_idx == -1) {
	       //printc("dbg clkc conf %d shem %x\n", conf_file_idx, shmem_addr);
              run_driver();
       } else {
              setup_conf_files();
              init_data.conf_str = conf_files[conf_file_idx];
              init_data.nf_id = cos_spd_id();

              click_initialize(&init_data);
              nf_hyp_checkpoint(init_data.nf_id);
              //run the configuration file once
              run_driver();
              //nf_hyp_clean();
       }
}
