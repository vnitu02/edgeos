#ifndef NINF_H
#define NINF_H

#include <stdio.h>
#include <string.h>

#include <cos_debug.h>
#include <llprint.h>
#include "pci.h"
#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>

void ninf_init(void);
void ninf_rx_loop(void);
void ninf_tx_loop(void);
void ninf_rx_init(void);
void ninf_tx_init(void);

#endif /* NINF_H */
