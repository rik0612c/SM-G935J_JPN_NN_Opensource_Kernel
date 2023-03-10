/*
 *  drivers/usb/notify/usblog_proc_notify.c
 *
 * Copyright (C) 2016 Samsung, Inc.
 * Author: Dongrak Shin <dongrak.shin@samsung.com>
 *
*/

 /* usb notify layer v2.0 */

 #define pr_fmt(fmt) "usb_notify: " fmt

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/security.h>
#include <linux/syscalls.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include <linux/usb_notify.h>

#define USBLOG_MAX_BUF_SIZE	(1 << 7) /* 128 */
#define USBLOG_MAX_BUF2_SIZE	(1 << 8) /* 256 */
#define USBLOG_MAX_STRING_SIZE	(1 << 4) /* 16 */
#define USBLOG_CMP_INDEX	3

struct ccic_buf {
	unsigned long long ts_nsec;
	int cc_type;
	uint64_t noti;
};

struct mode_buf {
	unsigned long long ts_nsec;
	char usbmode_str[USBLOG_MAX_STRING_SIZE];
};

struct state_buf {
	unsigned long long ts_nsec;
	int usbstate;
};

struct event_buf {
	unsigned long long ts_nsec;
	unsigned long event;
	int enable;
};

struct dwc_buf {
	unsigned long long ts_nsec;
	int state, prev_lpm, cur_lpm;
};

struct usblog_buf {
	unsigned long long ccic_count;
	unsigned long long mode_count;
	unsigned long long state_count;
	unsigned long long event_count;
	unsigned long long dwc_count;
	unsigned long ccic_index;
	unsigned long mode_index;
	unsigned long state_index;
	unsigned long event_index;
	unsigned long dwc_index;
	struct ccic_buf ccic_buffer[USBLOG_MAX_BUF2_SIZE];
	struct mode_buf mode_buffer[USBLOG_MAX_BUF_SIZE];
	struct state_buf state_buffer[USBLOG_MAX_BUF_SIZE];
	struct event_buf event_buffer[USBLOG_MAX_BUF_SIZE];
	struct dwc_buf dwc_buffer[USBLOG_MAX_BUF_SIZE];
};

struct ccic_version {
	unsigned char hw_version[4];
	unsigned char sw_main[3];
	unsigned char sw_boot;
};

struct usblog_root_str {
	struct usblog_buf *usblog_buffer;
	struct ccic_version ccic_ver;
	spinlock_t usblog_lock;
	int init;
};

struct ccic_type {
	uint64_t src:4;
	uint64_t dest:4;
	uint64_t id:8;
	uint64_t sub1:16;
	uint64_t sub2:16;
	uint64_t sub3:16;
};

static struct usblog_root_str usblog_root;

static const char *usbstate_string(enum usblog_state usbstate)
{
	switch (usbstate) {
	case NOTIFY_CONFIGURED:
		return "CONFIGURED";
	case NOTIFY_CONNECTED:
		return "CONNECTED";
	case NOTIFY_DISCONNECTED:
		return "DISCONNECTED";
	case NOTIFY_RESET:
		return "RESET";
	case NOTIFY_ACCSTART:
		return "ACCSTART";
	case NOTIFY_HIGH:
		return "HIGH SPEED";
	case NOTIFY_SUPER:
		return "SUPER SPEED";
	default:
		return "UNDEFINED";
	}
}

static const char *dwcstate_string(enum dwc_state dwcstate)
{
	switch (dwcstate) {
	case NOTIFY_IDLE:
		return "IDLE";
	case NOTIFY_SUSPEND:
		return "SUSPEND";
	case NOTIFY_RESUME:
		return "RESUME";
	default:
		return "UNDEFINED";
	}
}

static const char *usbstatus_string(enum usblog_status usbstatus)
{
	switch (usbstatus) {
	case NOTIFY_DETACH:
		return "DETACH";
	case NOTIFY_ATTACH_DFP:
		return "ATTACH_DFP";
	case NOTIFY_ATTACH_UFP:
		return "ATTACH_UFP";
	case NOTIFY_ATTACH_DRP:
		return "ATTACH_DRP";
	default:
		return "UNDEFINED";
	}
}

