#!/bin/sh

cp fwp_mng.o llboot.o
cp clickos.o _0_clickos.o
./cos_linker "llboot.o, ;_0_clickos.o, :" ./gen_client_stub
