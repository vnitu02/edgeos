// -*- c-basic-offset: 4 -*-
/*
 * click.cc -- user-level Click main program
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2001-2003 International Computer Science Institute
 * Copyright (c) 2004-2006 Regents of the University of California
 * Copyright (c) 2008-2009 Meraki, Inc.
 * Copyright (c) 1999-2012 Eddie Kohler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/pathvars.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <fcntl.h>
#if HAVE_EXECINFO_H
# include <execinfo.h>
#endif

#include <click/lexer.hh>
#include <click/routerthread.hh>
#include <click/router.hh>
#include <click/master.hh>
#include <click/error.hh>
#include <click/timer.hh>
#include <click/straccum.hh>
#include <click/archive.hh>
#include <click/glue.hh>
#include <click/driver.hh>
#include <click/args.hh>
#include <click/handlercall.hh>
#include "elements/standard/quitwatcher.hh"
#include "elements/userlevel/controlsocket.hh"
CLICK_USING_DECLS

extern "C" {
#include <llboot.h>
#include "init.h"
}

#define HELP_OPT		300
#define VERSION_OPT		301
#define CLICKPATH_OPT		302
#define ROUTER_OPT		303
#define EXPRESSION_OPT		304
#define QUIT_OPT		305
#define OUTPUT_OPT		306
#define HANDLER_OPT		307
#define TIME_OPT		308
#define PORT_OPT		310
#define UNIX_SOCKET_OPT		311
#define NO_WARNINGS_OPT		312
#define WARNINGS_OPT		313
#define ALLOW_RECONFIG_OPT	314
#define EXIT_HANDLER_OPT	315
#define THREADS_OPT		316
#define SIMTIME_OPT		317
#define SOCKET_OPT		318

struct cos_compinfo *posix_cinfo;
static const char *program_name;

void short_usage() {
	fprintf(stderr,
			"Usage: %s [OPTION]... [ROUTERFILE]\n\
Try '%s --help' for more information.\n",
			program_name, program_name);
}

void usage() {
	printf(
			"\
'Click' runs a Click router configuration at user level. It installs the\n\
configuration, reporting any errors to standard error, and then generally runs\n\
until interrupted.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE               Read router configuration from FILE.\n\
  -e, --expression EXPR         Use EXPR as router configuration.\n\
  -j, --threads N               Start N threads (default 1).\n\
  -p, --port PORT               Listen for control connections on TCP port.\n\
  -u, --unix-socket FILE        Listen for control connections on Unix socket.\n\
      --socket FD               Add a file descriptor control connection.\n\
  -R, --allow-reconfigure       Provide a writable 'hotconfig' handler.\n\
  -h, --handler ELEMENT.H       Call ELEMENT's read handler H after running\n\
                                driver and print result to standard output.\n\
  -x, --exit-handler ELEMENT.H  Use handler ELEMENT.H value for exit status.\n\
  -o, --output FILE             Write flat configuration to FILE.\n\
  -q, --quit                    Do not run driver.\n\
  -t, --time                    Print information on how long driver took.\n\
  -w, --no-warnings             Do not print warnings.\n\
      --simtime                 Run in simulation time.\n\
  -C, --clickpath PATH          Use PATH for CLICKPATH.\n\
      --help                    Print this message and exit.\n\
  -v, --version                 Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n",
			program_name);
}

static Router *router;
static ErrorHandler *errh;
static bool running = false;

extern "C" {
static void stop_signal_handler(int sig) {
#if !HAVE_SIGACTION
	signal(sig, SIG_DFL);
#endif
	if (!running)
		kill(getpid(), sig);
	else
		router->set_runcount(Router::STOP_RUNCOUNT);
}

#if HAVE_EXECINFO_H
static void catch_dump_signal(int sig) {
	(void) sig;

	/* reset these signals so if we do something bad we just exit */
	click_signal(SIGSEGV, SIG_DFL, false);
	click_signal(SIGBUS, SIG_DFL, false);
	click_signal(SIGILL, SIG_DFL, false);
	click_signal(SIGABRT, SIG_DFL, false);
	click_signal(SIGFPE, SIG_DFL, false);

	/* dump the results to standard error */
	void *return_addrs[50];
	int naddrs = backtrace(return_addrs, sizeof(return_addrs) / sizeof(void *));
	backtrace_symbols_fd(return_addrs, naddrs, STDERR_FILENO);

	/* dump core and quit */
	abort();
}
#endif
}

