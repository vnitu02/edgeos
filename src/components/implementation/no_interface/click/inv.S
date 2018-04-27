#include <cos_asm_server_stub_simple_stack.h>

.text
.globl click_shmem_inv
.type  click_shmem_inv, @function
.align 16
click_shmem_inv:
       movl $cos_static_stack, %esp
       movl %eax, %edx
       andl $0xffff, %eax
       shr $16, %edx
       pushl %edx
       pushl %eax
       pushl %ecx

       pushl %ebp
       xor %ebp, %ebp
       pushl %edi
       pushl %esi
       call click_shmem
       addl $16, %esp

       movl %eax, %ecx
       movl $RET_CAP, %eax
       COS_ASM_RET_STACK

       sysenter
