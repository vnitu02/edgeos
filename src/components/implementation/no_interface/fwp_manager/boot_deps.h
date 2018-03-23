#include <cobj_format.h>
#include <cos_alloc.h>
#include <cos_debug.h>
#include <cos_types.h>
#include <llprint.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <llboot.h>
#include <res_spec.h>

#define UNDEF_SYMBS 64

/* Assembly function for sinv from new component */
extern void *llboot_entry_inv(int a, int b, int c);
extern int num_cobj;
extern int resmgr_spdid;
extern int root_spdid;

struct cobj_header *hs[MAX_NUM_SPDS + 1];

/* The booter uses this to keep track of each comp */
struct comp_cap_info {
	struct cos_defcompinfo def_cinfo;
	struct usr_inv_cap   ST_user_caps[UNDEF_SYMBS];
	vaddr_t              vaddr_user_caps; // vaddr of user caps table in comp
	vaddr_t              addr_start;
	vaddr_t              vaddr_mapped_in_booter;
	vaddr_t              upcall_entry;
	int                  is_sched;
	int                  sched_no;
	int                  parent_no;
	spdid_t              parent_spdid;
	u64_t                childid_bitf;
	u64_t                childid_sched_bitf;
	struct sl_thd       *initaep;
} new_comp_cap_info[MAX_NUM_SPDS + 1];

int                      schedule[MAX_NUM_SPDS + 1];
unsigned long               comp_info_offset[MAX_NUM_SPDS + 1];
volatile size_t          sched_cur;

/*
 * These are probably used ony in a clickos.o component
 * but define them as an array to avoid bugs.
 */
vaddr_t                     end_rodata[MAX_NUM_SPDS + 1];
vaddr_t                     sinv_next[MAX_NUM_SPDS + 1];

struct cos_compinfo *
boot_spd_compinfo_get(spdid_t spdid)
{
	if (spdid == 0) return cos_compinfo_get(cos_defcompinfo_curr_get());

	return cos_compinfo_get(&(new_comp_cap_info[spdid].def_cinfo));
}

static inline struct cos_defcompinfo *
boot_spd_defcompinfo_get(spdid_t spdid)
{
	if (spdid == 0) return cos_defcompinfo_curr_get();

	return &(new_comp_cap_info[spdid].def_cinfo);
}

static vaddr_t
boot_deps_map_sect(spdid_t spdid, vaddr_t dest_daddr)
{
	struct cos_compinfo *compinfo  = boot_spd_compinfo_get(spdid);
	struct cos_compinfo *boot_info = boot_spd_compinfo_get(0);
	vaddr_t addr = (vaddr_t)cos_page_bump_alloc(boot_info);

	assert(addr);
	if (cos_mem_alias_at(compinfo, dest_daddr, boot_info, addr)) BUG();
	cos_vasfrontier_init(compinfo, dest_daddr + PAGE_SIZE);

	return addr;
}

static void
boot_comp_pgtbl_expand(size_t n_pte, pgtblcap_t pt, vaddr_t vaddr, struct cobj_header *h)
{
	struct cos_compinfo *boot_info = boot_spd_compinfo_get(0);
	size_t i;
	int tot = 0;

	/* Expand Page table, could do this faster */
	for (i = 0; i < (size_t)h->nsect; i++) {
		tot += cobj_sect_size(h, i);
	}

	if (tot > SERVICE_SIZE) {
		n_pte = tot / SERVICE_SIZE;
		if (tot % SERVICE_SIZE) n_pte++;
	}

	for (i = 0; i < n_pte; i++) {
		if (!cos_pgtbl_intern_alloc(boot_info, pt, vaddr, SERVICE_SIZE)) BUG();
	}
}

#define RESMGR_UNTYPED_MEM_SZ (COS_MEM_KERN_PA_SZ / 2)

/* Initialize just the captblcap and pgtblcap, due to hack for upcall_fn addr */
static void
boot_compinfo_init(spdid_t spdid, captblcap_t *ct, pgtblcap_t *pt, u32_t vaddr)
{
	struct cos_compinfo *compinfo  = boot_spd_compinfo_get(spdid);
	struct cos_compinfo *boot_info = boot_spd_compinfo_get(0);

	*ct = cos_captbl_alloc(boot_info);
	assert(*ct);
	*pt = cos_pgtbl_alloc(boot_info);
	assert(*pt);

	cos_compinfo_init(compinfo, *pt, *ct, 0, (vaddr_t)vaddr, BOOT_CAPTBL_FREE, boot_info);
	if (spdid && spdid == resmgr_spdid) {
		pgtblcap_t utpt;

		utpt = cos_pgtbl_alloc(boot_info);
		assert(utpt);
		cos_meminfo_init(&(compinfo->mi), BOOT_MEM_KM_BASE, RESMGR_UNTYPED_MEM_SZ, utpt);
		cos_meminfo_alloc(compinfo, BOOT_MEM_KM_BASE, RESMGR_UNTYPED_MEM_SZ);
	}
}

