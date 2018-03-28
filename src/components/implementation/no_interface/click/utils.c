#include <cos_kernel_api.h>
#include <cos_types.h>

/*
 * sinv to another click component
 */
void 
next_call_sinv(void)
{
       word_t unused1 = 0, unused2 = 0;

       cos_sinv_3rets(BOOT_CAPTBL_FREE, 0, 0, 0, 0, &unused1, &unused2);
}
