#!/bin/sh

cp fwp_mng.o llboot.o
./cos_linker "llboot.o, ;clickos.o, :" ./gen_client_stub