static void
boot_newcomp_sinv_alloc(spdid_t spdid)
{
	sinvcap_t sinv;
	int i = 0;
	int intr_spdid;
	void *user_cap_vaddr;
	struct cos_compinfo *interface_compinfo;
	struct cos_compinfo *compinfo  = boot_spd_compinfo_get(spdid);
	/* TODO: Purge rest of booter of spdid convention */
	unsigned long token = (unsigned long)spdid;

	/*
	 * Loop through all undefined symbs
	 */
	for (i = 0; i < UNDEF_SYMBS; i++) {
		if ( new_comp_cap_info[spdid].ST_user_caps[i].service_entry_inst > 0) {

			intr_spdid = new_comp_cap_info[spdid].ST_user_caps[i].invocation_count;
			interface_compinfo = boot_spd_compinfo_get(intr_spdid);
			user_cap_vaddr = (void *) (new_comp_cap_info[spdid].vaddr_mapped_in_booter
						+ (new_comp_cap_info[spdid].vaddr_user_caps
						- new_comp_cap_info[spdid].addr_start) + (sizeof(struct usr_inv_cap) * i));

			/* Create sinv capability from client to server */
			sinv = cos_sinv_alloc(compinfo, interface_compinfo->comp_cap,
					      (vaddr_t)new_comp_cap_info[spdid].ST_user_caps[i].service_entry_inst, token);
			assert(sinv > 0);

			new_comp_cap_info[spdid].ST_user_caps[i].cap_no = sinv;

			/* Now that we have the sinv allocated, we can copy in the symb user cap to correct index */
			memcpy(user_cap_vaddr, &new_comp_cap_info[spdid].ST_user_caps[i], sizeof(struct usr_inv_cap));
		}
	}
}

static void
boot_alloc_tls(struct cos_compinfo *parent_cinfo_l, struct cos_compinfo *chld_cinfo_l, thdcap_t tc, size_t tls_size)
{
       vaddr_t tls_seg, addr, dst_pg, dst_seg;

       tls_seg = (vaddr_t) cos_page_bump_allocn(parent_cinfo_l, tls_size);
       assert(tls_seg);
       dst_seg = cos_mem_alias(chld_cinfo_l, parent_cinfo_l, tls_seg);
       assert(dst_seg);
       for (addr = PAGE_SIZE; addr < tls_size; addr += PAGE_SIZE) {
              dst_pg = cos_mem_alias(chld_cinfo_l, parent_cinfo_l, (tls_seg + addr));
              assert(dst_pg);
       }

       cos_thd_mod(parent_cinfo_l, tc, (void *)dst_seg);
}

/* TODO: possible to cos_defcompinfo_child_alloc if we somehow move allocations to one place */
static void
boot_newcomp_defcinfo_init(spdid_t spdid)
{
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_aep_info *   sched_aep = cos_sched_aep_get(defci);
	struct cos_aep_info *   child_aep = cos_sched_aep_get(boot_spd_defcompinfo_get(spdid));
	struct cos_compinfo *   child_ci  = boot_spd_compinfo_get(spdid);
	struct cos_compinfo *   boot_info = boot_spd_compinfo_get(0);

	child_aep->thd = cos_initthd_alloc(boot_info, child_ci->comp_cap);
	assert(child_aep->thd);

       boot_alloc_tls(boot_info, child_ci, child_aep->thd, PAGE_SIZE);

	if (new_comp_cap_info[spdid].is_sched) {
		child_aep->tc = cos_tcap_alloc(boot_info);
		assert(child_aep->tc);

		child_aep->rcv = cos_arcv_alloc(boot_info, child_aep->thd, child_aep->tc, boot_info->comp_cap, sched_aep->rcv);
		assert(child_aep->rcv);
	} else {
		child_aep->tc  = sched_aep->tc;
		child_aep->rcv = sched_aep->rcv;
	}

	child_aep->fn   = NULL;
	child_aep->data = NULL;
}