static const char *ccic_dev_string(enum ccic_device dev)
{
	switch (dev) {
	case NOTIFY_DEV_INITIAL:
		return "INITIAL";
	case NOTIFY_DEV_USB:
		return "USB";
	case NOTIFY_DEV_BATTERY:
		return "BATTERY";
	case NOTIFY_DEV_PDIC:
		return "PDIC";
	case NOTIFY_DEV_MUIC:
		return "MUIC";
	case NOTIFY_DEV_CCIC:
		return "CCIC";
	case NOTIFY_DEV_MANAGER:
		return "MANAGER";
	default:
		return "UNDEFINED";
	}
}

static const char *ccic_id_string(enum ccic_id id)
{
	switch (id) {
	case NOTIFY_ID_INITIAL:
		return "ID_INITIAL";
	case NOTIFY_ID_ATTACH:
		return "ID_CONNECT";
	case NOTIFY_ID_RID:
		return "ID_RID";
	case NOTIFY_ID_USB:
		return "ID_USB";
	case NOTIFY_ID_POWER_STATUS:
		return "ID_POWER_STATUS";
	case NOTIFY_ID_WATER:
		return "ID_WATER";
	case NOTIFY_ID_VCONN:
		return "ID_VCONN";
	default:
		return "UNDEFINED";
	}
}

static const char *ccic_rid_string(enum ccic_rid rid)
{
	switch (rid) {
	case NOTIFY_RID_UNDEFINED:
		return "RID_UNDEFINED";
	case NOTIFY_RID_000K:
		return "RID_000K";
	case NOTIFY_RID_001K:
		return "RID_001K";
	case NOTIFY_RID_255K:
		return "RID_255K";
	case NOTIFY_RID_301K:
		return "RID_301K";
	case NOTIFY_RID_523K:
		return "RID_523K";
	case NOTIFY_RID_619K:
		return "RID_619K";
	case NOTIFY_RID_OPEN:
		return "RID_OPEN";
	default:
		return "UNDEFINED";
	}
}

static const char *ccic_con_string(enum ccic_con con)
{
	switch (con) {
	case NOTIFY_CON_DETACH:
		return "DETACHED";
	case NOTIFY_CON_ATTACH:
		return "ATTACHED";
	default:
		return "UNDEFINED";
	}
}

static const char *ccic_rprd_string(enum ccic_rprd rprd)
{
	switch (rprd) {
	case NOTIFY_RD:
		return "RD";
	case NOTIFY_RP:
		return "RP";
	default:
		return "UNDEFINED";
	}
}

