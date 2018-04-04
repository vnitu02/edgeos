#include <stdio.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <cos_defkernel_api.h>
#include <llprint.h>
#include <syscall.h>
#include <stdbool.h>
#include <errno.h>
#include <nf_hypercall.h>

#define SYSCALLS_NUM 378

extern int click_instance_id;

typedef long (*cos_syscall_t)(long a, long b, long c, long d, long e, long f);
cos_syscall_t cos_syscalls[SYSCALLS_NUM];

static void
libc_syscall_override(cos_syscall_t fn, int syscall_num)
{
       printc("Overriding syscall %d\n", syscall_num);
       cos_syscalls[syscall_num] = fn;
}

int
cos_open(const char *pathname, int flags, int mode)
{
       printc("open not implemented!\n");
       return -ENOTSUP;
}

int
cos_close(int fd)
{
       printc("close not implemented!\n");
       return -ENOTSUP;
}

ssize_t
cos_read(int fd, void *buf, size_t count)
{
       printc("read not implemented!\n");
       return -ENOTSUP;
}

ssize_t
cos_readv(int fd, const struct iovec *iov, int iovcnt)
{
       printc("readv not implemented!\n");
       return -ENOTSUP;
}

ssize_t
cos_write(int fd, const void *buf, size_t count)
{
       if (fd == 0) {
              printc("stdin is not supported!\n");
              return -ENOTSUP;
       } else if (fd == 1 || fd == 2) {
              unsigned int i;
              char *d = (char *)buf;
              for(i=0; i<count; i++) printc("%c", d[i]);
              return count;
       } else {
              printc("write not implemented!\n");
              return -ENOTSUP;
       }
}

ssize_t
cos_writev(int fd, const struct iovec *iov, int iovcnt)
{
       int i;
       ssize_t ret = 0;
       for(i=0; i<iovcnt; i++) {
              ret += cos_write(fd, (const void *)iov[i].iov_base, iov[i].iov_len);
       }
       return ret;
}

long
cos_ioctl(int fd, void *unuse1, void *unuse2)
{
       /* musl libc does some ioctls to stdout, so just allow these to silently go through */
       if (fd == 1 || fd == 2) {
              return 0;
       } else {
              printc("ioctl not implemented!\n");
              return -ENOTSUP;
       }
}

ssize_t
cos_brk(void *addr)
{
       /* musl libc tries to use brk to expand heap in malloc. But if brk fails, it 
          turns to mmap. So this fake brk always fails, force musl libc to use mmap */
       return 0;
}

void *
cos_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
       void *ret=0;

       if (addr != NULL) {
              printc("parameter void *addr is not supported!\n");
              errno = ENOTSUP;
              return MAP_FAILED;
       }
       if (fd != -1) {
              printc("file mapping is not supported!\n");
              errno = ENOTSUP;
              return MAP_FAILED;
       }

       nf_hyp_malloc(length, (vaddr_t *)&addr);
       if (!addr){
              ret = (void *) -1;
       } else {
              ret = addr;
       }

       if (ret == (void *)-1) {  /* return value comes from man page */
              printc("mmap() failed!\n");
              /* This is a best guess about what went wrong */
              errno = ENOMEM;
       }
       return ret;
}

int
cos_munmap(void *start, size_t length)
{
       printc("munmap not implemented!\n");
       return -ENOTSUP;
}

int
cos_madvise(void *start, size_t length, int advice)
{
       /* musl libc use madvise in free. Again allow these to silently go through */
       return 0;
}

void *
cos_mremap(void *old_address, size_t old_size, size_t new_size, int flags)
{
       printc("mremap not implemented!\n");
       return MAP_FAILED;
}

off_t
cos_lseek(int fd, off_t offset, int whence)
{
       printc("lseek not implemented!\n");
       return -ENOTSUP;
}

void
pre_syscall_default_setup()
{
       printc("pre_syscall_default_setup\n");

       struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
       struct cos_compinfo    *ci    = cos_compinfo_get(defci);

       cos_defcompinfo_init();
       cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
}

void
syscall_emulation_setup(void)
{
       printc("syscall_emulation_setup\n");

       int i;
       for (i = 0; i < SYSCALLS_NUM; i++) {
              cos_syscalls[i] = 0;
       }

       libc_syscall_override((cos_syscall_t)cos_write, __NR_write);
       libc_syscall_override((cos_syscall_t)cos_writev, __NR_writev);
       libc_syscall_override((cos_syscall_t)cos_ioctl, __NR_ioctl);
       libc_syscall_override((cos_syscall_t)cos_brk, __NR_brk);
       libc_syscall_override((cos_syscall_t)cos_mmap, __NR_mmap);
       libc_syscall_override((cos_syscall_t)cos_mmap, __NR_mmap2);
       libc_syscall_override((cos_syscall_t)cos_munmap, __NR_munmap);
       libc_syscall_override((cos_syscall_t)cos_madvise, __NR_madvise);
       libc_syscall_override((cos_syscall_t)cos_mremap, __NR_mremap);
}

long
cos_syscall_handler(int syscall_num, long a, long b, long c, long d, long e, long f)
{
       assert(syscall_num <= SYSCALLS_NUM);
       /* printc("Making syscall %d\n", syscall_num); */
       if (!cos_syscalls[syscall_num]){
              printc("WARNING: Component %ld calling unimplemented system call %d\n", cos_spd_id(), syscall_num);
              //assert(0);
              return 0;
       } else {
              return cos_syscalls[syscall_num](a, b, c, d, e, f);
       }
}

void
libc_initialization_handler()
{
       printc("libc_init\n");
}
