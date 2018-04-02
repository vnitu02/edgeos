C_OBJS=
ASM_OBJS=
COMPONENT=clickos.o
INTERFACES=
DEPENDENCIES=
IF_LIB=conf1.o conf2.o
ADDITIONAL_LIBS=-lclick -lcobj_format -lcos_kernel_api $(POSIX_LIB) -lcos_defkernel_api -lstdc++ -lsupc++

pre: conf1.o conf2.o all

conf1.o: conf_file1
	ld -r -b binary -o conf1.o conf_file1
	
conf2.o: conf_file2
	ld -r -b binary -o conf2.o conf_file2

include ../../Makefile.subsubdir
MANDITORY_LIB=simple_stklib.o
