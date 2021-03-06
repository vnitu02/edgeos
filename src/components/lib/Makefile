include Makefile.src Makefile.comp

LIB_OBJS=heap.o cobj_format.o cos_kernel_api.o cos_defkernel_api.o
LIBS=$(LIB_OBJS:%.o=%.a)
MANDITORY=c_stub.o cos_asm_upcall.o cos_asm_ainv.o cos_component.o
MAND=$(MANDITORY_LIB)
SIMPLE_STACKS=c_stub.o cos_asm_upcall_simple_stacks.o cos_asm_ainv.o cos_component.o
SIMPLE_STKLIB=simple_stklib.o
SIMPLE_STACKS_NF=c_stub.o cos_asm_upcall_simple_stacks_NF.o cos_asm_ainv.o cos_component.o
SIMPLE_STKLIB_NF=simple_stklib_NF.o

CINC_ENV=$(CINC)
export CINC_ENV

.PHONY: all sl ps ck
all: $(LIBS) $(MAND) $(SIMPLE_STKLIB) $(SIMPLE_STKLIB_NF) sl

# we have to compile these without dietlibc so that there are not
# symbol conflicts and this is why we have the %.a here and don't
# output a .a file.
%.a:%.c
	$(info |     [CC]   Creating library file $@ from $^)
	@$(CC) $(CFLAGS) $(CINC) -o $(@:%.a=%.o) -c $<
	@$(AR) cr lib$@ $(@:%.a=%.o)

$(MAND): $(MANDITORY)
	@$(LD) $(LDFLAGS) -r -o $@ $^

$(SIMPLE_STKLIB): $(SIMPLE_STACKS)
	@$(LD) $(LDFLAGS) -r -o $@ $^

$(SIMPLE_STKLIB_NF): $(SIMPLE_STACKS_NF)
	@$(LD) $(LDFLAGS) -r -o $@ $^

sl:
	make $(MAKEFLAGS) -C sl

%.o:%.S
	$(info |     [AS]   Compiling assembly file $^ into $@)
	@$(AS) $(ASFLAGS) $(CINC) -c -o $@ $^

%.o:%.c
	$(info |     [CC]   Compiling C file $^ into $@)
	@$(CC) $(CFLAGS) $(CINC) -o $@ -c $^

clean:
	$(info |     [RM]   Cleaning up directory)
	@rm -f a.out *.o *.a *.d *~
	make -C sl clean

distclean:
	make -C musl-1.1.11 distclean
# keep the following commands in one line. make executes each line
# with a new shell.
	make -C posix clean
	make -C libcxx clean
	make -C ck uninstall
	make -C click clean
	 make -C dpdk clean

musl:
	cd musl-1.1.11; ./configure "CFLAGS=-m32 -O3" "LDFLAGS=-Wl,-melf_i386" --disable-shared --target=i386; cd ..
	make -C musl-1.1.11
	make -C musl-1.1.11 install

ps:
	cd ps; ./configure cos x86 general; cd ..; make -C ps

ck:
	make -C ck all
	make -C ck install

init: clean distclean musl ck ps all
# keep the following commands in one line. Same as above.
	make -C posix
	make -C libcxx
	make -C click
	cd dpdk && make config T=i686-native-cosapp-gcc && make V=y && cp build/lib/* ../ #&& cp -av build/include/rte_eal.h ../../include/rte_eal.h && cp -av build/include/rte_ring.h ../../include/rte_ring.h