// hotswapping

static Router *hotswap_router;
static Router *hotswap_thunk_router;
static bool hotswap_hook(Task *, void *);
static Task hotswap_task(hotswap_hook, 0);

static bool hotswap_hook(Task *, void *) {
	hotswap_thunk_router->set_foreground(false);
	hotswap_router->activate(ErrorHandler::default_handler());
	router->unuse();
	router = hotswap_router;
	router->use();
	hotswap_router = 0;
	return true;
}

// switching configurations

static Vector<String> cs_unix_sockets;
static Vector<String> cs_ports;
static Vector<String> cs_sockets;
static bool warnings = true;
static int nthreads = 1;

/*
 * parse a Click config file and create a new Router with this information
 */
static Router *
parse_configuration(const String &text, bool text_is_expr, ErrorHandler *errh) {
	Master *new_master = 0, *master;
	if (router)
		master = router->master();
	else
		master = new_master = new Master(nthreads);

	Router *r = click_read_router(text, text_is_expr, errh, false, master);
	if (!r) {
		delete new_master;
		return 0;
	}
	if (errh->nerrors() > 0 || r->initialize(errh) < 0) {
		delete r;
		delete new_master;
		return 0;
	} else {
		return r;
    }
}

extern "C" void run_driver(void) {

	router->use();

	int exit_value = 0;
#if HAVE_MULTITHREAD
	Vector<pthread_t> other_threads;
#endif

	struct rusage before, after;
	getrusage(RUSAGE_SELF, &before);
	Timestamp before_time = Timestamp::now_unwarped();
	Timestamp after_time = Timestamp::uninitialized_t();

	// run driver
	// 10.Apr.2004 - Don't run the router if it has no elements.
	if (router->nelements()) {
		running = true;
		router->activate(errh);
#if HAVE_MULTITHREAD
		for (int t = 1; t < nthreads; ++t) {
			pthread_t p;
			pthread_create(&p, 0, thread_driver, router->master()->thread(t));
			other_threads.push_back(p);
		}
#endif

		// run driver
		router->master()->thread(0)->driver();

		// now that the driver has stopped, SIGINT gets default handling
		running = false;
		click_fence();
	} else if (warnings)
		errh->warning("configuration has no elements, exiting");

	after_time.assign_now_unwarped();
	getrusage(RUSAGE_SELF, &after);

	Master *master = router->master();
	router->unuse();
#if HAVE_MULTITHREAD
	for (int i = 0; i < other_threads.size(); ++i)
	master->thread(i + 1)->wake();
	for (int i = 0; i < other_threads.size(); ++i)
	(void) pthread_join(other_threads[i], 0);
#endif
	delete master;
}

extern "C" void run_driver_once(void) {
        router->use();

        // run driver
        running = true;
        router->activate(errh);

        // run driver
        router->master()->thread(0)->run_one_task();

        // now that the driver has stopped, SIGINT gets default handling
        running = false;
        click_fence();
}

/*
 * entry point in the click code
 */
extern "C" int click_initialize(void *data) {
	struct click_init *init_data = (struct click_init *) data;

	//start = rdtsc();
       click_static_initialize();
	errh = ErrorHandler::default_handler();

	// parse configuration
	router = parse_configuration(init_data->conf_str, true, errh);
	if (!router)
		return 1;

	return 0;
}