static void print_ccic_event(struct seq_file *m, unsigned long long ts,
		unsigned long rem_nsec, int cc_type, uint64_t *noti)
{
	struct ccic_type type = *(struct ccic_type *)noti;
	int cable = type.sub3;

	switch (cc_type) {
	case NOTIFY_FUNCSTATE:
		seq_printf(m, "[%5lu.%06lu] function state = %llu\n",
			(unsigned long)ts, rem_nsec / 1000, *noti);
		break;
	case NOTIFY_CCIC_EVENT:
		if (type.id == NOTIFY_ID_ATTACH)
			seq_printf(m, "[%5lu.%06lu] ccic notify:    id=%s src=%s dest=%s rprd=%s cable=%d %s\n",
			(unsigned long)ts, rem_nsec / 1000,
			ccic_id_string(type.id),
			ccic_dev_string(type.src),
			ccic_dev_string(type.dest),
			ccic_rprd_string(type.sub2),
			cable, ccic_con_string(type.sub1));
		else if (type.id  == NOTIFY_ID_RID)
			seq_printf(m, "[%5lu.%06lu] ccic notify:    id=%s src=%s dest=%s rid=%s\n",
			(unsigned long)ts, rem_nsec / 1000,
			ccic_id_string(type.id),
			ccic_dev_string(type.src),
			ccic_dev_string(type.dest),
			ccic_rid_string(type.sub1));
		else if (type.id  == NOTIFY_ID_USB)
			seq_printf(m, "[%5lu.%06lu] ccic notify:    id=%s src=%s dest=%s status=%s %s\n",
			(unsigned long)ts, rem_nsec / 1000,
			ccic_id_string(type.id),
			ccic_dev_string(type.src),
			ccic_dev_string(type.dest),
			usbstatus_string(type.sub2),
			ccic_con_string(type.sub1));
		else if (type.id  == NOTIFY_ID_POWER_STATUS)
			seq_printf(m, "[%5lu.%06lu] ccic notify:    id=%s src=%s dest=%s rprd=%s %s\n",
			(unsigned long)ts, rem_nsec / 1000,
			ccic_id_string(type.id),
			ccic_dev_string(type.src),
			ccic_dev_string(type.dest),
			ccic_rprd_string(type.sub2),
			ccic_con_string(type.sub1));
		else if (type.id  == NOTIFY_ID_WATER)
			seq_printf(m, "[%5lu.%06lu] ccic notify:   id=%s src=%s dest=%s\n",
			(unsigned long)ts, rem_nsec / 1000,
			ccic_id_string(type.id),
			ccic_dev_string(type.src),
			ccic_dev_string(type.dest));
		else if (type.id  == NOTIFY_ID_VCONN)
			seq_printf(m, "[%5lu.%06lu] ccic notify:    id=%s src=%s dest=%s\n",
			(unsigned long)ts, rem_nsec / 1000,
			ccic_id_string(type.id),
			ccic_dev_string(type.src),
			ccic_dev_string(type.dest));
		else
			seq_printf(m, "[%5lu.%06lu] ccic notify:    id=%s src=%s dest=%s rprd=%s %s\n",
			(unsigned long)ts, rem_nsec / 1000,
			ccic_id_string(type.id),
			ccic_dev_string(type.src),
			ccic_dev_string(type.dest),
			ccic_rprd_string(type.sub2),
			ccic_con_string(type.sub1));
		break;
	case NOTIFY_MANAGER:
		if (type.id == NOTIFY_ID_ATTACH)
			seq_printf(m, "[%5lu.%06lu] manager notify: id=%s src=%s dest=%s rprd=%s cable=%d %s\n",
			(unsigned long)ts, rem_nsec / 1000,
			ccic_id_string(type.id),
			ccic_dev_string(type.src),
			ccic_dev_string(type.dest),
			ccic_rprd_string(type.sub2), cable,
			ccic_con_string(type.sub1));
		else if (type.id  == NOTIFY_ID_RID)
			seq_printf(m, "[%5lu.%06lu] manager notify: id=%s src=%s dest=%s rid=%s\n",
			(unsigned long)ts, rem_nsec / 1000,
			ccic_id_string(type.id),
			ccic_dev_string(type.src),
			ccic_dev_string(type.dest),
			ccic_rid_string(type.sub1));
		else if (type.id  == NOTIFY_ID_USB)
			seq_printf(m, "[%5lu.%06lu] manager notify: id=%s src=%s dest=%s status=%s %s\n",
			(unsigned long)ts, rem_nsec / 1000,
			ccic_id_string(type.id),
			ccic_dev_string(type.src),
			ccic_dev_string(type.dest),
			usbstatus_string(type.sub2),
			ccic_con_string(type.sub1));
		else if (type.id  == NOTIFY_ID_POWER_STATUS)
			seq_printf(m, "[%5lu.%06lu] manager notify: id=%s src=%s dest=%s rprd=%s %s\n",
			(unsigned long)ts, rem_nsec / 1000,
			ccic_id_string(type.id),
			ccic_dev_string(type.src),
			ccic_dev_string(type.dest),
			ccic_rprd_string(type.sub2),
			ccic_con_string(type.sub1));
		else if (type.id  == NOTIFY_ID_WATER)
			seq_printf(m, "[%5lu.%06lu] manager notify: id=%s src=%s dest=%s\n",
			(unsigned long)ts, rem_nsec / 1000,
			ccic_id_string(type.id),
			ccic_dev_string(type.src),
			ccic_dev_string(type.dest));
		else if (type.id  == NOTIFY_ID_VCONN)
			seq_printf(m, "[%5lu.%06lu] manager notify: id=%s src=%s dest=%s\n",
			(unsigned long)ts, rem_nsec / 1000,
			ccic_id_string(type.id),
			ccic_dev_string(type.src),
			ccic_dev_string(type.dest));
		else
			seq_printf(m, "[%5lu.%06lu] manager notify: id=%s src=%s dest=%s rprd=%s %s\n",
			(unsigned long)ts, rem_nsec / 1000,
			ccic_id_string(type.id),
			ccic_dev_string(type.src),
			ccic_dev_string(type.dest),
			ccic_rprd_string(type.sub2),
			ccic_con_string(type.sub1));
		break;
	default:
		pr_info("%s undefined event\n", __func__);
		break;
	}
}

