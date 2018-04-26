#include <ps.h>
#include <ck_ring.h>
#include <sl_xcpu.h>
#include <sl.h>
#include <bitmap.h>

#define SL_REQ_THD_ALLOC(req, fn, data) do {							\
						req.sl_xcpu_req_thd_alloc.fn = fn;		\
						req.sl_xcpu_req_thd_alloc.data = data;		\
					     } while (0)

#define SL_REQ_INITAEP_ALLOC(req, dci, sched_thd, is_sched, own_tcap, key, ret_thd) do { \
						req.sl_xcpu_req_initaep_alloc.dci = dci;		\
						req.sl_xcpu_req_initaep_alloc.sched_thd = sched_thd;		\
						req.sl_xcpu_req_initaep_alloc.is_sched = is_sched;		\
						req.sl_xcpu_req_initaep_alloc.own_tcap = own_tcap;		\
						req.sl_xcpu_req_initaep_alloc.key = key;		\
						req.sl_xcpu_req_initaep_alloc.ret_thd = ret_thd;		\
					     } while (0)

#define SL_REQ_THD_SET_PARAM(req, arg_thd, sp) do {			\
						req.sl_xcpu_req_thd_set_param.arg_thd = arg_thd; \
						req.sl_xcpu_req_thd_set_param.sp = sp; \
					     } while (0)

#define SL_REQ_THD_WAKEUP(req, tid) do {						\
						req.sl_xcpu_req_thd_wakeup.tid = tid; \
					     } while (0)

#define	SL_REQ_THD_SET_TLS(req, ci, tc, tlsaddr) do { \
						req.sl_xcpu_req_set_tls.ci = ci; \
						req.sl_xcpu_req_set_tls.tc = tc; \
						req.sl_xcpu_req_set_tls.tlsaddr = tlsaddr; \
	} while (0)

#define SL_REQ_TYPE(req, type) do {						\
						req->type = type;			\
						req->client = cos_cpuid();				\
						req->req_response = 0;				\
					     } while (0)

extern struct sl_thd *sl_thd_alloc_no_cs(cos_thd_fn_t fn, void *data);
extern struct sl_thd *sl_thd_initaep_alloc_no_cs(struct cos_defcompinfo *comp, struct sl_thd *sched_thd, int is_sched, int own_tcap, cos_channelkey_t key);

static inline int
__sl_xcpu_req_exit(struct sl_xcpu_request *req, cpuid_t cpu, sl_xcpu_req_t type)
{
	int ret = 0;
	SL_REQ_TYPE(req, type);
	if (ck_ring_enqueue_mpsc_xcpu(sl__ring(cpu), sl__ring_buffer(cpu), req) != true) {
		ret = -ENOMEM;
	}
	sl_cs_exit();
	return ret;
}

int
sl_xcpu_thd_alloc(cpuid_t cpu, cos_thd_fn_t fn, void *data, sched_param_t params[])
{
	int i, sz = sizeof(params) / sizeof(params[0]);
	int ret = 0;
	asndcap_t snd = 0;
	struct sl_xcpu_request req;

	if (cpu == cos_cpuid()) return -EINVAL;
	if (!bitmap_check(sl__globals()->cpu_bmp, cpu)) return -EINVAL;

	sl_cs_enter();

	SL_REQ_THD_ALLOC(req, fn, data);
	memcpy(req.params, params, sizeof(sched_param_t) * sz);
	req.param_count = sz;
	return __sl_xcpu_req_exit(&req, cpu, SL_XCPU_THD_ALLOC);
}

int
sl_xcpu_thd_alloc_ext(cpuid_t cpu, struct cos_defcompinfo *dci, thdclosure_index_t idx, sched_param_t params[])
{
	return -ENOTSUP;
}

int
sl_xcpu_aep_alloc(cpuid_t cpu, cos_thd_fn_t fn, void *data, int own_tcap, cos_channelkey_t key, sched_param_t params[])
{
	return -ENOTSUP;
}

int
sl_xcpu_aep_alloc_ext(cpuid_t cpu, struct cos_defcompinfo *dci, thdclosure_index_t idx, int own_tcap, cos_channelkey_t key, sched_param_t params[])
{
	return -ENOTSUP;
}

int
sl_xcpu_initaep_alloc(cpuid_t cpu, struct cos_defcompinfo *dci, struct sl_thd *sched_thd, int is_sched, int own_tcap, cos_channelkey_t key, struct sl_thd **ret_thd)
{
	struct sl_xcpu_request req;

	if (cpu == cos_cpuid()) return -EINVAL;
	if (!bitmap_check(sl__globals()->cpu_bmp, cpu)) return -EINVAL;

	sl_cs_enter();

	SL_REQ_INITAEP_ALLOC(req, dci, sched_thd, is_sched, own_tcap, key, ret_thd);
	return __sl_xcpu_req_exit(&req, cpu, SL_XCPU_INITAEP_ALLOC);
}

