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
#include "fromshmem.hh"

#include <click/args.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/task.hh>
#include <stdio.h>
#include <llprint.h>

extern "C"{
       extern int packet_ptr;
       extern int packet_length;
       extern int packet_port;
}

CLICK_DECLS

FromShmem::FromShmem() :
		_shmem_ptr((unsigned long)shmem_addr), _count(0), _task(this) {
}

FromShmem::~FromShmem() {
}

int FromShmem::configure(Vector<String> &conf, ErrorHandler *errh) {
	if (Args(conf, this, errh).read_p("SHMEM_PTR", IntArg(), _shmem_ptr).read_p(
			"SHEME_SIZE", IntArg(), _shmem_size).complete() < 0)
		return -1;

	return 0;
}

int FromShmem::initialize(ErrorHandler *errh) {
	ScheduleInfo::initialize_task(this, &_task, errh);
	return 0;
}

void FromShmem::cleanup(CleanupStage stage) {
}

void FromShmem::push(int port, Packet *p) {
}

bool FromShmem::run_task(Task *) {
       Packet *p;

       assert(packet_ptr);
       assert(packet_length);
       p = Packet::make((unsigned char*)packet_ptr, packet_length, NULL, NULL, packet_port);
       output(0).push(p);
	_task.fast_reschedule();
       return 0;
}

void FromShmem::add_handlers() {
	add_read_handler("count", read_handler, 0);
	add_write_handler("reset_counts", reset_counts, 0, Handler::BUTTON);
}

String FromShmem::read_handler(Element* e, void *thunk) {
	return String(static_cast<FromShmem*>(e)->_count);
}

int FromShmem::reset_counts(const String &, Element *e, void *, ErrorHandler *) {
	static_cast<FromShmem*>(e)->_count = 0;
	return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FromShmem)
