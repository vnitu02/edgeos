C_OBJS=
ASM_OBJS=
COMPONENT=clickos.o
INTERFACES=
DEPENDENCIES=
IF_LIB=conf1.o conf2.o conf3.o conf4.o conf5.o
ADDITIONAL_LIBS=-lclick -lcobj_format -lcos_kernel_api $(POSIX_HYPERCALL_LIB) -lcos_defkernel_api -lstdc++ -lsupc++

pre: conf1.o conf2.o conf3.o conf4.o conf5.o all

conf1.o: conf_file1
	ld -r -b binary -o conf1.o conf_file1
	
conf2.o: conf_file2
	ld -r -b binary -o conf2.o conf_file2

conf3.o: conf_file3
	ld -r -b binary -o conf3.o conf_file3

conf4.o: conf_file4
	ld -r -b binary -o conf4.o conf_file4

conf5.o: conf_file5
	ld -r -b binary -o conf5.o conf_file5

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