int
sl_xcpu_initaep_alloc_ext(cpuid_t cpu, struct cos_defcompinfo *dci, struct cos_defcompinfo *sched, int own_tcap, cos_channelkey_t key, sched_param_t params[])
{
	return -ENOTSUP;
}

int
sl_xcpu_thd_param_set(cpuid_t cpu, struct sl_thd **arg_thd, sched_param_t sp)
{
	struct sl_xcpu_request req;

	if (cpu == cos_cpuid()) return -EINVAL;
	if (!bitmap_check(sl__globals()->cpu_bmp, cpu)) return -EINVAL;

	sl_cs_enter();

	SL_REQ_THD_SET_PARAM(req, arg_thd, sp);
	return __sl_xcpu_req_exit(&req, cpu, SL_XCPU_THD_SET_PARAM);
}

int
sl_xcpu_thd_wakeup(cpuid_t cpu, thdid_t tid)
{
	struct sl_xcpu_request req;

	if (cpu == cos_cpuid()) return -EINVAL;
	if (!bitmap_check(sl__globals()->cpu_bmp, cpu)) return -EINVAL;

	sl_cs_enter();

	SL_REQ_THD_WAKEUP(req, tid);
	return __sl_xcpu_req_exit(&req, cpu, SL_XCPU_THD_WAKEUP);
}

int
sl_xcpu_thd_set_tls(cpuid_t cpu, struct cos_compinfo *ci, thdcap_t *tc, void *tlsaddr)
{
	struct sl_xcpu_request req;

	if (cpu == cos_cpuid()) return -EINVAL;
	if (!bitmap_check(sl__globals()->cpu_bmp, cpu)) return -EINVAL;

	sl_cs_enter();

	SL_REQ_THD_SET_TLS(req, ci, tc, tlsaddr);
	return __sl_xcpu_req_exit(&req, cpu, SL_XCPU_THD_SET_TLS);
}

int
sl_xcpu_process_no_cs(void)
{
	int num = 0;
	struct sl_thd *t;
	struct sl_xcpu_request req;

	while (ck_ring_dequeue_mpsc_xcpu(sl__ring_curr(), sl__ring_buffer_curr(), &req) == true) {

		assert(req.client != cos_cpuid());
		switch(req.type) {
		case SL_XCPU_THD_ALLOC:
		{
			cos_thd_fn_t   fn   = req.sl_xcpu_req_thd_alloc.fn;
			void          *data = req.sl_xcpu_req_thd_alloc.data;
			int i;

			assert(fn);

			t = sl_thd_alloc_no_cs(fn, data);
			assert(t);
			for (i = 0; i < req.param_count; i++) {
				sl_thd_param_set(t, req.params[i]);
			}

			break;
		}
		case SL_XCPU_INITAEP_ALLOC:
		{
			t = sl_thd_initaep_alloc_no_cs(req.sl_xcpu_req_initaep_alloc.dci, 
						       req.sl_xcpu_req_initaep_alloc.sched_thd,
						       req.sl_xcpu_req_initaep_alloc.is_sched,
						       req.sl_xcpu_req_initaep_alloc.own_tcap, 
						       req.sl_xcpu_req_initaep_alloc.key);
			assert(t);
			*req.sl_xcpu_req_initaep_alloc.ret_thd = t;
			break;
		}
		case SL_XCPU_THD_SET_PARAM:
		{
			t = *req.sl_xcpu_req_thd_set_param.arg_thd;
			assert(t);
			sl_thd_param_set(t, req.sl_xcpu_req_thd_set_param.sp);
			break;
		}
		case SL_XCPU_THD_WAKEUP:
		{
			t = sl_thd_lkup(req.sl_xcpu_req_thd_wakeup.tid);
			assert(t);
			sl_thd_wakeup_no_cs_rm(t);
			/* sl_thd_sched_wakeup_no_cs(t); */
			break;
		}
		case SL_XCPU_THD_SET_TLS:
		{
			cos_thd_mod(req.sl_xcpu_req_set_tls.ci, 
				    *(req.sl_xcpu_req_set_tls.tc), 
				    req.sl_xcpu_req_set_tls.tlsaddr);
			break;
		}
		case SL_XCPU_THD_ALLOC_EXT:
		case SL_XCPU_AEP_ALLOC:
		case SL_XCPU_AEP_ALLOC_EXT:
		case SL_XCPU_THD_DEALLOC:
		default:
		{
			PRINTC("Unimplemented request! Aborting!\n");
			assert(0);
		}
		}
		num ++;
	}

	return num; /* number of requests processed */
}
