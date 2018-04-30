#!/bin/sh

cp fwp_mng.o llboot.o
./cos_linker "llboot.o, ;simple-fwp.o, :" ./gen_client_stub
