#ifndef CK_MANAG_H
#define CK_MANAG_H

#define NF_COUNT 2
#define CHILD_COMP_COUNT	4

/* Macro for sinv back to booter from new component */
enum {
	NEXT_CALL_SINV_CAP = round_up_to_pow2(BOOT_CAPTBL_FREE + CAP32B_IDSZ,
			CAPMAX_ENTRY_SZ)
};

struct click_checkpoint {
	vaddr_t addr;
	size_t size;
};

struct chld_click_info chld_infos[CHILD_COMP_COUNT];
struct click_checkpoint templates[NF_COUNT];

/*function pointer for IPC*/
void (*next_call)(void);

void next_call_sinv_impl(void);

#endif /*CK_MANAG_H*/
