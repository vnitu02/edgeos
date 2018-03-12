#include<llboot.h>

#include"init.h"

extern vaddr_t cos_upcall_entry;

/*
 * the ID of this click instance
 */
nfid_t this_nf_id = 0;

/*
 * The index of the conf file to be loaded
 */
unsigned int conf_file_idx = 0;

#define MAX_CONF_FILES 2
char *conf_files[MAX_CONF_FILES];

static void 
setup_conf_files(void)
{
       conf_files[0] = &_binary_conf_file1_start;
       conf_files[1] = &_binary_conf_file2_start;

       /*
       * Mark the end of the configuration file.
       * Without this the Click parser can continue on the 
       * next configuration file if they are liked contiguously.
       */
       *(char *)(&_binary_conf_file1_end - 1) = '\0';
       *(char *)(&_binary_conf_file2_end - 1) = '\0';
}

void 
cos_init(void *args)
{
       //unsigned long i;
       struct click_init init_data;

       setup_conf_files();

       llboot_comp_confidx_get(&conf_file_idx);

       init_data.conf_str = conf_files[conf_file_idx];
       init_data.nf_id = 0;

       //for (i = &_binary_conf_file1_start; i <= &_binary_conf_file1_end; i++)
       //       printc("%c", *(char *)i);

       click_main(&init_data);
}