static int usblog_proc_show(struct seq_file *m, void *v)
{
	struct usblog_buf *temp_usblog_buffer;
	unsigned long long ts;
	unsigned long rem_nsec;
	int i;

	temp_usblog_buffer = usblog_root.usblog_buffer;

	if (!temp_usblog_buffer)
		goto err;

	seq_printf(m,
		"usblog CC IC version:");

	seq_printf(m,
		"hw version =%2x %2x %2x %2x\n",
		usblog_root.ccic_ver.hw_version[3],
		usblog_root.ccic_ver.hw_version[2],
		usblog_root.ccic_ver.hw_version[1],
		usblog_root.ccic_ver.hw_version[0]);

	seq_printf(m,
		"sw version =%2x %2x %2x %2x\n",
		usblog_root.ccic_ver.sw_main[2],
		usblog_root.ccic_ver.sw_main[1],
		usblog_root.ccic_ver.sw_main[0],
		usblog_root.ccic_ver.sw_boot);

	seq_printf(m,
		"\n\n");
	seq_printf(m,
		"usblog CCIC EVENT: count=%llu maxline=%d\n",
			temp_usblog_buffer->ccic_count, USBLOG_MAX_BUF2_SIZE);

	if (temp_usblog_buffer->ccic_count >= USBLOG_MAX_BUF2_SIZE) {
		for (i = temp_usblog_buffer->ccic_index;
			i < USBLOG_MAX_BUF2_SIZE; i++) {
			ts = temp_usblog_buffer->ccic_buffer[i].ts_nsec;
			rem_nsec = do_div(ts, 1000000000);
			print_ccic_event(m, ts, rem_nsec,
				temp_usblog_buffer->ccic_buffer[i].cc_type,
				&temp_usblog_buffer->ccic_buffer[i].noti);
		}
	}

	for (i = 0; i < temp_usblog_buffer->ccic_index; i++) {
		ts = temp_usblog_buffer->ccic_buffer[i].ts_nsec;
		rem_nsec = do_div(ts, 1000000000);
		print_ccic_event(m, ts, rem_nsec,
				temp_usblog_buffer->ccic_buffer[i].cc_type,
				&temp_usblog_buffer->ccic_buffer[i].noti);
	}

	seq_printf(m,
		"\n\n");
	seq_printf(m,
		"usblog USB_MODE: count=%llu maxline=%d\n",
			temp_usblog_buffer->mode_count, USBLOG_MAX_BUF_SIZE);

	if (temp_usblog_buffer->mode_count >= USBLOG_MAX_BUF_SIZE) {
		for (i = temp_usblog_buffer->mode_index;
			i < USBLOG_MAX_BUF_SIZE; i++) {
			ts = temp_usblog_buffer->mode_buffer[i].ts_nsec;
			rem_nsec = do_div(ts, 1000000000);
			seq_printf(m, "[%5lu.%06lu] %s\n", (unsigned long)ts,
				rem_nsec / 1000,
			temp_usblog_buffer->mode_buffer[i].usbmode_str);
		}
	}

	for (i = 0; i < temp_usblog_buffer->mode_index; i++) {
		ts = temp_usblog_buffer->mode_buffer[i].ts_nsec;
		rem_nsec = do_div(ts, 1000000000);
		seq_printf(m, "[%5lu.%06lu] %s\n", (unsigned long)ts,
			rem_nsec / 1000,
		temp_usblog_buffer->mode_buffer[i].usbmode_str);
	}
	seq_printf(m,
		"\n\n");
	seq_printf(m,
		"usblog USB STATE: count=%llu maxline=%d\n",
			temp_usblog_buffer->state_count, USBLOG_MAX_BUF_SIZE);

	if (temp_usblog_buffer->state_count >= USBLOG_MAX_BUF_SIZE) {
		for (i = temp_usblog_buffer->state_index;
			i < USBLOG_MAX_BUF_SIZE; i++) {
			ts = temp_usblog_buffer->state_buffer[i].ts_nsec;
			rem_nsec = do_div(ts, 1000000000);
			seq_printf(m, "[%5lu.%06lu] %s\n", (unsigned long)ts,
				rem_nsec / 1000,
			usbstate_string(temp_usblog_buffer->
						state_buffer[i].usbstate));
		}
	}

	for (i = 0; i < temp_usblog_buffer->state_index; i++) {
		ts = temp_usblog_buffer->state_buffer[i].ts_nsec;
		rem_nsec = do_div(ts, 1000000000);
		seq_printf(m, "[%5lu.%06lu] %s\n", (unsigned long)ts,
			rem_nsec / 1000,
		usbstate_string(temp_usblog_buffer->state_buffer[i].usbstate));
	}
	seq_printf(m,
		"\n\n");
	seq_printf(m,
		"usblog USB EVENT: count=%llu maxline=%d\n",
			temp_usblog_buffer->event_count, USBLOG_MAX_BUF_SIZE);

	if (temp_usblog_buffer->event_count >= USBLOG_MAX_BUF_SIZE) {
		for (i = temp_usblog_buffer->event_index;
			i < USBLOG_MAX_BUF_SIZE; i++) {
			ts = temp_usblog_buffer->event_buffer[i].ts_nsec;
			rem_nsec = do_div(ts, 1000000000);
			seq_printf(m, "[%5lu.%06lu] %s %s\n", (unsigned long)ts,
				rem_nsec / 1000,
			event_string(temp_usblog_buffer->event_buffer[i].event),
			status_string(temp_usblog_buffer->
					event_buffer[i].enable));
		}
	}

	for (i = 0; i < temp_usblog_buffer->event_index; i++) {
		ts = temp_usblog_buffer->event_buffer[i].ts_nsec;
		rem_nsec = do_div(ts, 1000000000);
		seq_printf(m, "[%5lu.%06lu] %s %s\n", (unsigned long)ts,
			rem_nsec / 1000,
		event_string(temp_usblog_buffer->event_buffer[i].event),
		status_string(temp_usblog_buffer->event_buffer[i].enable));
	}
	
	seq_printf(m,
		"\n\n");
	seq_printf(m,
		"usblog DWC: count=%llu maxline=%d\n",
			temp_usblog_buffer->dwc_count, USBLOG_MAX_BUF_SIZE);

	if (temp_usblog_buffer->dwc_count >= USBLOG_MAX_BUF_SIZE) {
		for (i = temp_usblog_buffer->dwc_index;
			i < USBLOG_MAX_BUF_SIZE; i++) {
			ts = temp_usblog_buffer->dwc_buffer[i].ts_nsec;
			rem_nsec = do_div(ts, 1000000000);
			seq_printf(m, "[%5lu.%06lu] %s prev_lpm:%d cur_lpm:%d\n",
			(unsigned long)ts,
				rem_nsec / 1000,
			dwcstate_string(temp_usblog_buffer->
				dwc_buffer[i].state),
			temp_usblog_buffer->dwc_buffer[i].prev_lpm,
			temp_usblog_buffer->dwc_buffer[i].cur_lpm);
		}
	}

	for (i = 0; i < temp_usblog_buffer->dwc_index; i++) {
		ts = temp_usblog_buffer->dwc_buffer[i].ts_nsec;
		rem_nsec = do_div(ts, 1000000000);
		seq_printf(m, "[%5lu.%06lu] %s prev_lpm:%d cur_lpm:%d\n",
		(unsigned long)ts,
			rem_nsec / 1000,
		dwcstate_string(temp_usblog_buffer->dwc_buffer[i].state),
		temp_usblog_buffer->dwc_buffer[i].prev_lpm,
		temp_usblog_buffer->dwc_buffer[i].cur_lpm);
	}
err:
	return 0;
}

