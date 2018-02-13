/*
 *          ClickOS
 *
 *   file: shmem.cc
 *
 *          NEC Europe Ltd. PROPRIETARY INFORMATION
 *
 * This software is supplied under the terms of a license agreement
 * or nondisclosure agreement with NEC Europe Ltd. and may not be
 * copied or disclosed except in accordance with the terms of that
 * agreement. The software and its source code contain valuable trade
 * secrets and confidential information which have to be maintained in
 * confidence.
 * Any unauthorized publication, transfer to third parties or duplication
 * of the object or source code - either totally or in part – is
 * prohibited.
 *
 *      Copyright (c) 2014 NEC Europe Ltd. All Rights Reserved.
 *
 * Authors: Joao Martins <joao.martins@neclab.eu>
 *          Filipe Manco <filipe.manco@neclab.eu>
 *
 * NEC Europe Ltd. DISCLAIMS ALL WARRANTIES, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE AND THE WARRANTY AGAINST LATENT
 * DEFECTS, WITH RESPECT TO THE PROGRAM AND THE ACCOMPANYING
 * DOCUMENTATION.
 *
 * No Liability For Consequential Damages IN NO EVENT SHALL NEC Europe
 * Ltd., NEC Corporation OR ANY OF ITS SUBSIDIARIES BE LIABLE FOR ANY
 * DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION, DAMAGES FOR LOSS
 * OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF INFORMATION, OR
 * OTHER PECUNIARY LOSS AND INDIRECT, CONSEQUENTIAL, INCIDENTAL,
 * ECONOMIC OR PUNITIVE DAMAGES) ARISING OUT OF THE USE OF OR INABILITY
 * TO USE THIS PROGRAM, EVEN IF NEC Europe Ltd. HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 *     THIS HEADER MAY NOT BE EXTRACTED OR MODIFIED IN ANY WAY.
 */
#include "toshmem.hh"

#include <click/args.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/task.hh>
#include <stdio.h>

/*function pointer for ipc*/
extern void (*next_call)(void);

ck_ring_t *output_ring;
static int entries;

CLICK_DECLS

ToShmem::ToShmem() :
		_shmem_ptr(DEFAULT_SHMEM_ADDR + sizeof(ck_ring_t)), _shmem_size(
		DEFAULT_SHMEM_SIZE - sizeof(ck_ring_t)), _count(0), _task(this) {
}

ToShmem::~ToShmem() {
}

int ToShmem::configure(Vector<String> &conf, ErrorHandler *errh) {
	if (Args(conf, this, errh).read_p("SHMEM_PTR", IntArg(), _shmem_ptr).read_p(
			"SHEME_SIZE", IntArg(), _shmem_size).complete() < 0)
		return -1;

	/*TODO hardwire*/
	entries = 8;
	if (entries <= 4 || (entries & (entries - 1))) {
		return errh->error("ERROR: Size must be a power of 2 greater than 4.\n");
	}

	//errh->warning("Address of the pointer %p ring %p\n", output_ring, &ring);

	return 0;
}

int ToShmem::initialize(ErrorHandler *errh) {

	ck_ring_init(output_ring, entries);

	if (input_is_pull(0)) {
		ScheduleInfo::initialize_task(this, &_task, errh);
	}

	return 0;
}

void ToShmem::cleanup(CleanupStage stage) {
	next_call();
}

void ToShmem::push(int port, Packet *p) {
	bool r = ck_ring_enqueue_spsc(output_ring, (ck_ring_buffer *) _shmem_ptr, p);
	assert(r==true);
	next_call();
	//checked_output_push(0, p);
}

bool ToShmem::run_task(Task *) {
	bool r;
	Packet* p;

	for ( ; ; ) {
		p = input(0).pull();
		if (!p)
			break;

		r = ck_ring_enqueue_spsc(output_ring, (ck_ring_buffer *) _shmem_ptr, p);
		assert(r==true);
		checked_output_push(0, p);
	}

	return 0;
}

void ToShmem::add_handlers() {
	add_read_handler("count", read_handler, 0);
	add_write_handler("reset_counts", reset_counts, 0, Handler::BUTTON);
}

String ToShmem::read_handler(Element* e, void *thunk) {
	return String(static_cast<ToShmem*>(e)->_count);
}

int ToShmem::reset_counts(const String &, Element *e, void *, ErrorHandler *) {
	static_cast<ToShmem*>(e)->_count = 0;
	return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ToShmem)
