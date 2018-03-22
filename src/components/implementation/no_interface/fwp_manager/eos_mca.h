#ifndef __EOS_MCA_H__
#define __EOS_MCA_H__

#include "eos_ring.h"

struct mca_conn {
	struct eos_ring *src_ring;
	struct eos_ring *dst_ring;
	struct mca_conn *next;
	int used;
	int src_ip, src_port;
	int dst_ip, dst_port;
};

struct mca_conn *mca_conn_create(struct eos_ring *src, struct eos_ring *dst);
void mca_conn_free(struct mca_conn *conn);
void mca_init(void);
void *mca_run(void);

#endif /* __EOS_MCA_H__ */

