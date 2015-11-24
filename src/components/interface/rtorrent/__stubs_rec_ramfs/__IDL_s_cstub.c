/* IDL generated code ver 0.1 ---  Tue Nov 24 09:16:26 2015 */

#include <cos_component.h>
#include <sched.h>
#include <print.h>
#include <cos_debug.h>
#include <cos_map.h>
#include <cos_list.h>
#include <cstub.h>
#include <ramfs.h>

int __ser_treadp(spdid_t spdid, int tid, int len, int __pad0, int *off_len)
{
	int ret = 0;
	ret = treadp(spdid, tid, len, &off_len[0], &off_len[1]);
	return ret;
}

struct __ser_tsplit_marshalling {
	spdid_t spdid;
	td_t parent_tid;
	int len;
	tor_flags_t tflags;
	long evtid;
	char data[0];
};

td_t __ser_tsplit(spdid_t spdid, cbuf_t cbid, int len)
{
	struct __ser_tsplit_marshalling *md = NULL;

	md = (struct __ser_tsplit_marshalling *)cbuf2buf(cbid, len);
	assert(md);

	/* // for IDL now, ignore these checking */
	/* if (unlikely(md->len[0] != 0)) return -2;  */
	/* if (unlikely(md->len[0] > d->len[1])) return -3; */
	/* if (unlikely(((int)(md->len[1] + sizeof(struct __ser_tsplit_data))) != len)) return -4; */
	/* if (unlikely(md->tid == 0)) return -EINVAL; */

	return tsplit(md->spdid, md->parent_tid, &md->data[0], md->len,
		      md->tflags, md->evtid);
}
