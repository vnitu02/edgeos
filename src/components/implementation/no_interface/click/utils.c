#include <cos_kernel_api.h>
#include <cos_types.h>

/*
 * sinv to another click component
 */
void 
next_call_sinv(void)
{
       cos_sinv(BOOT_CAPTBL_FREE, 0, 0, 0, 0);
}
