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
#include "eos_ring.h"

#undef assert
/* On assert, immediately switch to the "exit" thread */
#define assert(node)                                       \
	do {                                               \
		if (unlikely(!(node))) {                   \
			debug_print("assert error in @ "); \
            SPIN();                             \
		}                                          \
	} while (0)

struct tx_ring;

void ninf_init(void);
void ninf_rx_loop(void);
void ninf_tx_loop(void);
void ninf_tx_init(void);
void ninf_tx_add_ring(struct eos_ring *r);
void ninf_tx_del_ring(struct tx_ring *r);

#endif /* NINF_H */
