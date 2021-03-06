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

CLICK_DECLS

ToShmem::ToShmem() :
		_shmem_ptr((unsigned long)shmem_addr), _count(0), _task(this) {
}

ToShmem::~ToShmem() {
}

int ToShmem::configure(Vector<String> &conf, ErrorHandler *errh) {
	if (Args(conf, this, errh).read_p("SHMEM_PTR", IntArg(), _shmem_ptr).read_p(
			"SHMEM_SIZE", IntArg(), _shmem_size).complete() < 0)
		return -1;

	return 0;
}

int ToShmem::initialize(ErrorHandler *errh) {
	if (input_is_pull(0)) {
		ScheduleInfo::initialize_task(this, &_task, errh);
	}

	return 0;
}

void ToShmem::cleanup(CleanupStage stage) {
}

void ToShmem::push(int port, Packet *p) {
	next_call_sinv((int)p->data(), (int)p->length(), p->port());
       p->kill();
}

bool ToShmem::run_task(Task *) {
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
