/* Use the passed in stack in arg4 */

#define __ASM__
#include <consts.h>
#include <cos_asm_simple_stacks.h>

.text
.globl __inv_mng_agent
.type  __inv_mng_agent, @function
__inv_mng_agent:
	COS_ASM_GET_STACK

	pushl %ebp /* arg 4 */
    pushl %edi /* arg 3 */
    pushl %esi /* arg 2 */
    pushl %ebx /* arg 1 */
	call mng_agent
    movl %eax, %ecx

	COS_ASM_RET_STACK
	movl $RET_CAP, %eax
	sysenter;

.globl __inv_next_call
.type  __inv_next_call, @function
__inv_next_call:
	COS_ASM_GET_STACK

	pushl %ebp /* arg 4 */
    pushl %edi /* arg 3 */
    pushl %esi /* arg 2 */
    pushl %ebx /* arg 1 */
	call run_driver
    movl %eax, %ecx

	COS_ASM_RET_STACK
	movl $RET_CAP, %eax
	sysenter;