static int usblog_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, usblog_proc_show, NULL);
}

static const struct file_operations usblog_proc_fops = {
	.open		= usblog_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void ccic_store_usblog_notify(int type, uint64_t *param1)
{
	struct ccic_buf *ccic_buffer;
	unsigned long long *target_count;
	unsigned long *target_index;

	target_count = &usblog_root.usblog_buffer->ccic_count;
	target_index = &usblog_root.usblog_buffer->ccic_index;
	ccic_buffer = &usblog_root.usblog_buffer->ccic_buffer[*target_index];
	if (ccic_buffer == NULL) {
		pr_err("%s target_buffer error\n", __func__);
		goto err;
	}
	ccic_buffer->ts_nsec = local_clock();
	ccic_buffer->cc_type = type;
	ccic_buffer->noti = *param1;

	*target_index = (*target_index+1)%USBLOG_MAX_BUF2_SIZE;
	(*target_count)++;
err:
	return;
}

void mode_store_usblog_notify(int type, char *param1)
{
	struct mode_buf *md_buffer;
	unsigned long long *target_count;
	unsigned long *target_index;
	char buf[256], buf2[4];
	char *b, *name;

	target_count = &usblog_root.usblog_buffer->mode_count;
	target_index = &usblog_root.usblog_buffer->mode_index;
	md_buffer = &usblog_root.usblog_buffer->mode_buffer[*target_index];
	if (md_buffer == NULL) {
		pr_err("%s target_buffer error\n", __func__);
		goto err;
	}
	md_buffer->ts_nsec = local_clock();

	strlcpy(buf, param1, sizeof(buf));
	b = strim(buf);
	if (b) {
		name = strsep(&b, ",");
		strlcpy(buf2, name, sizeof(buf2));
		strncpy(md_buffer->usbmode_str, buf2,
			sizeof(md_buffer->usbmode_str)-1);
	}
	while (b) {
		name = strsep(&b, ",");
		if (!name)
			continue;
		if (USBLOG_MAX_STRING_SIZE
			- strlen(md_buffer->usbmode_str) < 5) {
			strncpy(md_buffer->usbmode_str, "overflow",
					sizeof(md_buffer->usbmode_str)-1);
			b = NULL;
		} else {
			strncat(md_buffer->usbmode_str, ",", 1);
			strncat(md_buffer->usbmode_str, name, 3);
		}
	}

	*target_index = (*target_index+1)%USBLOG_MAX_BUF_SIZE;
	(*target_count)++;
err:
	return;
}

void state_store_usblog_notify(int type, char *param1)
{
	struct state_buf *st_buffer;
	unsigned long long *target_count;
	unsigned long *target_index;
	char buf[256], index;
	char *b, *name;
	int usbstate;

	target_count = &usblog_root.usblog_buffer->state_count;
	target_index = &usblog_root.usblog_buffer->state_index;
	st_buffer = &usblog_root.usblog_buffer->state_buffer[*target_index];
	if (st_buffer == NULL) {
		pr_err("%s target_buffer error\n", __func__);
		goto err;
	}
	st_buffer->ts_nsec = local_clock();

	strlcpy(buf, param1, sizeof(buf));
	b = strim(buf);
	name = strsep(&b, "=");

	index = *(b+USBLOG_CMP_INDEX);

	switch (index) {
	case 'F': /* CONFIGURED */
		usbstate = NOTIFY_CONFIGURED;
		break;
	case 'N':  /* CONNECTED */
		usbstate = NOTIFY_CONNECTED;
		break;
	case 'C':  /* DISCONNECTED */
		usbstate = NOTIFY_DISCONNECTED;
		break;
	case 'E':  /* RESET */
		usbstate = NOTIFY_RESET;
		break;
	case 'R':  /* ACCESSORY START */
		usbstate = NOTIFY_ACCSTART;
		break;
	case 'H':  /* HIGH SPEED */
		usbstate = NOTIFY_HIGH;
		break;
	case 'S':  /* SUPER SPEED */
		usbstate = NOTIFY_SUPER;
		break;
	default:
		pr_err("%s state param error. state=%s\n", __func__, param1);
		goto err;
	}

	st_buffer->usbstate = usbstate;

	*target_index = (*target_index+1)%USBLOG_MAX_BUF_SIZE;
	(*target_count)++;
err:
	return;
}

void event_store_usblog_notify(int type, unsigned long *param1, int *param2)
{
	struct event_buf *ev_buffer;
	unsigned long long *target_count;
	unsigned long *target_index;

	target_count = &usblog_root.usblog_buffer->event_count;
	target_index = &usblog_root.usblog_buffer->event_index;
	ev_buffer = &usblog_root.usblog_buffer->event_buffer[*target_index];
	if (ev_buffer == NULL) {
		pr_err("%s target_buffer error\n", __func__);
		goto err;
	}
	ev_buffer->ts_nsec = local_clock();
	ev_buffer->event = *param1;
	ev_buffer->enable = *param2;

	*target_index = (*target_index+1)%USBLOG_MAX_BUF_SIZE;
	(*target_count)++;
err:
	return;
}

void dwc_store_usblog_notify(int state, int *param1, int *param2)
{
	struct dwc_buf *dw_buffer;
	unsigned long long *target_count;
	unsigned long *target_index;

	target_count = &usblog_root.usblog_buffer->dwc_count;
	target_index = &usblog_root.usblog_buffer->dwc_index;
	dw_buffer = &usblog_root.usblog_buffer->dwc_buffer[*target_index];
	if (dw_buffer == NULL) {
		pr_err("%s target_buffer error\n", __func__);
		goto err;
	}
	dw_buffer->ts_nsec = local_clock();
	dw_buffer->state = *param1;
	dw_buffer->prev_lpm =  *param2 & NOTIFY_DWC_PREV_LPM ? 1 : 0;
	dw_buffer->cur_lpm = *param2 & NOTIFY_DWC_CUR_LPM ? 1 : 0;

	*target_index = (*target_index+1)%USBLOG_MAX_BUF_SIZE;
	(*target_count)++;
err:
	return;
}

void store_usblog_notify(int type, void *param1, void *param2)
{
	unsigned long flags = 0;
	uint64_t temp = 0;

	if (!usblog_root.init)
		register_usblog_proc();

	spin_lock_irqsave(&usblog_root.usblog_lock, flags);

	if (!usblog_root.usblog_buffer) {
		pr_err("%s usblog_buffer is null\n", __func__);
		spin_unlock_irqrestore(&usblog_root.usblog_lock, flags);
		return;
	}

	if (type == NOTIFY_FUNCSTATE) {
		temp = *(int *)param1;
		ccic_store_usblog_notify(type, &temp);
	} else if (type == NOTIFY_CCIC_EVENT
		|| type == NOTIFY_MANAGER)
		ccic_store_usblog_notify(type, (uint64_t *)param1);
	else if (type == NOTIFY_EVENT)
		event_store_usblog_notify(type,
			(unsigned long *)param1, (int *)param2);
	else  if (type == NOTIFY_USBMODE)
		mode_store_usblog_notify(type, (char *)param1);
	else if (type == NOTIFY_USBSTATE)
		state_store_usblog_notify(type, (char *)param1);
	else if (type == NOTIFY_DWC)
		dwc_store_usblog_notify(type,
			(int *)param1, (int *)param2);
	else
		pr_err("%s type error %d\n", __func__, type);

	spin_unlock_irqrestore(&usblog_root.usblog_lock, flags);
}
EXPORT_SYMBOL(store_usblog_notify);

void store_ccic_version(unsigned char *hw, unsigned char *sw_main,
			unsigned char *sw_boot)
{
	if (!hw || !sw_main || !sw_boot) {
		pr_err("%s null buffer\n", __func__);
		return;
	}

	memcpy(&usblog_root.ccic_ver.hw_version, hw, 4);
	memcpy(&usblog_root.ccic_ver.sw_main, sw_main, 3);
	memcpy(&usblog_root.ccic_ver.sw_boot, sw_boot, 1);
}
EXPORT_SYMBOL(store_ccic_version);

int register_usblog_proc(void)
{
	int ret = 0;

	if (usblog_root.init) {
		pr_err("%s already registered\n", __func__);
		goto err;
	}
	spin_lock_init(&usblog_root.usblog_lock);

	usblog_root.init = 1;

	proc_create("usblog", 0, NULL, &usblog_proc_fops);

	usblog_root.usblog_buffer
		= kzalloc(sizeof(struct usblog_buf), GFP_KERNEL);
	if (!usblog_root.usblog_buffer) {
		ret = -ENOMEM;
		goto err;
	}
	pr_info("%s size=%zu\n", __func__, sizeof(struct usblog_buf));
err:
	return ret;
}
EXPORT_SYMBOL(register_usblog_proc);

void unregister_usblog_proc(void)
{
	kfree(usblog_root.usblog_buffer);
	usblog_root.usblog_buffer = NULL;
	remove_proc_entry("usblog", NULL);
	usblog_root.init = 0;
}
EXPORT_SYMBOL(unregister_usblog_proc);

