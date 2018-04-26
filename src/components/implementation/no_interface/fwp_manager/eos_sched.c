#include "eos_sched.h"
#include <sl.h>
#include <sl_xcpu.h>

int
eos_initaep_alloc(cpuid_t cpu, struct cos_defcompinfo *dci, struct sl_thd *sched_thd, int is_sched, int own_tcap, cos_channelkey_t key, struct sl_thd **ret_thd)
{
	struct sl_thd *ret;

	if (cpu == cos_cpuid()) {
		ret = sl_thd_initaep_alloc(dci, sched_thd, is_sched, own_tcap, key);
		*ret_thd = ret;
		return 0;
	} else {
		return sl_xcpu_initaep_alloc(cpu, dci, sched_thd, is_sched, own_tcap, key, ret_thd);
	}
}

int
eos_thd_param_set(cpuid_t cpu, struct sl_thd **arg_thd, sched_param_t sp)
{
	if (cpu == cos_cpuid()) {
		sl_thd_param_set(*arg_thd, sp);
		return 0;
	} else {
		return sl_xcpu_thd_param_set(cpu, arg_thd, sp);
	}
}

int
eos_thd_wakeup(cpuid_t cpu, thdid_t tid)
{
	if (cpu == cos_cpuid()) {
		sl_thd_wakeup(tid);
		return 0;
	} else {
		return sl_xcpu_thd_wakeup(cpu, tid);
	}
}

int
eos_thd_set_tls(cpuid_t cpu, struct cos_compinfo *ci, thdcap_t *tc, void *tlsaddr)
{
	if (cpu == cos_cpuid()) {
		assert(0);
		return 0;
	} else {
		return sl_xcpu_thd_set_tls(cpu, ci, tc, tlsaddr);
	}
}
