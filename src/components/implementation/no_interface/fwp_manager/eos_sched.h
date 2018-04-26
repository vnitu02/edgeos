#ifndef __EOS_SCHED_H__
#define __EOS_SCHED_H__

#include <sl.h>

int eos_initaep_alloc(cpuid_t cpu, struct cos_defcompinfo *dci, struct sl_thd *sched_thd, int is_sched, int own_tcap, cos_channelkey_t key, struct sl_thd **ret_thd);
int eos_thd_param_set(cpuid_t cpu, struct sl_thd **arg_thd, sched_param_t sp);
int eos_thd_wakeup(cpuid_t cpu, thdid_t tid);
int eos_thd_set_tls(cpuid_t cpu, struct cos_compinfo *ci, thdcap_t *tc, void *tlsaddr);

#endif /* __EOS_SCHED_H__ */