static void
boot_newcomp_init_caps(spdid_t spdid)
{
	struct cos_compinfo  *boot_info = boot_spd_compinfo_get(0);
	struct cos_compinfo  *ci        = boot_spd_compinfo_get(spdid);
	struct comp_cap_info *capci     = &new_comp_cap_info[spdid];
	int ret, i;
	
	/* FIXME: not everyone should have it. but for now, because getting cpu cycles uses this */
	ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_INITHW_BASE, boot_info, BOOT_CAPTBL_SELF_INITHW_BASE);
	assert(ret == 0);

	if (capci->is_sched) {
		/*
		 * FIXME:
		 * This is an ugly hack to allow components to do cos_introspect()
		 * - to get thdid
		 * - to get budget on tcap
		 * - other introspect...requirements..
		 *
		 * I don't know a way to get away from this for now! 
		 * If it were just thdid, resmgr could have returned the thdids!
		 */
		ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_CT, boot_info, ci->captbl_cap);
		assert(ret == 0);
	}

	/* If booter should create the init caps in that component */
	if (capci->parent_spdid == 0) {
		boot_newcomp_defcinfo_init(spdid);
		capci->initaep = sl_thd_comp_init(&(capci->def_cinfo), capci->is_sched);
		assert(capci->initaep);

		/* TODO: Scheduling parameters to schedule them! */

		ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_INITTHD_BASE, boot_info, sl_thd_thdcap(capci->initaep));
		assert(ret == 0);

		if (capci->is_sched) {
			ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_INITRCV_BASE, boot_info, sl_thd_rcvcap(capci->initaep));
			assert(ret == 0);
			ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_INITTCAP_BASE, boot_info, sl_thd_tcap(capci->initaep));
			assert(ret == 0);
		}

		if (resmgr_spdid == spdid) {
			assert(capci->is_sched == 0);
			ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_PT, boot_info, ci->pgtbl_cap);
			assert(ret == 0);
			ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_CT, boot_info, ci->captbl_cap);
			assert(ret == 0);
			ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_COMP, boot_info, ci->comp_cap);
			assert(ret == 0);
			ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_UNTYPED_PT, boot_info, ci->mi.pgtbl_cap);
			assert(ret == 0);
		}

		/* FIXME: remove when llbooter can do something else for scheduling bootup phase */
		i = 0;
		while (schedule[i] != 0) i ++;
		schedule[i] = sl_thd_thdcap(capci->initaep);
	}
}

static void
boot_newcomp_create(spdid_t spdid, struct cos_compinfo *comp_info)
{
	struct cos_compinfo *compinfo  = boot_spd_compinfo_get(spdid);
	struct cos_compinfo *boot_info = boot_spd_compinfo_get(0);
	compcap_t   cc;
	captblcap_t ct = compinfo->captbl_cap;
	pgtblcap_t  pt = compinfo->pgtbl_cap;
	sinvcap_t   sinv;
	thdcap_t    main_thd;
	int         i = 0;
	unsigned long token = (unsigned long) spdid;
	int ret;

	cc = cos_comp_alloc(boot_info, ct, pt, (vaddr_t)new_comp_cap_info[spdid].upcall_entry);
	assert(cc);
	compinfo->comp_cap = cc;

	/* Create sinv capability from Userspace to Booter components */
	sinv = cos_sinv_alloc(boot_info, boot_info->comp_cap, (vaddr_t)llboot_entry_inv, token);
	assert(sinv > 0);

	ret = cos_cap_cpy_at(compinfo, BOOT_CAPTBL_SINV_CAP, boot_info, sinv);
	assert(ret == 0);

	boot_newcomp_sinv_alloc(spdid);
	boot_newcomp_init_caps(spdid);
}

static void
boot_bootcomp_init(void)
{
	struct cos_compinfo *boot_info = boot_spd_compinfo_get(0);

	/* TODO: if posix already did meminfo init */
	cos_meminfo_init(&(boot_info->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();
	sl_init(SL_MIN_PERIOD_US);
}

#define LLBOOT_ROOT_PRIO 1
#define LLBOOT_ROOT_BUDGET_MS (10*1000)
#define LLBOOT_ROOT_PERIOD_MS (10*1000)

#undef LLBOOT_CHRONOS_ENABLED

static void
boot_done(void)
{
	struct sl_thd *root = NULL;
	int ret;

	printc("Booter: done creating system.\n");
	printc("********************************\n");
	/*cos_thd_switch(schedule[sched_cur]);

	if (root_spdid) {
		printc("Root scheduler is %u\n", root_spdid);
		root = new_comp_cap_info[root_spdid].initaep;
		assert(root);
		sl_thd_param_set(root, sched_param_pack(SCHEDP_PRIO, LLBOOT_ROOT_PRIO));
#ifdef LLBOOT_CHRONOS_ENABLED
		sl_thd_param_set(root, sched_param_pack(SCHEDP_BUDGET, LLBOOT_ROOT_BUDGET_MS));
		sl_thd_param_set(root, sched_param_pack(SCHEDP_WINDOW, LLBOOT_ROOT_PERIOD_MS));
#else
		ret = cos_tcap_transfer(sl_thd_rcvcap(root), sl__globals()->sched_tcap, TCAP_RES_INF, LLBOOT_ROOT_PRIO);
		assert(ret == 0);
#endif
	}*/

	printc("Starting llboot sched loop\n");
	sl_sched_loop();
}

/* Run after a componenet is done init execution, via sinv() into booter */
void
boot_thd_done(void)
{
	sched_cur++;

	if (schedule[sched_cur] != 0) {
		cos_thd_switch(schedule[sched_cur]);
	} else {
		printc("Done Initializing\n");
		printc("********************************\n");
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
	}
}