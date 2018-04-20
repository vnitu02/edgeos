BUILDDIR := build
INCLUDE := -I. -I./include -I../../include -I../musl-1.1.11/include -I../libcxx/include -I../libcxx/libstdc++-v3-4.8/include -I../../implementation/no_interface/fwp_manager -I../../../kernel/include/shared -I../ck/ck/include -I../ps
CXXFLAGS = $(INCLUDE) -O3 -fno-exceptions
OBJ_CC = $(CLICK_ALL)

include Makefile22

.PHONY: all clean cp dir

all: dir libclick.a

clean:
	@-rm -r $(BUILDDIR)

%.o:%.cc
	$(CXX) $(CXXFLAGS) -o $@ -c $<

%.o:%.c
	$(CC) $(CXXFLAGS) -o $@ -c $<

dir:
	@mkdir -p $(BUILDDIR)

libclick.a: $(OBJ_CC)
	@$(AR) rcs $@ $^
	#$(LD) -r -o $@ $^
	@mv $^ $(BUILDDIR)
	@mv $@ ../
