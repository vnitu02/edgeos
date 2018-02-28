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

#define MAX_CONF_FILES 1
char *conf_files[MAX_CONF_FILES];

static void 
setup_conf_files(void)
{
       conf_files[0] = &_binary_conf_file1_start;
}

void 
cos_init(void *args)
{
       struct click_init init_data;

       setup_conf_files();

       init_data.conf_str = conf_files[conf_file_idx];
       init_data.nf_id = 0;

       printc("cos_init\n");
       click_main(&init_data);
}
