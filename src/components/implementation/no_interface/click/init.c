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

#define MAX_CONF_FILES 3
char *conf_files[MAX_CONF_FILES];

static void 
setup_conf_files(void)
{
       conf_files[0] = &_binary_conf_file1_start;
       conf_files[1] = &_binary_conf_file2_start;
       conf_files[2] = &_binary_conf_file3_start;

       /*
       * Mark the end of the configuration file.
       * Without this the Click parser can continue on the 
       * next configuration file if they are liked contiguously.
       */
       *(char *)(&_binary_conf_file1_end - 1) = '\0';
       *(char *)(&_binary_conf_file2_end - 1) = '\0';
       *(char *)(&_binary_conf_file3_end - 1) = '\0';
}

void 
cos_init(void *args)
{
       struct click_init init_data;

       nf_hyp_confidx_get(&conf_file_idx);
       nf_hyp_get_shmem_addr(&shmem_addr);

       /*
       * idx=-1 means that the conf file is already parsed
       * (fork from template)
       */
       if (conf_file_idx == -1) {
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
