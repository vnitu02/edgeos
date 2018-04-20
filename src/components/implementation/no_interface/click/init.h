#ifndef CK_INIT_H
#define CK_INIT_H

#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <stdbool.h>

#define CK_SHM_BASE     0x80000000  /* shared memory region */
#define CK_SHM_SZ       (1<<22)     /* Shared memory mapping for each vm = 4MB */
#define CK_UNTYPED_SIZE (1<<26)     /* untyped memory per vm = 64MB */

__attribute__((weak)) char _binary_conf_file1_start = 0;
__attribute__((weak)) char _binary_conf_file1_end = 0;
__attribute__((weak)) char _binary_conf_file2_start = 0;
__attribute__((weak)) char _binary_conf_file2_end = 0;
__attribute__((weak)) char _binary_conf_file3_start = 0;
__attribute__((weak)) char _binary_conf_file3_end = 0;

struct click_init {
	char *conf_str;
	int nf_id;
};

int click_initialize(void *data);
void run_driver(void);
void run_driver_once(void);

#endif /*CK_INIT_H*/
