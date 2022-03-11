/*
 * Copyright 2014  Samsung Electronics Co., Ltd
 *
 * Licensed under the Flora License, Version 1.1 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://floralicense.org/license/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h> /* malloc */
#include <string.h> /* strdup */
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <gio/gio.h>
#include <aul.h>
#include <dlog.h>

#include <com-core_packet.h>
#include <packet.h>
#include <widget_errno.h>
#include <widget_service.h>
#include <widget_service_internal.h>
#include <widget_cmd_list.h>
#include <widget_buffer.h>

#include "debug.h"
#include "fb.h"
#include "widget_viewer.h"
#include "widget_viewer_internal.h"
#include "dlist.h"
#include "util.h"
#include "master_rpc.h"
#include "client.h"
#include "conf.h"

#define EAPI __attribute__((visibility("default")))
#if !defined(WIDGET_COUNT_OF_SIZE_TYPE)
	#define WIDGET_COUNT_OF_SIZE_TYPE 13
#endif

#if defined(FLOG)
FILE *__file_log_fp;
#endif

static int default_launch_handler(widget_h handle, const char *appid, void *data);

static struct info {
	int init_count;
	int prevent_overwrite;
	guint job_timer;
	struct dlist *job_list;

	struct launch {
		int (*handle)(widget_h handle, const char *appid, void *data);
		void *data;
	} launch;
} s_info = {
	.init_count = 0,
	.prevent_overwrite = 0,
	.job_timer = 0,
	.job_list = NULL,
	.launch = {
		.handle = default_launch_handler,
		.data = NULL,
	},
};

static void widget_pixmap_acquired_cb(widget_h handle, const struct packet *result, void *data);
static void gbar_pixmap_acquired_cb(widget_h handle, const struct packet *result, void *data);
static void gbar_xpixmap_acquired_cb(widget_h handle, const struct packet *result, void *data);
static void widget_xpixmap_acquired_cb(widget_h handle, const struct packet *result, void *data);

static int default_launch_handler(widget_h handle, const char *appid, void *data)
{
	int ret;

	ret = aul_launch_app(appid, NULL);
	if (ret <= 0) {
		ErrPrint("Failed to launch an app %s (%d)\n", appid, ret);
	}

	/*
	   app_control_h service;

	   DbgPrint("AUTO_LAUNCH [%s]\n", handle->common->widget.auto_launch);

	   ret = app_control_create(&service);
	   if (ret == APP_CONTROL_ERROR_NONE) {
		   app_control_set_package(service, handle->common->widget.auto_launch);
		   app_control_send_launch_request(service, NULL, NULL);
		   app_control_destroy(service);
	   } else {
		   ErrPrint("Failed to launch an app %s (%d)\n", handle->common->widget.auto_launch, ret);
	   }
	*/

	return ret > 0 ? WIDGET_ERROR_NONE : WIDGET_ERROR_FAULT;
}

static inline void default_create_cb(widget_h handle, int ret, void *data)
{
	DbgPrint("Default created event handle: %d\n", ret);
}

static inline void default_pinup_cb(widget_h handle, int ret, void *data)
{
	DbgPrint("Default pinup event handle: %d\n", ret);
}

static inline void default_group_changed_cb(widget_h handle, int ret, void *data)
{
	DbgPrint("Default group changed event handle: %d\n", ret);
}

static inline void default_period_changed_cb(widget_h handle, int ret, void *data)
{
	DbgPrint("Default period changed event handle: %d\n", ret);
}

static inline void default_gbar_created_cb(widget_h handle, int ret, void *data)
{
	DbgPrint("Default GBAR created event handle: %d\n", ret);
}

static inline void default_gbar_destroyed_cb(widget_h handle, int ret, void *data)
{
	DbgPrint("Default GBAR destroyed event handle: %d\n", ret);
}

static inline void default_widget_size_changed_cb(widget_h handle, int ret, void *data)
{
	DbgPrint("Default WIDGET size changed event handle: %d\n", ret);
}

static inline void default_update_mode_cb(widget_h handle, int ret, void *data)
{
	DbgPrint("Default update mode set event handle: %d\n", ret);
}

static inline void default_access_event_cb(widget_h handle, int ret, void *data)
{
	DbgPrint("Default access event handle: %d\n", ret);
}

static inline void default_key_event_cb(widget_h handle, int ret, void *data)
{
	DbgPrint("Default key event handle: %d\n", ret);
}

static void update_mode_cb(widget_h handle, const struct packet *result, void *data)
{
	int ret;

	if (!result) {
		ret = WIDGET_ERROR_FAULT;
		goto errout;
	} else if (packet_get(result, "i", &ret) != 1) {
		ErrPrint("Invalid argument\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto errout;
	}

	if (ret < 0) {
		ErrPrint("Resize request is failed: %d\n", ret);
		goto errout;
	}

	return;

errout:
	handle->cbs.update_mode.cb(handle, ret, handle->cbs.update_mode.data);
	handle->cbs.update_mode.cb = NULL;
	handle->cbs.update_mode.data = NULL;
	handle->common->request.update_mode = 0;

	if (handle->common->state != WIDGET_STATE_DELETE) {
		if (ret == (int)WIDGET_ERROR_NOT_EXIST && handle->refcnt == 2) {
			_widget_invoke_event_handler(handle, WIDGET_EVENT_DELETED);
			_widget_unref(handle, 1);
		}
	}
}

static void resize_cb(widget_h handle, const struct packet *result, void *data)
{
	int ret;

	if (!result) {
		ret = WIDGET_ERROR_FAULT;
		goto errout;
	} else if (packet_get(result, "i", &ret) != 1) {
		ErrPrint("Invalid argument\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto errout;
	}

	/*!
	 * \note
	 * In case of resize request,
	 * The widget handle will not have resized value right after this callback,
	 * It can only get the new size when it makes updates.
	 *
	 * So the user can only get the resized value(result) from the first update event
	 * after this request.
	 */
	if (ret < 0) {
		ErrPrint("Resize request is failed: %d\n", ret);
		goto errout;
	}

	return;

errout:
	handle->cbs.size_changed.cb(handle, ret, handle->cbs.size_changed.data);
	handle->cbs.size_changed.cb = NULL;
	handle->cbs.size_changed.data = NULL;
	handle->common->request.size_changed = 0;

	if (handle->common->state != WIDGET_STATE_DELETE) {
		if (ret == (int)WIDGET_ERROR_NOT_EXIST && handle->refcnt == 2) {
			_widget_invoke_event_handler(handle, WIDGET_EVENT_DELETED);
			_widget_unref(handle, 1);
		}
	}
}

static void text_signal_cb(widget_h handle, const struct packet *result, void *data)
{
	int ret;
	void *cbdata;
	struct cb_info *info = data;
	widget_ret_cb cb;

	cbdata = info->data;
	cb = info->cb;
	_widget_destroy_cb_info(info);

	if (!result) {
		ret = WIDGET_ERROR_FAULT;
	} else if (packet_get(result, "i", &ret) != 1) {
		ErrPrint("Invalid argument\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (cb) {
		cb(handle, ret, cbdata);
	}
	return;
}

static void set_group_ret_cb(widget_h handle, const struct packet *result, void *data)
{
	int ret;

	if (!result) {
		ret = WIDGET_ERROR_FAULT;
		goto errout;
	} else if (packet_get(result, "i", &ret) != 1) {
		ErrPrint("Invalid argument\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto errout;
	}

	if (ret < 0) {
		goto errout;
	}

	return;

errout:
	handle->cbs.group_changed.cb(handle, ret, handle->cbs.group_changed.data);
	handle->cbs.group_changed.cb = NULL;
	handle->cbs.group_changed.data = NULL;
	handle->common->request.group_changed = 0;

	if (handle->common->state != WIDGET_STATE_DELETE) {
		if (ret == (int)WIDGET_ERROR_NOT_EXIST && handle->refcnt == 2) {
			_widget_invoke_event_handler(handle, WIDGET_EVENT_DELETED);
			_widget_unref(handle, 1);
		}
	}
}

static void period_ret_cb(widget_h handle, const struct packet *result, void *data)
{
	int ret;

	if (!result) {
		ret = WIDGET_ERROR_FAULT;
		goto errout;
	} else if (packet_get(result, "i", &ret) != 1) {
		ErrPrint("Invalid argument\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto errout;
	}

	if (ret < 0) {
		goto errout;
	}

	return;

errout:
	handle->cbs.period_changed.cb(handle, ret, handle->cbs.period_changed.data);
	handle->cbs.period_changed.cb = NULL;
	handle->cbs.period_changed.data = NULL;
	handle->common->request.period_changed = 0;

	if (handle->common->state != WIDGET_STATE_DELETE) {
		if (ret == (int)WIDGET_ERROR_NOT_EXIST && handle->refcnt == 2) {
			_widget_invoke_event_handler(handle, WIDGET_EVENT_DELETED);
			_widget_unref(handle, 1);
		}
	}
}

static void gbar_create_cb(widget_h handle, const struct packet *result, void *data)
{
	int ret;

	if (!result) {
		ret = WIDGET_ERROR_FAULT;
		goto errout;
	} else if (packet_get(result, "i", &ret) != 1) {
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto errout;
	}

	if (ret < 0) {
		ErrPrint("Failed to create a GBAR[%d]\n", ret);
		goto errout;
	}

	return;

errout:
	handle->cbs.gbar_created.cb(handle, ret, handle->cbs.gbar_created.data);
	handle->cbs.gbar_created.cb = NULL;
	handle->cbs.gbar_created.data = NULL;
	handle->common->request.gbar_created = 0;

	if (handle->common->state != WIDGET_STATE_DELETE) {
		if (ret == (int)WIDGET_ERROR_NOT_EXIST && handle->refcnt == 2) {
			_widget_invoke_event_handler(handle, WIDGET_EVENT_DELETED);
			_widget_unref(handle, 1);
		}
	}
}

static void activated_cb(widget_h handle, const struct packet *result, void *data)
{
	int ret;
	struct cb_info *info = data;
	void *cbdata;
	widget_ret_cb cb;
	const char *pkgname = "";

	cbdata = info->data;
	cb = info->cb;
	_widget_destroy_cb_info(info);

	if (!result) {
		ret = WIDGET_ERROR_FAULT;
	} else if (packet_get(result, "is", &ret, &pkgname) != 2) {
		ret = WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (cb) {
		cb(handle, ret, cbdata);
	}
}

static void gbar_destroy_cb(widget_h handle, const struct packet *result, void *data)
{
	int ret;
	widget_ret_cb cb;
	void *cbdata;
	struct cb_info *info = data;

	cbdata = info->data;
	cb = info->cb;
	_widget_destroy_cb_info(info);

	if (!result) {
		ErrPrint("Result is NIL (may connection lost)\n");
		ret = WIDGET_ERROR_FAULT;
	} else if (packet_get(result, "i", &ret) != 1) {
		ErrPrint("Invalid parameter\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (ret == (int)WIDGET_ERROR_NONE) {
		handle->cbs.gbar_destroyed.cb = cb;
		handle->cbs.gbar_destroyed.data = cbdata;
	} else {
		handle->common->is_gbar_created = 0;
		handle->common->request.gbar_destroyed = 0;

		if (cb) {
			cb(handle, ret, cbdata);
		}
	}
}

static void _delete_cluster_cb(widget_h handle, const struct packet *result, void *data)
{
	struct cb_info *info = data;
	int ret;
	widget_ret_cb cb;
	void *cbdata;

	cb = info->cb;
	cbdata = info->data;
	_widget_destroy_cb_info(info);

	if (!result) {
		ret = WIDGET_ERROR_FAULT;
	} else if (packet_get(result, "i", &ret) != 1) {
		ret = WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (cb) {
		cb(handle, ret, cbdata);
	}
}

static void _delete_category_cb(widget_h handle, const struct packet *result, void *data)
{
	struct cb_info *info = data;
	int ret;
	widget_ret_cb cb;
	void *cbdata;

	cb = info->cb;
	cbdata = info->data;
	_widget_destroy_cb_info(info);

	if (!result) {
		ret = WIDGET_ERROR_FAULT;
	} else if (packet_get(result, "i", &ret) != 1) {
		ret = WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (cb) {
		cb(handle, ret, cbdata);
	}
}

static int _widget_acquire_widget_pixmap(widget_h handle, widget_ret_cb cb, void *data)
{
	struct packet *packet;
	struct cb_info *cbinfo;
	const char *id;
	unsigned int cmd = CMD_WIDGET_ACQUIRE_PIXMAP;
	int ret;

	id = fb_id(handle->common->widget.fb);
	if (!id || strncasecmp(id, SCHEMA_PIXMAP, strlen(SCHEMA_PIXMAP))) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create((const char *)&cmd, "ss", handle->common->pkgname, handle->common->id);
	if (!packet) {
		ErrPrint("Failed to build a param\n");
		return WIDGET_ERROR_FAULT;
	}

	cbinfo = _widget_create_cb_info(cb, data);
	if (!cbinfo) {
		packet_destroy(packet);
		return WIDGET_ERROR_FAULT;
	}

	ret = master_rpc_async_request(handle, packet, 0, widget_pixmap_acquired_cb, cbinfo);
	if (ret < 0) {
		_widget_destroy_cb_info(cbinfo);
	}

	return ret;
}

static void widget_pixmap_acquired_cb(widget_h handle, const struct packet *result, void *data)
{
	int pixmap;
	int ret = WIDGET_ERROR_INVALID_PARAMETER;
	widget_ret_cb cb;
	void *cbdata;
	struct cb_info *info = data;

	cb = info->cb;
	cbdata = info->data;
	_widget_destroy_cb_info(info);

	if (!result) {
		pixmap = 0; /* PIXMAP 0 means error */
	} else if (packet_get(result, "ii", &pixmap, &ret) != 2) {
		pixmap = 0;
	}

	if (ret == (int)WIDGET_ERROR_RESOURCE_BUSY) {
		ret = _widget_acquire_widget_pixmap(handle, cb, cbdata);
		DbgPrint("Busy, Try again: %d\n", ret);
		/* Try again */
	} else if (ret == (int)WIDGET_ERROR_NOT_EXIST && handle->refcnt == 2) {
		if (cb) {
			cb(handle, pixmap, cbdata);
		}

		if (handle->common->state != WIDGET_STATE_DELETE) {
			_widget_invoke_event_handler(handle, WIDGET_EVENT_DELETED);
			_widget_unref(handle, 1);
		}
	} else {
		if (cb) {
			cb(handle, pixmap, cbdata);
		}
	}
}

static void widget_xpixmap_acquired_cb(widget_h handle, const struct packet *result, void *data)
{
	int pixmap;
	int ret = WIDGET_ERROR_INVALID_PARAMETER;
	widget_ret_cb cb;
	void *cbdata;
	struct cb_info *info = data;

	cb = info->cb;
	cbdata = info->data;
	_widget_destroy_cb_info(info);

	if (!result) {
		pixmap = 0;
	} else if (packet_get(result, "ii", &pixmap, &ret) != 2) {
		pixmap = 0;
	}

	if (cb) {
		cb(handle, pixmap, cbdata);
	}

	if (handle->common->state != WIDGET_STATE_DELETE) {
		if (ret == (int)WIDGET_ERROR_NOT_EXIST && handle->refcnt == 2) {
			_widget_invoke_event_handler(handle, WIDGET_EVENT_DELETED);
			_widget_unref(handle, 1);
		}
	}
}

static int widget_acquire_gbar_extra_pixmap(widget_h handle, int idx, widget_ret_cb cb, void *data)
{
	struct packet *packet;
	struct cb_info *cbinfo;
	unsigned int cmd = CMD_GBAR_ACQUIRE_XPIXMAP;
	int ret;

	packet = packet_create((const char *)&cmd, "ssi", handle->common->pkgname, handle->common->id, idx);
	if (!packet) {
		ErrPrint("Failed to build a param\n");
		return WIDGET_ERROR_FAULT;
	}

	cbinfo = _widget_create_cb_info(cb, data);
	if (!cbinfo) {
		packet_destroy(packet);
		return WIDGET_ERROR_FAULT;
	}

	ret = master_rpc_async_request(handle, packet, 0, gbar_xpixmap_acquired_cb, cbinfo);
	if (ret < 0) {
		/*!
		 * \note
		 * Packet will be destroyed by master_rpc_async_request
		 */
		_widget_destroy_cb_info(cbinfo);
	}

	return ret;
}

static int widget_acquire_widget_extra_pixmap(widget_h handle, int idx, widget_ret_cb cb, void *data)
{
	struct packet *packet;
	struct cb_info *cbinfo;
	unsigned int cmd = CMD_WIDGET_ACQUIRE_XPIXMAP;
	int ret;

	packet = packet_create((const char *)&cmd, "ssi", handle->common->pkgname, handle->common->id, idx);
	if (!packet) {
		ErrPrint("Failed to build a param\n");
		return WIDGET_ERROR_FAULT;
	}

	cbinfo = _widget_create_cb_info(cb, data);
	if (!cbinfo) {
		packet_destroy(packet);
		return WIDGET_ERROR_FAULT;
	}

	ret = master_rpc_async_request(handle, packet, 0, widget_xpixmap_acquired_cb, cbinfo);
	if (ret < 0) {
		/*!
		 * \note
		 * Packet will be destroyed by master_rpc_async_request
		 */
		_widget_destroy_cb_info(cbinfo);
	}

	return ret;
}

static int widget_acquire_gbar_pixmap(widget_h handle, widget_ret_cb cb, void *data)
{
	struct packet *packet;
	struct cb_info *cbinfo;
	unsigned int cmd = CMD_GBAR_ACQUIRE_PIXMAP;
	const char *id;
	int ret;

	id = fb_id(handle->common->gbar.fb);
	if (!id || strncasecmp(id, SCHEMA_PIXMAP, strlen(SCHEMA_PIXMAP))) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create((const char *)&cmd, "ss", handle->common->pkgname, handle->common->id);
	if (!packet) {
		ErrPrint("Failed to build a param\n");
		return WIDGET_ERROR_FAULT;
	}

	cbinfo = _widget_create_cb_info(cb, data);
	if (!cbinfo) {
		packet_destroy(packet);
		return WIDGET_ERROR_FAULT;
	}

	ret = master_rpc_async_request(handle, packet, 0, gbar_pixmap_acquired_cb, cbinfo);
	if (ret < 0) {
		/*!
		 * \note
		 * Packet will be destroyed by master_rpc_async_request
		 */
		_widget_destroy_cb_info(cbinfo);
	}

	return ret;
}

static void gbar_xpixmap_acquired_cb(widget_h handle, const struct packet *result, void *data)
{
	int pixmap;
	int ret;
	widget_ret_cb cb;
	void *cbdata;
	struct cb_info *info = data;

	cb = info->cb;
	cbdata = info->data;
	_widget_destroy_cb_info(info);

	if (!result) {
		pixmap = 0; /* PIXMAP 0 means error */
		ret = WIDGET_ERROR_FAULT;
	} else if (packet_get(result, "ii", &pixmap, &ret) != 2) {
		pixmap = 0;
		ret = WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (cb) {
		DbgPrint("ret: %x, pixmap: %d\n", ret, pixmap);
		cb(handle, pixmap, cbdata);
	}

	if (handle->common->state != WIDGET_STATE_DELETE) {
		if (ret == (int)WIDGET_ERROR_NOT_EXIST && handle->refcnt == 2) {
			_widget_invoke_event_handler(handle, WIDGET_EVENT_DELETED);
			_widget_unref(handle, 1);
		}
	}
}

static void gbar_pixmap_acquired_cb(widget_h handle, const struct packet *result, void *data)
{
	int pixmap;
	int ret;
	widget_ret_cb cb;
	void *cbdata;
	struct cb_info *info = data;

	cb = info->cb;
	cbdata = info->data;
	_widget_destroy_cb_info(info);

	if (!result) {
		pixmap = 0; /* PIXMAP 0 means error */
		ret = WIDGET_ERROR_FAULT;
	} else if (packet_get(result, "ii", &pixmap, &ret) != 2) {
		pixmap = 0;
		ret = WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (ret == (int)WIDGET_ERROR_RESOURCE_BUSY) {
		ret = widget_acquire_gbar_pixmap(handle, cb, cbdata);
		DbgPrint("Busy, Try again: %d\n", ret);
		/* Try again */
	} else if (ret == (int)WIDGET_ERROR_NOT_EXIST && handle->refcnt == 2) {
		if (cb) {
			cb(handle, pixmap, cbdata);
		}

		if (handle->common->state != WIDGET_STATE_DELETE) {
			_widget_invoke_event_handler(handle, WIDGET_EVENT_DELETED);
			_widget_unref(handle, 1);
		}
	} else {
		if (cb) {
			DbgPrint("ret: %d, pixmap: %d\n", ret, pixmap);
			cb(handle, pixmap, cbdata);
		}
	}
}

static void pinup_done_cb(widget_h handle, const struct packet *result, void *data)
{
	int ret;

	if (!result) {
		ret = WIDGET_ERROR_FAULT;
		goto errout;
	} else if (packet_get(result, "i", &ret) != 1) {
		goto errout;
	}

	if (ret < 0) {
		goto errout;
	}

	return;

errout:
	handle->cbs.pinup.cb(handle, ret, handle->cbs.pinup.data);
	handle->cbs.pinup.cb = NULL;
	handle->cbs.pinup.data = NULL;
	handle->common->request.pinup = 0;

	if (handle->common->state != WIDGET_STATE_DELETE) {
		if (ret == (int)WIDGET_ERROR_NOT_EXIST && handle->refcnt == 2) {
			_widget_invoke_event_handler(handle, WIDGET_EVENT_DELETED);
			_widget_unref(handle, 1);
		}
	}
}

static void key_ret_cb(widget_h handle, const struct packet *result, void *data)
{
	int ret;

	if (!result) {
		ret = WIDGET_ERROR_FAULT;
		return;
	}

	if (packet_get(result, "i", &ret) != 1) {
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		return;
	}

	if (ret != WIDGET_ERROR_NONE) {
		goto errout;
	}

	return;
errout:
	handle->cbs.key_event.cb(handle, ret, handle->cbs.key_event.data);
	handle->cbs.key_event.cb = NULL;
	handle->cbs.key_event.data = NULL;
	handle->common->request.key_event = 0;

	if (handle->common->state != WIDGET_STATE_DELETE) {
		if (ret == (int)WIDGET_ERROR_NOT_EXIST && handle->refcnt == 2) {
			_widget_invoke_event_handler(handle, WIDGET_EVENT_DELETED);
			_widget_unref(handle, 1);
		}
	}
}

static void access_ret_cb(widget_h handle, const struct packet *result, void *data)
{
	int ret;

	if (!result) {
		ret = WIDGET_ERROR_FAULT;
		return;
	}

	if (packet_get(result, "i", &ret) != 1) {
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		return;
	}

	if (ret != WIDGET_ERROR_NONE) {
		goto errout;
	}

	return;

errout:
	handle->cbs.access_event.cb(handle, ret, handle->cbs.access_event.data);
	handle->cbs.access_event.cb = NULL;
	handle->cbs.access_event.data = NULL;
	handle->common->request.access_event = 0;

	if (handle->common->state != WIDGET_STATE_DELETE) {
		if (ret == (int)WIDGET_ERROR_NOT_EXIST && handle->refcnt == 2) {
			_widget_invoke_event_handler(handle, WIDGET_EVENT_DELETED);
			_widget_unref(handle, 1);
		}
	}
}

static int send_access_event(widget_h handle, const char *event, int x, int y, int type)
{
	struct packet *packet;
	double timestamp;

	timestamp = util_timestamp();

	packet = packet_create(event, "ssdiii", handle->common->pkgname, handle->common->id, timestamp, x, y, type);
	if (!packet) {
		ErrPrint("Failed to build packet\n");
		return WIDGET_ERROR_FAULT;
	}

	return master_rpc_async_request(handle, packet, 0, access_ret_cb, NULL);
}

static int send_key_event(widget_h handle, const char *event, unsigned int keycode, int device)
{
	struct packet *packet;
	double timestamp;

	timestamp = util_timestamp();
	packet = packet_create(event, "ssdii", handle->common->pkgname, handle->common->id, timestamp, keycode, device);
	if (!packet) {
		ErrPrint("Failed to build packet\n");
		return WIDGET_ERROR_FAULT;
	}

	return master_rpc_async_request(handle, packet, 0, key_ret_cb, NULL);
}

static int send_mouse_event(widget_h handle, const char *event, int x, int y, double ratio_w, double ratio_h, int device)
{
	struct packet *packet;
	double timestamp;

	timestamp = util_timestamp();
	packet = packet_create_noack(event, "ssdiiiddi", handle->common->pkgname, handle->common->id, timestamp, x, y, INPUT_EVENT_SOURCE_VIEWER, ratio_w, ratio_h, device);
	if (!packet) {
		ErrPrint("Failed to build param\n");
		return WIDGET_ERROR_FAULT;
	}

	return master_rpc_request_only(handle, packet);
}

static int initialize_widget(void *disp, int use_thread)
{
	int ret;
#if defined(FLOG)
	char filename[BUFSIZ];
	snprintf(filename, sizeof(filename), "/tmp/%d.box.log", getpid());
	__file_log_fp = fopen(filename, "w+t");
	if (!__file_log_fp) {
		__file_log_fp = fdopen(1, "w+t");
	}
#endif
	ret = widget_service_init();
	if (ret != WIDGET_ERROR_NONE) {
		return ret;
	}

	ret = fb_init(disp);
	if (ret != WIDGET_ERROR_NONE) {
		widget_service_fini();
		return ret;
	}

	ret = client_init(use_thread);
	if (ret != WIDGET_ERROR_NONE) {
		fb_fini();
		widget_service_fini();
		return ret;
	}

	s_info.init_count++;
	return ret;
}

static inline char *_widget_pkgname(const char *pkgname)
{
	char *widget;

	widget = widget_service_get_widget_id(pkgname);
	if (!widget) {
		widget = strdup(pkgname);
	}

	return widget;
}

static gboolean job_execute_cb(void *data)
{
	struct job_item *item;
	struct dlist *l;

	l = dlist_nth(s_info.job_list, 0);
	if (!l) {
		s_info.job_timer = 0;
		return FALSE;
	}

	item = dlist_data(l);
	s_info.job_list = dlist_remove(s_info.job_list, l);

	if (item) {
		item->cb(item->handle, item->ret, item->data);
		_widget_unref(item->handle, 1);
		free(item);
	}

	return TRUE;
}

static int job_add(widget_h handle, widget_ret_cb job_cb, int ret, void *data)
{
	struct job_item *item;

	if (!job_cb) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	item = malloc(sizeof(*item));
	if (!item) {
		ErrPrint("Heap: %d\n", errno);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	item->handle = _widget_ref(handle);
	item->cb = job_cb;
	item->data = data;
	item->ret = ret;

	s_info.job_list = dlist_append(s_info.job_list, item);

	if (!s_info.job_timer) {
		s_info.job_timer = g_timeout_add(1, job_execute_cb, NULL);
		if (!s_info.job_timer) {
			ErrPrint("Failed to create a job timer\n");
		}
	}

	return WIDGET_ERROR_NONE;
}

static void new_ret_cb(widget_h handle, const struct packet *result, void *data)
{
	int ret;
	struct cb_info *info = data;
	widget_ret_cb cb;
	void *cbdata;

	cb = info->cb;
	cbdata = info->data;
	_widget_destroy_cb_info(info);

	if (!result) {
		ret = WIDGET_ERROR_FAULT;
	} else if (packet_get(result, "i", &ret) != 1) {
		ret = WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (ret >= 0) {
		handle->cbs.created.cb = cb;
		handle->cbs.created.data = cbdata;

		/*!
		 * \note
		 * Don't go anymore ;)
		 */
		return;
	} else if (cb) {
		/*!
		 * \note
		 * It means the current instance is not created,
		 * so user has to know about this.
		 * notice it to user using "deleted" event.
		 */
		cb(handle, ret, cbdata);
	}

	_widget_unref(handle, 1);
}

static int create_real_instance(widget_h handle, widget_ret_cb cb, void *data)
{
	struct cb_info *cbinfo;
	struct packet *packet;
	struct widget_common *common;
	unsigned int cmd = CMD_NEW;
	int ret;

	common = handle->common;

	packet = packet_create((const char *)&cmd, "dssssdii",
			common->timestamp, common->pkgname, common->content,
			common->cluster, common->category,
			common->widget.period, common->widget.width, common->widget.height);
	if (!packet) {
		ErrPrint("Failed to create a new packet\n");
		set_last_result(WIDGET_ERROR_FAULT);
		return WIDGET_ERROR_FAULT;
	}

	cbinfo = _widget_create_cb_info(cb, data);
	if (!cbinfo) {
		ErrPrint("Failed to create a cbinfo\n");
		packet_destroy(packet);
		set_last_result(WIDGET_ERROR_OUT_OF_MEMORY);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	/*!
	 * \note
	 * master_rpc_async_request will destroy the packet (decrease the refcnt)
	 * So be aware the packet object after return from master_rpc_async_request.
	 */
	ret = master_rpc_async_request(handle, packet, 0, new_ret_cb, cbinfo);
	if (ret < 0) {
		ErrPrint("Failed to send a new packet\n");
		_widget_destroy_cb_info(cbinfo);
		set_last_result(WIDGET_ERROR_FAULT);
		return WIDGET_ERROR_FAULT;
	}
	handle->common->request.created = 1;
	return WIDGET_ERROR_NONE;
}

static void create_cb(widget_h handle, int ret, void *data)
{
	struct cb_info *cbinfo = data;

	if (cbinfo->cb) {
		cbinfo->cb(handle, ret, cbinfo->data);
	}

	_widget_destroy_cb_info(cbinfo);

	/*!
	 * \note
	 * Forcely generate "updated" event
	 */
	_widget_invoke_event_handler(handle, WIDGET_EVENT_WIDGET_UPDATED);
}

static int create_fake_instance(widget_h handle, widget_ret_cb cb, void *data)
{
	struct cb_info *cbinfo;

	cbinfo = _widget_create_cb_info(cb, data);
	if (!cbinfo) {
		ErrPrint("Failed to create a cbinfo\n");
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	if (job_add(handle, create_cb, WIDGET_ERROR_NONE, cbinfo) != WIDGET_ERROR_NONE) {
		_widget_destroy_cb_info(cbinfo);
	}

	return WIDGET_ERROR_NONE;
}

static void refresh_for_paused_updating_cb(widget_h handle, int ret, void *data)
{
	if (handle->paused_updating == 0) {
		DbgPrint("Paused updates are cleared\n");
		return;
	}

	DbgPrint("Pending updates are found\n");
	_widget_invoke_event_handler(handle, WIDGET_EVENT_WIDGET_UPDATED);
}

static int _widget_set_visibility(widget_h handle, widget_visible_state_e state)
{
	struct packet *packet;
	int need_to_add_job = 0;
	unsigned int cmd = CMD_CHANGE_VISIBILITY;
	int ret;

	if (handle->common->visible != WIDGET_SHOW && state == WIDGET_SHOW) {
		need_to_add_job = !!handle->paused_updating;
	} else if (handle->common->visible == WIDGET_SHOW && state != WIDGET_SHOW) {
		if (!!_widget_find_widget_in_show(handle->common)) {
			return WIDGET_ERROR_NONE;
		}
	} else if (handle->common->visible == WIDGET_SHOW && state == WIDGET_SHOW && handle->paused_updating) {
		if (job_add(handle, refresh_for_paused_updating_cb, WIDGET_ERROR_NONE, NULL) < 0) {
			ErrPrint("Unable to add a new job for refreshing box\n");
		}

		return WIDGET_ERROR_NONE;
	} else {
		/*!
		 * \brief
		 * No need to send this to the master
		 */
		return WIDGET_ERROR_NONE;
	}

	packet = packet_create_noack((const char *)&cmd, "ssi", handle->common->pkgname, handle->common->id, (int)state);
	if (!packet) {
		ErrPrint("Failed to create a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = master_rpc_request_only(handle, packet);
	if (ret == (int)WIDGET_ERROR_NONE) {
		DbgPrint("[%s] visibility is changed 0x[%x]\n", handle->common->pkgname, state);
		handle->common->visible = state;

		if (need_to_add_job) {
			if (job_add(handle, refresh_for_paused_updating_cb, WIDGET_ERROR_NONE, NULL) < 0) {
				ErrPrint("Unable to add a new job for refreshing box\n");
			}
		}
	}

	return ret;
}

static void _widget_update_visibility(struct widget_common *old_common)
{
	widget_h item;

	item = _widget_find_widget_in_show(old_common);
	if (!item) {
		item = _widget_get_widget_nth(old_common, 0);
		if (item) {
			_widget_set_visibility(item, WIDGET_HIDE_WITH_PAUSE);
		} else {
			ErrPrint("Unable to get the valid handle from common handle\n");
		}
	} else {
		_widget_set_visibility(item, WIDGET_SHOW);
	}
}

/*!
 * \note
 * The second parameter should be the "return value",
 * But in this case, we will use it for "type of deleting instance".
 */
static void _job_del_cb(widget_h handle, int type, void *data)
{
	struct cb_info *cbinfo = data;
	widget_ret_cb cb;

	if (handle->visible == WIDGET_SHOW) {
		_widget_update_visibility(handle->common);
	}

	cb = cbinfo->cb;
	data = cbinfo->data;
	_widget_destroy_cb_info(cbinfo);

	if (handle->common->state != WIDGET_STATE_CREATE) {
		DbgPrint("[%s] %d\n", handle->common->pkgname, handle->refcnt);
		if (cb) {
			cb(handle, WIDGET_ERROR_NONE, data);
		}

		return;
	}

	if (handle->common->refcnt == 1) {
		handle->common->delete_type = type;
		handle->common->state = WIDGET_STATE_DELETE;

		if (!handle->common->id) {
			/*!
			 * \note
			 * The id is not determined yet.
			 * It means a user didn't receive created event yet.
			 * Then just stop to delete procedure from here.
			 * Because the "created" event handle will release this.
			 * By the way, if the user adds any callback for getting return status of this,
			 * call it at here.
			 */
			if (cb) {
				cb(handle, WIDGET_ERROR_NONE, data);
			}
		}

		DbgPrint("Send delete request [%s]\n", handle->common->id);
		_widget_send_delete(handle, type, cb, data);
	} else {
		if (cb) {
			cb(handle, WIDGET_ERROR_NONE, data);
		}

		DbgPrint("Before unref: %d\n", handle->common->refcnt);
		_widget_unref(handle, 1);
	}
}

static void _resize_job_cb(widget_h handle, int ret, void *data)
{
	struct cb_info *info = data;

	if (info->cb) {
		info->cb(handle, ret, info->data);
	}

	free(info);

	/*!
	 * \note
	 * Forcely update the box
	 */
	_widget_invoke_event_handler(handle, WIDGET_EVENT_WIDGET_UPDATED);
}

static void _turn_off_gbar_destroyed_flag_cb(widget_h handle, int ret, void *data)
{
	if (handle->common->request.gbar_destroyed) {
		widget_ret_cb cb;
		void *data;

		DbgPrint("gbar_destroyed request is canceled\n");
		handle->common->request.gbar_destroyed = 0;
		cb = handle->cbs.gbar_destroyed.cb;
		data = handle->cbs.gbar_destroyed.data;
		handle->cbs.gbar_destroyed.cb = NULL;
		handle->cbs.gbar_destroyed.data = NULL;

		if (cb) {
			cb(handle, ret, data);
		}
	}
}

static void _turn_off_gbar_created_flag_cb(widget_h handle, int ret, void *data)
{
	if (handle->common->request.gbar_created) {
		widget_ret_cb cb;
		void *data;

		DbgPrint("gbar_created request is canceled\n");
		handle->common->request.gbar_created = 0;
		cb = handle->cbs.gbar_created.cb;
		data = handle->cbs.gbar_created.data;
		handle->cbs.gbar_created.cb = NULL;
		handle->cbs.gbar_created.data = NULL;

		if (cb) {
			cb(handle, ret, data);
		}
	}
}

EAPI int widget_viewer_init(void *disp, int prevent_overwrite, double event_filter, int use_thread)
{
	if (s_info.init_count > 0) {
		s_info.init_count++;
		return WIDGET_ERROR_NONE;
	}

	/*!
	 * \note
	 * Some application doesn't want to use the environment value.
	 * So set them using arguments.
	 */
	s_info.prevent_overwrite = prevent_overwrite;
	conf_set_event_filter(event_filter);

	return initialize_widget(disp, use_thread);
}

EAPI int widget_viewer_fini(void)
{
	if (s_info.init_count <= 0) {
		ErrPrint("Doesn't initialized\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	s_info.init_count--;
	if (s_info.init_count > 0) {
		ErrPrint("init count : %d\n", s_info.init_count);
		return WIDGET_ERROR_NONE;
	}

	client_fini();
	fb_fini();
	widget_service_fini();
	return WIDGET_ERROR_NONE;
}

EAPI widget_h widget_viewer_add_widget(const char *pkgname, const char *content, const char *cluster, const char *category, double period, widget_size_type_e type, widget_ret_cb cb, void *data)
{
	char *widgetid;
	widget_h handle;
	int w = 0;
	int h = 0;

	if (!pkgname || !cluster || !category) {
		ErrPrint("Invalid arguments: pkgname[%p], cluster[%p], category[%p]\n",
				pkgname, cluster, category);
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	widgetid = _widget_pkgname(pkgname);
	if (!widgetid) {
		ErrPrint("Invalid package: %s\n", pkgname);
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	if (widget_service_is_enabled(widgetid) == 0) {
		DbgPrint("Livebox [%s](%s) is disabled package\n", widgetid, pkgname);
		free(widgetid);
		set_last_result(WIDGET_ERROR_DISABLED);
		return NULL;
	}

	if (type != WIDGET_SIZE_TYPE_UNKNOWN) {
		(void)widget_service_get_size(type, &w, &h);
	}

	handle = calloc(1, sizeof(*handle));
	if (!handle) {
		ErrPrint("Error: %d\n", errno);
		free(widgetid);
		set_last_result(WIDGET_ERROR_OUT_OF_MEMORY);
		return NULL;
	}

	if (!cb) {
		cb = default_create_cb;
	}

	handle->common = _widget_find_sharable_common_handle(widgetid, content, w, h, cluster, category);
	if (!handle->common) {
		handle->common = _widget_create_common_handle(handle, widgetid, cluster, category);
		free(widgetid);
		if (!handle->common) {
			ErrPrint("Failed to find common handle\n");
			free(handle);
			return NULL;
		}

		if (!content || !strlen(content)) {
			char *pc;
			/**
			 * @note
			 * I know the content should not be modified. use it temporarly without "const"
			 */
			pc = widget_service_get_content_string(handle->common->pkgname);
			_widget_set_content(handle->common, pc);
			free(pc);
		} else {
			_widget_set_content(handle->common, content);
		}

		_widget_set_period(handle->common, period);
		_widget_set_size(handle->common, w, h);
		_widget_common_ref(handle->common, handle);

		if (create_real_instance(handle, cb, data) < 0) {
			if (_widget_common_unref(handle->common, handle) == 0) {
				/*!
				 * Delete common
				 */
				_widget_destroy_common_handle(handle->common);
				handle->common = NULL;
			}
			free(handle);
			return NULL;
		}
	} else {
		free(widgetid);

		_widget_common_ref(handle->common, handle);

		if (handle->common->request.created) {
			/*!
			 * If a box is in creating, wait its result too
			 */
			handle->cbs.created.cb = cb;
			handle->cbs.created.data = data;
		} else {
			/*!
			 * or fire the fake created_event
			 */
			if (create_fake_instance(handle, cb, data) < 0) {
				if (_widget_common_unref(handle->common, handle) == 0) {
					/*!
					 * Delete common
					 */
					_widget_destroy_common_handle(handle->common);
				}
				free(handle);
				return NULL;
			}
		}
	}

	handle->visible = WIDGET_HIDE_WITH_PAUSE;
	handle->state = WIDGET_STATE_CREATE;
	handle = _widget_ref(handle);

	return handle;
}

EAPI int widget_viewer_get_period(widget_h handle, double *period)
{
	int ret = WIDGET_ERROR_NONE;
	double result_period = -1.0f;

	if (!handle || handle->state != WIDGET_STATE_CREATE || period == NULL) {
		ErrPrint("Handler is not valid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto out;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Invalid handle\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto out;
	}

	if (!handle->common->id) {
		ErrPrint("Hnalder is not valid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto out;
	}

	result_period = handle->common->widget.period;

out:
	if (period)
		*period = result_period;

	return ret;
}

EAPI int widget_viewer_set_period(widget_h handle, double period, widget_ret_cb cb, void *data)
{
	struct packet *packet;
	unsigned int cmd = CMD_SET_PERIOD;
	int ret;

	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Invalid handle\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (handle->common->request.period_changed) {
		ErrPrint("Previous request for changing period is not finished\n");
		return WIDGET_ERROR_RESOURCE_BUSY;
	}

	if (!handle->common->is_user) {
		ErrPrint("CA Livebox is not able to change the period\n");
		return WIDGET_ERROR_PERMISSION_DENIED;
	}

	if (handle->common->widget.period == period) {
		DbgPrint("No changes\n");
		return WIDGET_ERROR_ALREADY_EXIST;
	}

	packet = packet_create((const char *)&cmd, "ssd", handle->common->pkgname, handle->common->id, period);
	if (!packet) {
		ErrPrint("Failed to build a packet %s\n", handle->common->pkgname);
		return WIDGET_ERROR_FAULT;
	}

	if (!cb) {
		cb = default_period_changed_cb;
	}

	ret = master_rpc_async_request(handle, packet, 0, period_ret_cb, NULL);
	if (ret == (int)WIDGET_ERROR_NONE) {
		handle->cbs.period_changed.cb = cb;
		handle->cbs.period_changed.data = data;
		handle->common->request.period_changed = 1;
	}

	return ret;
}

EAPI int widget_viewer_delete_widget(widget_h handle, widget_delete_type_e type, widget_ret_cb cb, void *data)
{
	struct cb_info *cbinfo;

	if (!handle) {
		ErrPrint("Handler is NIL\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is already deleted\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	handle->state = WIDGET_STATE_DELETE;

	cbinfo = _widget_create_cb_info(cb, data);
	if (!cbinfo) {
		ErrPrint("Failed to create a cbinfo\n");
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	if (job_add(handle, _job_del_cb, type, cbinfo) != WIDGET_ERROR_NONE) {
		ErrPrint("Failed to add a new job\n");
		_widget_destroy_cb_info(cbinfo);
		return WIDGET_ERROR_FAULT;
	}

	return WIDGET_ERROR_NONE;
}

EAPI int widget_viewer_add_fault_handler(widget_fault_handler_cb widget_cb, void *data)
{
	if (!widget_cb) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return _widget_add_fault_handler(widget_cb, data);
}

EAPI void *widget_viewer_remove_fault_handler(widget_fault_handler_cb widget_cb)
{
	if (!widget_cb) {
		return NULL;
	}

	return _widget_remove_fault_handler(widget_cb);
}

EAPI int widget_viewer_add_event_handler(widget_event_handler_cb widget_cb, void *data)
{
	if (!widget_cb) {
		ErrPrint("Invalid argument widget_cb is nil\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return _widget_add_event_handler(widget_cb, data);
}

EAPI void *widget_viewer_remove_event_handler(widget_event_handler_cb widget_cb)
{
	if (!widget_cb) {
		return NULL;
	}

	return _widget_remove_event_handler(widget_cb);
}

EAPI int widget_viewer_set_update_mode(widget_h handle, int active_update, widget_ret_cb cb, void *data)
{
	struct packet *packet;
	unsigned int cmd = CMD_UPDATE_MODE;
	int ret;

	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is Invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is Invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is Invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (handle->common->request.update_mode) {
		ErrPrint("Previous update_mode cb is not finished yet\n");
		return WIDGET_ERROR_RESOURCE_BUSY;
	}

	if (handle->common->is_active_update == active_update) {
		return WIDGET_ERROR_ALREADY_EXIST;
	}

	if (!handle->common->is_user) {
		return WIDGET_ERROR_PERMISSION_DENIED;
	}

	packet = packet_create((const char *)&cmd, "ssi", handle->common->pkgname, handle->common->id, active_update);
	if (!packet) {
		return WIDGET_ERROR_FAULT;
	}

	if (!cb) {
		cb = default_update_mode_cb;
	}

	ret = master_rpc_async_request(handle, packet, 0, update_mode_cb, NULL);
	if (ret == (int)WIDGET_ERROR_NONE) {
		handle->cbs.update_mode.cb = cb;
		handle->cbs.update_mode.data = data;
		handle->common->request.update_mode = 1;
	}

	return ret;
}

EAPI int widget_viewer_is_active_update(widget_h handle)
{
	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is Invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is Invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return handle->common->is_active_update;
}

EAPI int widget_viewer_resize_widget(widget_h handle, widget_size_type_e type, widget_ret_cb cb, void *data)
{
	struct widget_common *common;
	int w;
	int h;
	int ret;

	/*!
	 * \TODO
	 * If this handle is host instance or link instance,
	 * Create a new instance or find another linkable instance.
	 */

	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Invalid handle\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	/*!
	 * \note
	 * resize operation should be separated by each handle.
	 * If a handle is resizing, the other handle can request resize too.
	 * So we should not use the common->request.size_changed flag.
	 */
	if (handle->cbs.size_changed.cb) {
		ErrPrint("Previous resize request is not finished yet\n");
		return WIDGET_ERROR_RESOURCE_BUSY;
	}

	if (widget_service_get_size(type, &w, &h) != 0) {
		ErrPrint("Invalid size type\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (handle->common->widget.width == w && handle->common->widget.height == h) {
		DbgPrint("No changes\n");
		return WIDGET_ERROR_ALREADY_EXIST;
	}

	if (!handle->common->is_user) {
		ErrPrint("CA Livebox is not able to be resized\n");
		return WIDGET_ERROR_PERMISSION_DENIED;
	}

	if (handle->common->refcnt <= 1) {
		struct packet *packet;
		unsigned int cmd = CMD_RESIZE;

		/* Only 1 instance */
		packet = packet_create((const char *)&cmd, "ssii", handle->common->pkgname, handle->common->id, w, h);
		if (!packet) {
			ErrPrint("Failed to build param\n");
			return WIDGET_ERROR_FAULT;
		}

		if (!cb) {
			cb = default_widget_size_changed_cb;
		}

		ret = master_rpc_async_request(handle, packet, 0, resize_cb, NULL);
		if (ret == (int)WIDGET_ERROR_NONE) {
			handle->cbs.size_changed.cb = cb;
			handle->cbs.size_changed.data = data;
			handle->common->request.size_changed = 1;
		}
	} else {
		common = _widget_find_sharable_common_handle(handle->common->pkgname, handle->common->content, w, h, handle->common->cluster, handle->common->category);
		if (!common) {
			struct widget_common *old_common;
			/*!
			 * \note
			 * If the common handle is in resizing,
			 * if user tries to resize a hander, then simply create new one even if the requested size is same with this.

			 if (handle->common->request.size_changed) {
			 }

			 */

			old_common = handle->common;

			common = _widget_create_common_handle(handle, old_common->pkgname, old_common->cluster, old_common->category);
			if (!common) {
				ErrPrint("Failed to create common handle\n");
				return WIDGET_ERROR_FAULT;
			}

			_widget_set_size(common, w, h);
			_widget_set_content(common, old_common->content);
			_widget_set_period(common, old_common->widget.period);

			/*!
			 * \note
			 * Disconnecting from old one.
			 */
			if (_widget_common_unref(old_common, handle) == 0) {
				/*!
				 * \note
				 * Impossible
				 */
				ErrPrint("Common has no associated handle\n");
			}

			_widget_common_ref(common, handle);

			/*!
			 * Connect to a new one
			 */
			handle->common = common;

			/*!
			 * \TODO
			 * Need to care, if it fails to create a common handle,
			 * the resize operation will be failed.
			 * in that case, we should reuse the old common handle
			 */
			ret = create_real_instance(handle, cb, data);
			if (ret < 0) {
				_widget_common_unref(common, handle);
				_widget_destroy_common_handle(common);

				_widget_common_ref(old_common, handle);
				handle->common = old_common;
			} else {
				/*!
				 * In this case, we should update visibility of old_common's widgetes
				 */
				if (handle->visible == WIDGET_SHOW) {
					_widget_update_visibility(old_common);
				}
			}
		} else {
			struct cb_info *cbinfo;

			cbinfo = _widget_create_cb_info(cb, data);
			if (!cbinfo) {
				ErrPrint("Failed to create a cbinfo\n");
				ret = WIDGET_ERROR_OUT_OF_MEMORY;
			} else {
				ret = job_add(handle, _resize_job_cb, WIDGET_ERROR_NONE, cbinfo);
				if (ret == (int)WIDGET_ERROR_NONE) {
					struct widget_common *old_common;

					old_common = handle->common;

					if (_widget_common_unref(handle->common, handle) == 0) {
						ErrPrint("Old common has no associated handle\n");
					}

					_widget_common_ref(common, handle);
					handle->common = common;

					if (handle->visible == WIDGET_SHOW) {
						_widget_update_visibility(old_common); /* To update visibility: Show --> Paused */
						_widget_update_visibility(common);    /* To update visibility: Paused --> Show */
					}
				} else {
					_widget_destroy_cb_info(cbinfo);
				}
			}
		}
	}

	return ret;
}

EAPI int widget_viewer_send_click_event(widget_h handle, const char *event, double x, double y)
{
	struct packet *packet;
	double timestamp;
	unsigned int cmd = CMD_CLICKED;
	int ret;

	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!event || (strcmp(event, WIDGET_VIEWER_CLICK_BUTTON_LEFT) && strcmp(event, WIDGET_VIEWER_CLICK_BUTTON_RIGHT) && strcmp(event, WIDGET_VIEWER_CLICK_BUTTON_CENTER))) {
		ErrPrint("Unknown event: (%s)\n", event);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (handle->common->widget.auto_launch) {
		if (s_info.launch.handle) {
			ret = s_info.launch.handle(handle, handle->common->widget.auto_launch, s_info.launch.data);
			if (ret < 0) {
				ErrPrint("launch handle app %s (%d)\n", handle->common->widget.auto_launch, ret);
			}
		}
	}

	timestamp = util_timestamp();
	DbgPrint("CLICKED: %lf\n", timestamp);

	packet = packet_create_noack((const char *)&cmd, "sssddd", handle->common->pkgname, handle->common->id, event, timestamp, x, y);
	if (!packet) {
		ErrPrint("Failed to build param\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = master_rpc_request_only(handle, packet);
	return ret;
}

EAPI int widget_viewer_has_glance_bar(widget_h handle)
{
	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return !!handle->common->gbar.fb;
}

EAPI int widget_viewer_glance_bar_is_created(widget_h handle)
{
	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->gbar.fb || !handle->common->id) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return handle->common->is_gbar_created;
}

EAPI int widget_viewer_create_glance_bar(widget_h handle, double x, double y, widget_ret_cb cb, void *data)
{
	struct packet *packet;
	unsigned int cmd = CMD_CREATE_GBAR;
	int ret;

	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->gbar.fb || !handle->common->id) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	/*!
	 * \note
	 * Only one handle can have a GBAR
	 */
	if (handle->common->is_gbar_created) {
		DbgPrint("GBAR is already created\n");
		return WIDGET_ERROR_NONE;
	}

	if (handle->common->request.gbar_created) {
		ErrPrint("Previous request is not completed yet\n");
		return WIDGET_ERROR_RESOURCE_BUSY;
	}

	/*!
	 * \note
	 * Turn off the gbar_destroyed request flag
	 */
	if (handle->common->request.gbar_destroyed) {
		if (job_add(handle, _turn_off_gbar_destroyed_flag_cb, WIDGET_ERROR_CANCELED, NULL) < 0) {
			ErrPrint("Failed to add gbar_destroyed job\n");
		}
	}

	packet = packet_create((const char *)&cmd, "ssdd", handle->common->pkgname, handle->common->id, x, y);
	if (!packet) {
		ErrPrint("Failed to build param\n");
		return WIDGET_ERROR_FAULT;
	}

	if (!cb) {
		cb = default_gbar_created_cb;
	}

	DbgPrint("PERF_WIDGET\n");
	ret = master_rpc_async_request(handle, packet, 0, gbar_create_cb, NULL);
	if (ret == (int)WIDGET_ERROR_NONE) {
		handle->cbs.gbar_created.cb = cb;
		handle->cbs.gbar_created.data = data;
		handle->common->request.gbar_created = 1;
	}

	return ret;
}

EAPI int widget_viewer_move_glance_bar(widget_h handle, double x, double y)
{
	struct packet *packet;
	unsigned int cmd = CMD_GBAR_MOVE;

	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->gbar.fb || !handle->common->id) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->is_gbar_created) {
		ErrPrint("GBAR is not created\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "ssdd", handle->common->pkgname, handle->common->id, x, y);
	if (!packet) {
		ErrPrint("Failed to build param\n");
		return WIDGET_ERROR_FAULT;
	}

	return master_rpc_request_only(handle, packet);
}

EAPI int widget_viewer_activate_faulted_widget(const char *pkgname, widget_ret_cb cb, void *data)
{
	struct packet *packet;
	struct cb_info *cbinfo;
	unsigned int cmd = CMD_ACTIVATE_PACKAGE;
	int ret;

	if (!pkgname) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create((const char *)&cmd, "s", pkgname);
	if (!packet) {
		ErrPrint("Failed to build a param\n");
		return WIDGET_ERROR_FAULT;
	}

	cbinfo = _widget_create_cb_info(cb, data);
	if (!cbinfo) {
		ErrPrint("Unable to create cbinfo\n");
		packet_destroy(packet);
		return WIDGET_ERROR_FAULT;
	}

	ret = master_rpc_async_request(NULL, packet, 0, activated_cb, cbinfo);
	if (ret < 0) {
		_widget_destroy_cb_info(cbinfo);
	}

	return ret;
}

EAPI int widget_viewer_destroy_glance_bar(widget_h handle, widget_ret_cb cb, void *data)
{
	struct packet *packet;
	struct cb_info *cbinfo;
	unsigned int cmd = CMD_DESTROY_GBAR;
	int ret;

	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->gbar.fb || !handle->common->id) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	/*!
	 * \FIXME
	 * Replace the callback check code.
	 * Use the flag instead of callback.
	 * the flag should be in the ADT "common"
	 */
	if (!handle->common->is_gbar_created && !handle->common->request.gbar_created) {
		ErrPrint("GBAR is not created\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (handle->common->request.gbar_destroyed) {
		ErrPrint("GBAR destroy request is already sent\n");
		return WIDGET_ERROR_ALREADY_EXIST;
	}

	/*!
	 * \note
	 * Disable the gbar_created request flag
	 */
	if (handle->common->request.gbar_created) {
		if (job_add(handle, _turn_off_gbar_created_flag_cb, WIDGET_ERROR_CANCELED, NULL) < 0) {
			ErrPrint("Failed to add a new job\n");
		}
	}

	DbgPrint("[%s]\n", handle->common->pkgname);

	packet = packet_create((const char *)&cmd, "ss", handle->common->pkgname, handle->common->id);
	if (!packet) {
		ErrPrint("Failed to build a param\n");
		return WIDGET_ERROR_FAULT;
	}

	if (!cb) {
		cb = default_gbar_destroyed_cb;
	}

	cbinfo = _widget_create_cb_info(cb, data);
	if (!cbinfo) {
		packet_destroy(packet);
		return WIDGET_ERROR_FAULT;
	}

	ret = master_rpc_async_request(handle, packet, 0, gbar_destroy_cb, cbinfo);
	if (ret < 0) {
		_widget_destroy_cb_info(cbinfo);
	} else {
		handle->common->request.gbar_destroyed = 1;
	}

	return ret;
}

EAPI int widget_viewer_feed_access_event(widget_h handle, widget_access_event_type_e type, widget_access_event_info_s info, widget_ret_cb cb, void *data)
{
	int w = 1;
	int h = 1;
	unsigned int cmd;
	int ret = 0;    /* re-used for sending event type */

	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (handle->common->request.access_event) {
		ErrPrint("Previous access event is not yet done\n");
		return WIDGET_ERROR_RESOURCE_BUSY;
	}

	if (type & WIDGET_ACCESS_EVENT_GBAR_MASK) {
		if (!handle->common->is_gbar_created) {
			ErrPrint("GBAR is not created\n");
			return WIDGET_ERROR_INVALID_PARAMETER;
		}

		w = handle->common->gbar.width;
		h = handle->common->gbar.height;

		switch (type & ~(WIDGET_ACCESS_EVENT_GBAR_MASK | WIDGET_ACCESS_EVENT_WIDGET_MASK)) {
		case WIDGET_ACCESS_EVENT_HIGHLIGHT:
			cmd = CMD_GBAR_ACCESS_HL;
			ret = (int)info->type;
			break;
		case WIDGET_ACCESS_EVENT_ACTIVATE:
			cmd = CMD_GBAR_ACCESS_ACTIVATE;
			break;
		case WIDGET_ACCESS_EVENT_ACTION:
			cmd = CMD_GBAR_ACCESS_ACTION;
			ret = (int)info->type;
			break;
		case WIDGET_ACCESS_EVENT_SCROLL:
			cmd = CMD_GBAR_ACCESS_SCROLL;
			ret = (int)info->type;
			break;
		case WIDGET_ACCESS_EVENT_VALUE_CHANGE:
			cmd = CMD_GBAR_ACCESS_VALUE_CHANGE;
			break;
		case WIDGET_ACCESS_EVENT_MOUSE:
			cmd = CMD_GBAR_ACCESS_MOUSE;
			ret = (int)info->type;
			break;
		case WIDGET_ACCESS_EVENT_BACK:
			cmd = CMD_GBAR_ACCESS_BACK;
			break;
		case WIDGET_ACCESS_EVENT_OVER:
			cmd = CMD_GBAR_ACCESS_OVER;
			break;
		case WIDGET_ACCESS_EVENT_READ:
			cmd = CMD_GBAR_ACCESS_READ;
			break;
		case WIDGET_ACCESS_EVENT_ENABLE:
			cmd = CMD_GBAR_ACCESS_ENABLE;
			ret = info->type;
			break;
		default:
			return WIDGET_ERROR_INVALID_PARAMETER;
		}

	} else if (type & WIDGET_ACCESS_EVENT_WIDGET_MASK) {
		w = handle->common->widget.width;
		h = handle->common->widget.height;
		switch (type & ~(WIDGET_ACCESS_EVENT_GBAR_MASK | WIDGET_ACCESS_EVENT_WIDGET_MASK)) {
		case WIDGET_ACCESS_EVENT_HIGHLIGHT:
			cmd = CMD_WIDGET_ACCESS_HL;
			ret = (int)info->type;
			break;
		case WIDGET_ACCESS_EVENT_ACTIVATE:
			cmd = CMD_WIDGET_ACCESS_ACTIVATE;
			break;
		case WIDGET_ACCESS_EVENT_ACTION:
			cmd = CMD_WIDGET_ACCESS_ACTION;
			ret = (int)info->type;
			break;
		case WIDGET_ACCESS_EVENT_SCROLL:
			cmd = CMD_WIDGET_ACCESS_SCROLL;
			ret = (int)info->type;
			break;
		case WIDGET_ACCESS_EVENT_VALUE_CHANGE:
			cmd = CMD_WIDGET_ACCESS_VALUE_CHANGE;
			break;
		case WIDGET_ACCESS_EVENT_MOUSE:
			cmd = CMD_WIDGET_ACCESS_MOUSE;
			ret = (int)info->type;
			break;
		case WIDGET_ACCESS_EVENT_BACK:
			cmd = CMD_WIDGET_ACCESS_BACK;
			break;
		case WIDGET_ACCESS_EVENT_OVER:
			cmd = CMD_WIDGET_ACCESS_OVER;
			break;
		case WIDGET_ACCESS_EVENT_READ:
			cmd = CMD_WIDGET_ACCESS_READ;
			break;
		case WIDGET_ACCESS_EVENT_ENABLE:
			cmd = CMD_WIDGET_ACCESS_ENABLE;
			ret = info->type;
			break;
		default:
			return WIDGET_ERROR_INVALID_PARAMETER;
		}
	} else {
		ErrPrint("Invalid event type\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!cb) {
		cb = default_access_event_cb;
	}

	ret = send_access_event(handle, (const char *)&cmd, info->x * w, info->y * h, ret);
	if (ret == (int)WIDGET_ERROR_NONE) {
		handle->cbs.access_event.cb = cb;
		handle->cbs.access_event.data = data;
		handle->common->request.access_event = 1;
	}

	return ret;
}

EAPI int widget_viewer_feed_mouse_event(widget_h handle, widget_mouse_event_type_e type, widget_mouse_event_info_s info)
{
	unsigned int cmd;

	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!(type & WIDGET_MOUSE_EVENT_MASK)) {
		ErrPrint("Invalid content event is used\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (type & WIDGET_MOUSE_EVENT_GBAR_MASK) {
		int flag = 1;

		if (!handle->common->is_gbar_created) {
			ErrPrint("GBAR is not created\n");
			return WIDGET_ERROR_INVALID_PARAMETER;
		}

		if (!handle->common->gbar.fb) {
			ErrPrint("Handler is not valid\n");
			return WIDGET_ERROR_INVALID_PARAMETER;
		}

		if (type & WIDGET_MOUSE_EVENT_MOVE) {
			if (fabs(info->x - handle->common->gbar.x) < conf_event_filter() && fabs(info->y - handle->common->gbar.y) < conf_event_filter()) {
				return WIDGET_ERROR_RESOURCE_BUSY;
			}
		} else if (type & WIDGET_MOUSE_EVENT_SET) {
			flag = 0;
		}

		if (flag) {
			handle->common->gbar.x = info->x;
			handle->common->gbar.y = info->y;
		}

		switch ((type & ~(WIDGET_MOUSE_EVENT_GBAR_MASK | WIDGET_MOUSE_EVENT_WIDGET_MASK))) {
		case WIDGET_MOUSE_EVENT_ENTER | WIDGET_MOUSE_EVENT_MASK:
			cmd = CMD_GBAR_MOUSE_ENTER;
			break;
		case WIDGET_MOUSE_EVENT_LEAVE | WIDGET_MOUSE_EVENT_MASK:
			cmd = CMD_GBAR_MOUSE_LEAVE;
			break;
		case WIDGET_MOUSE_EVENT_UP | WIDGET_MOUSE_EVENT_MASK:
			cmd = CMD_GBAR_MOUSE_UP;
			break;
		case WIDGET_MOUSE_EVENT_DOWN | WIDGET_MOUSE_EVENT_MASK:
			cmd = CMD_GBAR_MOUSE_DOWN;
			break;
		case WIDGET_MOUSE_EVENT_MOVE | WIDGET_MOUSE_EVENT_MASK:
			cmd = CMD_GBAR_MOUSE_MOVE;
			break;
		case WIDGET_MOUSE_EVENT_SET | WIDGET_MOUSE_EVENT_MASK:
			cmd = CMD_GBAR_MOUSE_SET;
			break;
		case WIDGET_MOUSE_EVENT_UNSET | WIDGET_MOUSE_EVENT_MASK:
			cmd = CMD_GBAR_MOUSE_UNSET;
			break;
		case WIDGET_MOUSE_EVENT_ON_SCROLL | WIDGET_MOUSE_EVENT_MASK:
			cmd = CMD_GBAR_MOUSE_ON_SCROLL;
			break;
		case WIDGET_MOUSE_EVENT_ON_HOLD | WIDGET_MOUSE_EVENT_MASK:
			cmd = CMD_GBAR_MOUSE_ON_HOLD;
			DbgPrint("Send ON_HOLD\n");
			break;
		case WIDGET_MOUSE_EVENT_OFF_SCROLL | WIDGET_MOUSE_EVENT_MASK:
			cmd = CMD_GBAR_MOUSE_OFF_SCROLL;
			break;
		case WIDGET_MOUSE_EVENT_OFF_HOLD | WIDGET_MOUSE_EVENT_MASK:
			cmd = CMD_GBAR_MOUSE_OFF_HOLD;
			break;
		default:
			ErrPrint("Invalid event type\n");
			return WIDGET_ERROR_INVALID_PARAMETER;
		}

	} else if (type & WIDGET_MOUSE_EVENT_WIDGET_MASK) {
		int flag = 1;

		if (!handle->common->widget.fb) {
			ErrPrint("Handler is not valid\n");
			return WIDGET_ERROR_INVALID_PARAMETER;
		}

		if (handle->common->widget.auto_launch) {
			return WIDGET_ERROR_DISABLED;
		}

		if (type & WIDGET_MOUSE_EVENT_MOVE) {
			if (fabs(info->x - handle->common->widget.x) < conf_event_filter() && fabs(info->y - handle->common->widget.y) < conf_event_filter()) {
				return WIDGET_ERROR_RESOURCE_BUSY;
			}
		} else if (type & WIDGET_MOUSE_EVENT_SET) {
			flag = 0;
		}

		if (flag) {
			handle->common->widget.x = info->x;
			handle->common->widget.y = info->y;
		}

		switch ((type & ~(WIDGET_MOUSE_EVENT_GBAR_MASK | WIDGET_MOUSE_EVENT_WIDGET_MASK))) {
		case WIDGET_MOUSE_EVENT_ENTER | WIDGET_MOUSE_EVENT_MASK:
			cmd = CMD_WIDGET_MOUSE_ENTER;
			break;
		case WIDGET_MOUSE_EVENT_LEAVE | WIDGET_MOUSE_EVENT_MASK:
			cmd = CMD_WIDGET_MOUSE_LEAVE;
			break;
		case WIDGET_MOUSE_EVENT_UP | WIDGET_MOUSE_EVENT_MASK:
			cmd = CMD_WIDGET_MOUSE_UP;
			break;
		case WIDGET_MOUSE_EVENT_DOWN | WIDGET_MOUSE_EVENT_MASK:
			cmd = CMD_WIDGET_MOUSE_DOWN;
			break;
		case WIDGET_MOUSE_EVENT_MOVE | WIDGET_MOUSE_EVENT_MASK:
			if (!handle->common->widget.mouse_event) {
				return WIDGET_ERROR_INVALID_PARAMETER;
			}
			cmd = CMD_WIDGET_MOUSE_MOVE;
			break;
		case WIDGET_MOUSE_EVENT_SET | WIDGET_MOUSE_EVENT_MASK:
			if (!handle->common->widget.mouse_event) {
				return WIDGET_ERROR_INVALID_PARAMETER;
			}
			cmd = CMD_WIDGET_MOUSE_SET;
			break;
		case WIDGET_MOUSE_EVENT_UNSET | WIDGET_MOUSE_EVENT_MASK:
			if (!handle->common->widget.mouse_event) {
				return WIDGET_ERROR_INVALID_PARAMETER;
			}
			cmd = CMD_WIDGET_MOUSE_UNSET;
			break;
		case WIDGET_MOUSE_EVENT_ON_SCROLL | WIDGET_MOUSE_EVENT_MASK:
			cmd = CMD_WIDGET_MOUSE_ON_SCROLL;
			break;
		case WIDGET_MOUSE_EVENT_ON_HOLD | WIDGET_MOUSE_EVENT_MASK:
			cmd = CMD_WIDGET_MOUSE_ON_HOLD;
			DbgPrint("Send ON_HOLD\n");
			break;
		case WIDGET_MOUSE_EVENT_OFF_SCROLL | WIDGET_MOUSE_EVENT_MASK:
			cmd = CMD_WIDGET_MOUSE_OFF_SCROLL;
			break;
		case WIDGET_MOUSE_EVENT_OFF_HOLD | WIDGET_MOUSE_EVENT_MASK:
			cmd = CMD_WIDGET_MOUSE_OFF_HOLD;
			break;
		default:
			ErrPrint("Invalid event type\n");
			return WIDGET_ERROR_INVALID_PARAMETER;
		}
	} else {
		ErrPrint("Invalid event type\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return send_mouse_event(handle, (const char *)&cmd, info->x, info->y, info->ratio_w, info->ratio_h, info->device);
}

EAPI int widget_viewer_feed_key_event(widget_h handle, widget_key_event_type_e type, widget_key_event_info_s info, widget_ret_cb cb, void *data)
{
	int ret;
	unsigned int cmd;

	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!(type & WIDGET_KEY_EVENT_MASK)) {
		ErrPrint("Invalid key event is used\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (handle->common->request.key_event) {
		ErrPrint("Previous key event is not completed yet\n");
		return WIDGET_ERROR_RESOURCE_BUSY;
	}

	if (type & WIDGET_MOUSE_EVENT_GBAR_MASK) {
		if (!handle->common->is_gbar_created) {
			ErrPrint("GBAR is not created\n");
			return WIDGET_ERROR_INVALID_PARAMETER;
		}

		if (!handle->common->gbar.fb) {
			ErrPrint("Handler is not valid\n");
			return WIDGET_ERROR_INVALID_PARAMETER;
		}

		if (type & WIDGET_KEY_EVENT_DOWN) {
			/*!
			 * \TODO
			 * filtering the reproduced events if it is too fast
			 */
		} else if (type & WIDGET_KEY_EVENT_SET) {
			/*!
			 * \TODO
			 * What can I do for this case?
			 */
		}

		/*!
		 * Must be short than 29 bytes.
		 */
		switch ((type & ~(WIDGET_MOUSE_EVENT_GBAR_MASK | WIDGET_MOUSE_EVENT_WIDGET_MASK))) {
		case WIDGET_KEY_EVENT_FOCUS_IN | WIDGET_KEY_EVENT_MASK:
			cmd = CMD_GBAR_KEY_FOCUS_IN;
			break;
		case WIDGET_KEY_EVENT_FOCUS_OUT | WIDGET_KEY_EVENT_MASK:
			cmd = CMD_GBAR_KEY_FOCUS_OUT;
			break;
		case WIDGET_KEY_EVENT_UP | WIDGET_KEY_EVENT_MASK:
			cmd = CMD_GBAR_KEY_UP;
			break;
		case WIDGET_KEY_EVENT_DOWN | WIDGET_KEY_EVENT_MASK:
			cmd = CMD_GBAR_KEY_DOWN;
			break;
		case WIDGET_KEY_EVENT_SET | WIDGET_KEY_EVENT_MASK:
			cmd = CMD_GBAR_KEY_SET;
			break;
		case WIDGET_KEY_EVENT_UNSET | WIDGET_KEY_EVENT_MASK:
			cmd = CMD_GBAR_KEY_UNSET;
			break;
		default:
			ErrPrint("Invalid event type\n");
			return WIDGET_ERROR_INVALID_PARAMETER;
		}

	} else if (type & WIDGET_MOUSE_EVENT_WIDGET_MASK) {
		if (!handle->common->widget.fb) {
			ErrPrint("Handler is not valid\n");
			return WIDGET_ERROR_INVALID_PARAMETER;
		}

		if (type & WIDGET_KEY_EVENT_DOWN) {
			/*!
			 * \TODO
			 * filtering the reproduced events if it is too fast
			 */
		} else if (type & WIDGET_KEY_EVENT_SET) {
			/*!
			 * What can I do for this case?
			 */
		}

		switch ((type & ~(WIDGET_MOUSE_EVENT_GBAR_MASK | WIDGET_MOUSE_EVENT_WIDGET_MASK))) {
		case WIDGET_KEY_EVENT_FOCUS_IN | WIDGET_KEY_EVENT_MASK:
			cmd = CMD_WIDGET_KEY_FOCUS_IN;
			break;
		case WIDGET_KEY_EVENT_FOCUS_OUT | WIDGET_KEY_EVENT_MASK:
			cmd = CMD_WIDGET_KEY_FOCUS_OUT;
			break;
		case WIDGET_KEY_EVENT_UP | WIDGET_KEY_EVENT_MASK:
			cmd = CMD_WIDGET_KEY_UP;
			break;
		case WIDGET_KEY_EVENT_DOWN | WIDGET_KEY_EVENT_MASK:
			cmd = CMD_WIDGET_KEY_DOWN;
			break;
		case WIDGET_KEY_EVENT_SET | WIDGET_KEY_EVENT_MASK:
			cmd = CMD_WIDGET_KEY_SET;
			break;
		case WIDGET_KEY_EVENT_UNSET | WIDGET_KEY_EVENT_MASK:
			cmd = CMD_WIDGET_KEY_UNSET;
			break;
		default:
			ErrPrint("Invalid event type\n");
			return WIDGET_ERROR_INVALID_PARAMETER;
		}
	} else {
		ErrPrint("Invalid event type\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!cb) {
		cb = default_key_event_cb;
	}

	ret = send_key_event(handle, (const char *)&cmd, info->keycode, info->device);
	if (ret == (int)WIDGET_ERROR_NONE) {
		handle->cbs.key_event.cb = cb;
		handle->cbs.key_event.data = data;
		handle->common->request.key_event = 1;
	}

	return ret;
}

EAPI const char *widget_viewer_get_filename(widget_h handle)
{
	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return NULL;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return NULL;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid\n");
		return NULL;
	}

	if (handle->common->filename) {
		return handle->common->filename;
	}

	/* Oooops */
	set_last_result(WIDGET_ERROR_NONE);
	return util_uri_to_path(handle->common->id);
}

EAPI int widget_viewer_get_glance_bar_size(widget_h handle, int *w, int *h)
{
	int _w;
	int _h;

	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!w) {
		w = &_w;
	}
	if (!h) {
		h = &_h;
	}

	if (!handle->common->is_gbar_created) {
		*w = handle->common->gbar.default_width;
		*h = handle->common->gbar.default_height;
	} else {
		*w = handle->common->gbar.width;
		*h = handle->common->gbar.height;
	}

	return WIDGET_ERROR_NONE;
}

EAPI int widget_viewer_get_size_type(widget_h handle, widget_size_type_e *size_type)
{
	int w;
	int h;
	int ret = WIDGET_ERROR_NONE;
	widget_size_type_e result_size_type = WIDGET_SIZE_TYPE_UNKNOWN;

	if (!handle || handle->state != WIDGET_STATE_CREATE || size_type == NULL) {
		ErrPrint("Handler is invalid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto out;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto out;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto out;
	}

	w = handle->common->widget.width;
	h = handle->common->widget.height;

	switch (handle->common->widget.type) {
	case WIDGET_TYPE_BUFFER:
	case WIDGET_TYPE_SCRIPT:
		if (!fb_is_created(handle->common->widget.fb)) {
			DbgPrint("FB is not created yet, but give its size to the caller: %dx%d\n", w, h);
		}
		break;
	default:
		break;
	}

	if ((ret = widget_service_get_size_type(w, h, &result_size_type)) != WIDGET_ERROR_NONE) {
		ErrPrint("widget_service_get_size_type failed : %d\n", ret);
		goto out;
	}

out:

	if (size_type) {
		*size_type = result_size_type;
	}

	return ret;
}

EAPI int widget_viewer_set_group(widget_h handle, const char *cluster, const char *category, widget_ret_cb cb, void *data)
{
	struct packet *packet;
	unsigned int cmd = CMD_CHANGE_GROUP;
	int ret;

	if (!handle) {
		ErrPrint("Handler is NIL\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!cluster || !category || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (handle->common->request.group_changed) {
		ErrPrint("Previous group changing request is not finished yet\n");
		return WIDGET_ERROR_RESOURCE_BUSY;
	}

	if (!handle->common->is_user) {
		ErrPrint("CA Livebox is not able to change the group\n");
		return WIDGET_ERROR_PERMISSION_DENIED;
	}

	if (!strcmp(handle->common->cluster, cluster) && !strcmp(handle->common->category, category)) {
		DbgPrint("No changes\n");
		return WIDGET_ERROR_ALREADY_EXIST;
	}

	packet = packet_create((const char *)&cmd, "ssss", handle->common->pkgname, handle->common->id, cluster, category);
	if (!packet) {
		ErrPrint("Failed to build a param\n");
		return WIDGET_ERROR_FAULT;
	}

	if (!cb) {
		cb = default_group_changed_cb;
	}

	ret = master_rpc_async_request(handle, packet, 0, set_group_ret_cb, NULL);
	if (ret == (int)WIDGET_ERROR_NONE) {
		handle->cbs.group_changed.cb = cb;
		handle->cbs.group_changed.data = data;
		handle->common->request.group_changed = 1;
	}

	return ret;
}

EAPI int widget_viewer_get_group(widget_h handle, const char **cluster, const char **category)
{
	if (!handle) {
		ErrPrint("Handler is NIL\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!cluster || !category || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	*cluster = handle->common->cluster;
	*category = handle->common->category;
	return WIDGET_ERROR_NONE;
}

EAPI int widget_viewer_get_supported_sizes(widget_h handle, int *cnt, widget_size_type_e *size_list)
{
	register int i;
	register int j;

	if (!handle || !size_list) {
		ErrPrint("Invalid argument, handle(%p), size_list(%p)\n", handle, size_list);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!cnt || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	for (j = i = 0; i < WIDGET_COUNT_OF_SIZE_TYPE; i++) {
		if (handle->common->widget.size_list & (0x01 << i)) {
			if (j == *cnt) {
				break;
			}

			size_list[j++] = (widget_size_type_e)(0x01 << i);
		}
	}

	*cnt = j;
	return WIDGET_ERROR_NONE;
}

EAPI const char *widget_viewer_get_pkgname(widget_h handle)
{
	if (!handle) {
		ErrPrint("Handler is NIL\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	if (handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is not valid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is not valid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	set_last_result(WIDGET_ERROR_NONE);
	return handle->common->pkgname;
}

EAPI int widget_viewer_get_priority(widget_h handle, double *priority)
{
	int ret = WIDGET_ERROR_NONE;
	double result_priority = -1.0f;

	if (!handle || handle->state != WIDGET_STATE_CREATE || priority == NULL) {
		ErrPrint("Handler is invalid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto out;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto out;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid (%p)\n", handle);
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto out;
	}

	result_priority = handle->common->widget.priority;

out:
	if (priority)
		*priority = result_priority;

	return ret;
}

EAPI int widget_delete_cluster(const char *cluster, widget_ret_cb cb, void *data)
{
	struct packet *packet;
	struct cb_info *cbinfo;
	unsigned int cmd = CMD_DELETE_CLUSTER;
	int ret;

	packet = packet_create((const char *)&cmd, "s", cluster);
	if (!packet) {
		ErrPrint("Failed to build a param\n");
		return WIDGET_ERROR_FAULT;
	}

	cbinfo = _widget_create_cb_info(cb, data);
	if (!cbinfo) {
		packet_destroy(packet);
		return WIDGET_ERROR_FAULT;
	}

	ret = master_rpc_async_request(NULL, packet, 0, _delete_cluster_cb, cbinfo);
	if (ret < 0) {
		_widget_destroy_cb_info(cbinfo);
	}

	return ret;
}

EAPI int widget_delete_category(const char *cluster, const char *category, widget_ret_cb cb, void *data)
{
	struct packet *packet;
	struct cb_info *cbinfo;
	unsigned int cmd = CMD_DELETE_CATEGORY;
	int ret;

	packet = packet_create((const char *)&cmd, "ss", cluster, category);
	if (!packet) {
		ErrPrint("Failed to build a param\n");
		return WIDGET_ERROR_FAULT;
	}

	cbinfo = _widget_create_cb_info(cb, data);
	if (!cbinfo) {
		packet_destroy(packet);
		return WIDGET_ERROR_FAULT;
	}

	ret = master_rpc_async_request(NULL, packet, 0, _delete_category_cb, cbinfo);
	if (ret < 0) {
		_widget_destroy_cb_info(cbinfo);
	}

	return ret;
}

EAPI int widget_viewer_get_type(widget_h handle, int gbar, widget_type_e *widget_type)
{
	int ret = WIDGET_ERROR_INVALID_PARAMETER;
	widget_type_e result_widget_type = WIDGET_CONTENT_TYPE_INVALID;

	if (!handle || handle->state != WIDGET_STATE_CREATE || widget_type == NULL) {
		ErrPrint("Handler is invalid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		result_widget_type = WIDGET_CONTENT_TYPE_INVALID;
		goto out;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		result_widget_type =  WIDGET_CONTENT_TYPE_INVALID;
		goto out;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		result_widget_type =  WIDGET_CONTENT_TYPE_INVALID;
		goto out;
	}

	if (gbar) {
		switch (handle->common->gbar.type) {
		case GBAR_TYPE_TEXT:
			result_widget_type =  WIDGET_CONTENT_TYPE_TEXT;
			break;
		case GBAR_TYPE_BUFFER:
		case GBAR_TYPE_SCRIPT:
			{
				const char *id;
				id = fb_id(handle->common->gbar.fb);
				if (id && !strncasecmp(id, SCHEMA_PIXMAP, strlen(SCHEMA_PIXMAP))) {
					result_widget_type =  WIDGET_CONTENT_TYPE_RESOURCE_ID;
					break;
				}
			}
			result_widget_type = WIDGET_CONTENT_TYPE_BUFFER;
			break;
		case GBAR_TYPE_UIFW:
			result_widget_type = WIDGET_CONTENT_TYPE_UIFW;
			break;
		default:
			break;
		}
	} else {
		switch (handle->common->widget.type) {
		case WIDGET_TYPE_FILE:
			result_widget_type = WIDGET_CONTENT_TYPE_IMAGE;
			break;
		case WIDGET_TYPE_BUFFER:
		case WIDGET_TYPE_SCRIPT:
			{
				const char *id;
				id = fb_id(handle->common->widget.fb);
				if (id && !strncasecmp(id, SCHEMA_PIXMAP, strlen(SCHEMA_PIXMAP))) {
					result_widget_type =  WIDGET_CONTENT_TYPE_RESOURCE_ID;
					break;
				}
			}
			result_widget_type = WIDGET_CONTENT_TYPE_BUFFER;
			break;
		case WIDGET_TYPE_TEXT:
			result_widget_type = WIDGET_CONTENT_TYPE_TEXT;
			break;
		case WIDGET_TYPE_UIFW:
			result_widget_type =  WIDGET_CONTENT_TYPE_UIFW;
			break;
		default:
			break;
		}
	}

out:
	if (widget_type)
		*widget_type = result_widget_type;
	return ret;
}

EAPI int widget_viewer_set_text_handler(widget_h handle, int gbar, struct widget_script_operators *ops)
{
	if (!handle) {
		ErrPrint("Handler is NIL\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (gbar) {
		memcpy(&handle->cbs.gbar_ops, ops, sizeof(*ops));
	} else {
		memcpy(&handle->cbs.widget_ops, ops, sizeof(*ops));
	}

	return WIDGET_ERROR_NONE;
}

EAPI int widget_viewer_acquire_extra_resource_id(widget_h handle, int gbar, int idx, widget_ret_cb cb, void *data)
{
	if (idx < 0) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Invalid handle\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (gbar) {
		/**
		 * This can be called from extra_resource_created event.
		 * and it can be called before get the created event.
		 * then we didn't know this handle's buffer type yet
		 * so we cannot use its type to validate handle
		 *
		 * handle->common.gbar.type == unknown
		 */
		if (!handle->common->gbar.extra_buffer) {
			return WIDGET_ERROR_NOT_EXIST;
		}

		if (idx >= conf_extra_buffer_count()) {
			return WIDGET_ERROR_INVALID_PARAMETER;
		}

		return widget_acquire_gbar_extra_pixmap(handle, idx, cb, data);
	} else {
		/**
		 * This can be called from extra_resource_created event.
		 * and it can be called before get the created event.
		 * then we didn't know this handle's buffer type yet
		 * so we cannot use its type to validate handle
		 *
		 * handle->common.widget.type == unknown
		 */
		if (!handle->common->widget.extra_buffer) {
			ErrPrint("Extra buffer is not prepared\n");
			return WIDGET_ERROR_NOT_EXIST;
		}

		if (idx >= conf_extra_buffer_count()) {
			ErrPrint("Invalid parameter: %d / %d\n", idx, conf_extra_buffer_count());
			return WIDGET_ERROR_INVALID_PARAMETER;
		}

		return widget_acquire_widget_extra_pixmap(handle, idx, cb, data);
	}
}

EAPI int widget_viewer_acquire_resource_id(widget_h handle, int gbar, widget_ret_cb cb, void *data)
{
	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Invalid handle\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (gbar) {
		if (handle->common->gbar.type != GBAR_TYPE_SCRIPT && handle->common->gbar.type != GBAR_TYPE_BUFFER) {
			ErrPrint("Handler is not valid type\n");
			return WIDGET_ERROR_INVALID_PARAMETER;
		}

		return widget_acquire_gbar_pixmap(handle, cb, data);
	} else {
		if (handle->common->widget.type != WIDGET_TYPE_SCRIPT && handle->common->widget.type != WIDGET_TYPE_BUFFER) {
			ErrPrint("Handler is not valid type\n");
			return WIDGET_ERROR_INVALID_PARAMETER;
		}

		return _widget_acquire_widget_pixmap(handle, cb, data);
	}
}

/*!
 * \note
 * Do not check the state of handle and common-handle.
 * If this function is used in the deleted callback,
 * the handle and common-handle's state would be DELETE
 * if this function check the state of handles,
 * user cannot release the pixmap.
 */
EAPI int widget_viewer_release_resource_id(widget_h handle, int gbar, unsigned int resource_id)
{
	struct packet *packet;
	const char *pkgname;
	const char *id;
	unsigned int cmd;

	if (resource_id == 0 /* || handle->state != WIDGET_STATE_CREATE */) {
		ErrPrint("Pixmap is invalid [%d]\n", resource_id);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (gbar) {
		if (!handle) {
			/*!
			 * \note
			 * Even though the handle is NULL, we should send the release request to the master.
			 * Because the resource_id resource can be released after the handle is destroyed.
			 * Pixmap resource is used by client. and it cannot be guaranteed to release resource_id.
			 * In some cases, the resource_id can be released after the handle is deleted.
			 *
			 * Its implementation is up to the viewer app.
			 * But we cannot force it to use only with valid handle.
			 */
			DbgPrint("Using NULL handle\n");
			pkgname = NULL;
			id = NULL;
			/*!
			 * \note
			 * Master will try to find the buffer handle using given resource_id. if the pkgname and id is not valid.
			 */
		} else {
			if (!handle->common /* || handle-common->state != WIDGET_STATE_CREATE */) {
				ErrPrint("Handler is invalid\n");
				return WIDGET_ERROR_INVALID_PARAMETER;
			}

			if (!handle->common->id) {
				ErrPrint("Invalid handle\n");
				return WIDGET_ERROR_INVALID_PARAMETER;
			}

			/**
			 * This can be called from extra_resource_created event.
			 * and it can be called before get the created event.
			 * then we didn't know this handle's buffer type yet
			 * so we cannot use its type to validate handle
			 *
			 * handle->common.gbar.type == unknown
			 */

			pkgname = handle->common->pkgname;
			id = handle->common->id;
		}

		cmd = CMD_GBAR_RELEASE_PIXMAP;
	} else {
		if (!handle) {
			/*!
			 * \note
			 * Even though the handle is NULL, we should send the release request to the master.
			 * Because the resource_id resource can be released after the handle is destroyed.
			 * Pixmap resource is used by client. and it cannot be guaranteed to release resource_id.
			 * In some cases, the resource_id can be released after the handle is deleted.
			 *
			 * Its implementation is up to the viewer app.
			 * But we cannot force it to use only with valid handle.
			 */
			DbgPrint("Using NULL handle\n");
			pkgname = NULL;
			id = NULL;
			/*!
			 * \note
			 * Master will try to find the buffer handle using given resource_id. if the pkgname and id is not valid.
			 */
		} else {
			if (!handle->common /* || handle->common->state != WIDGET_STATE_CREATE */) {
				ErrPrint("Handler is invalid\n");
				return WIDGET_ERROR_INVALID_PARAMETER;
			}

			if (!handle->common->id) {
				ErrPrint("Invalid handle\n");
				return WIDGET_ERROR_INVALID_PARAMETER;
			}

			/**
			 * This can be called from extra_resource_created event.
			 * and it can be called before get the created event.
			 * then we didn't know this handle's buffer type yet
			 * so we cannot use its type to validate handle
			 *
			 * handle->common.widget.type == unknown
			 */

			pkgname = handle->common->pkgname;
			id = handle->common->id;
		}

		cmd = CMD_WIDGET_RELEASE_PIXMAP;
	}

	packet = packet_create_noack((const char *)&cmd, "ssi", pkgname, id, resource_id);
	if (!packet) {
		ErrPrint("Failed to build a param\n");
		return WIDGET_ERROR_FAULT;
	}

	return master_rpc_request_only(handle, packet);
}

EAPI unsigned int widget_extra_resource_id(const widget_h handle, int gbar, int idx)
{
	if (idx < 0) {
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return 0u;
	}

	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return 0u;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return 0u;
	}

	if (!handle->common->id) {
		ErrPrint("Invalid handle\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return 0u;
	}

	if (gbar) {
		/**
		 * This can be called from extra_resource_created event.
		 * and it can be called before get the created event.
		 * then we didn't know this handle's buffer type yet
		 * so we cannot use its type to validate handle
		 *
		 * handle->common.gbar.type == unknown
		 */

		if (!handle->common->gbar.extra_buffer || handle->common->gbar.last_extra_buffer_idx < 0) {
			set_last_result(WIDGET_ERROR_NOT_EXIST);
			return 0u;
		}

		return handle->common->gbar.extra_buffer[handle->common->gbar.last_extra_buffer_idx];
	} else {
		/**
		 * This can be called from extra_resource_created event.
		 * and it can be called before get the created event.
		 * then we didn't know this handle's buffer type yet
		 * so we cannot use its type to validate handle
		 *
		 * handle->common.widget.type == unknown
		 */

		if (!handle->common->widget.extra_buffer || handle->common->widget.last_extra_buffer_idx < 0) {
			set_last_result(WIDGET_ERROR_NOT_EXIST);
			return 0u;
		}

		return handle->common->widget.extra_buffer[handle->common->widget.last_extra_buffer_idx];
	}
}

EAPI int widget_viewer_get_resource_id(const widget_h handle, int gbar, unsigned int *resouce_id)
{
	const char *id;
	unsigned int pixmap = 0u;
	int ret = WIDGET_ERROR_NONE;

	if (!handle || handle->state != WIDGET_STATE_CREATE || resouce_id == NULL) {
		ErrPrint("Handler is invalid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto out;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto out;
	}

	if (!handle->common->id) {
		ErrPrint("Invalid handle\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto out;
	}

	if (gbar) {
		if (handle->common->gbar.type != GBAR_TYPE_SCRIPT && handle->common->gbar.type != GBAR_TYPE_BUFFER) {
			ErrPrint("Invalid handle\n");
			ret = WIDGET_ERROR_INVALID_PARAMETER;
			goto out;
		}

		id = fb_id(handle->common->gbar.fb);
		if (id && sscanf(id, SCHEMA_PIXMAP "%u", &pixmap) != 1) {
			ErrPrint("PIXMAP Id is not valid\n");
			ret = WIDGET_ERROR_INVALID_PARAMETER;
			goto out;
		}
	} else {
		if (handle->common->widget.type != WIDGET_TYPE_SCRIPT && handle->common->widget.type != WIDGET_TYPE_BUFFER) {
			ErrPrint("Invalid handle\n");
			ret = WIDGET_ERROR_INVALID_PARAMETER;
			goto out;
		}

		id = fb_id(handle->common->widget.fb);
		if (id && sscanf(id, SCHEMA_PIXMAP "%u", &pixmap) != 1) {
			ErrPrint("PIXMAP Id is not valid\n");
			ret = WIDGET_ERROR_INVALID_PARAMETER;
			goto out;
		}
	}
out:
	if (resouce_id)
		*resouce_id = pixmap;

	return ret;
}

EAPI void *widget_viewer_acquire_buffer(widget_h handle, int gbar)
{
	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	if (!handle->common->id) {
		ErrPrint("Invalid handle\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	if (gbar) {
		if (handle->common->gbar.type != GBAR_TYPE_SCRIPT && handle->common->gbar.type != GBAR_TYPE_BUFFER) {
			ErrPrint("Handler is not valid type\n");
			set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
			return NULL;
		}

		return fb_acquire_buffer(handle->common->gbar.fb);
	} else {
		if (handle->common->widget.type != WIDGET_TYPE_SCRIPT && handle->common->widget.type != WIDGET_TYPE_BUFFER) {
			ErrPrint("Handler is not valid type\n");
			set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
			return NULL;
		}

		return fb_acquire_buffer(handle->common->widget.fb);
	}
}

EAPI int widget_viewer_release_buffer(void *buffer)
{
	return fb_release_buffer(buffer);
}

EAPI int widget_viewer_get_buffer_reference_count(void *buffer)
{
	return fb_refcnt(buffer);
}

EAPI int widget_viewer_get_buffer_size(widget_h handle, int gbar)
{
	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Invalid handle\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (gbar) {
		return fb_size(handle->common->gbar.fb);
	} else {
		return fb_size(handle->common->widget.fb);
	}
}

EAPI int widget_viewer_is_created_by_user(widget_h handle)
{
	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Invalid handle\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return handle->common->is_user;
}

EAPI int widget_viewer_set_pinup(widget_h handle, int flag, widget_ret_cb cb, void *data)
{
	struct packet *packet;
	unsigned int cmd = CMD_PINUP_CHANGED;
	int ret;

	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Invalid handle\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (handle->common->request.pinup) {
		ErrPrint("Previous pinup request is not finished\n");
		return WIDGET_ERROR_RESOURCE_BUSY;
	}

	if (handle->common->is_pinned_up == flag) {
		DbgPrint("No changes\n");
		return WIDGET_ERROR_ALREADY_EXIST;
	}

	packet = packet_create((const char *)&cmd, "ssi", handle->common->pkgname, handle->common->id, flag);
	if (!packet) {
		ErrPrint("Failed to build a param\n");
		return WIDGET_ERROR_FAULT;
	}

	if (!cb) {
		cb = default_pinup_cb;
	}

	ret = master_rpc_async_request(handle, packet, 0, pinup_done_cb, NULL);
	if (ret == (int)WIDGET_ERROR_NONE) {
		handle->cbs.pinup.cb = cb;
		handle->cbs.pinup.data = data;
		handle->common->request.pinup = 1;
	}

	return ret;
}

EAPI int widget_viewer_is_pinned_up(widget_h handle)
{
	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Invalid handle\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return handle->common->is_pinned_up;
}

EAPI int widget_viewer_has_pinup(widget_h handle)
{
	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Invalid handle\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return handle->common->widget.pinup_supported;
}

EAPI int widget_viewer_set_data(widget_h handle, void *data)
{
	if (!handle) {
		ErrPrint("Handler is NIL\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	handle->data = data;
	return WIDGET_ERROR_NONE;
}

EAPI void *widget_viewer_get_data(widget_h handle)
{
	if (!handle) {
		ErrPrint("Handler is NIL\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	if (handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	return handle->data;
}

EAPI const char *widget_viewer_get_content_string(widget_h handle)
{
	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Invalid handle\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	set_last_result(WIDGET_ERROR_NONE);
	return handle->common->content;
}

EAPI const char *widget_viewer_get_title_string(widget_h handle)
{
	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Invalid handle\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	set_last_result(WIDGET_ERROR_NONE);
	return handle->common->title;
}

EAPI int widget_viewer_emit_text_signal(widget_h handle, widget_text_signal_s event_info, widget_ret_cb cb, void *data)
{
	struct packet *packet;
	struct cb_info *cbinfo;
	unsigned int cmd = CMD_TEXT_SIGNAL;
	int ret;
	const char *signal_name;
	const char *source;

	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (handle->common->widget.type != WIDGET_TYPE_TEXT && handle->common->gbar.type != GBAR_TYPE_TEXT) {
		DbgPrint("Not a text box, but send signal\n");
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!event_info) {
		ErrPrint("Invalid event info\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	signal_name = event_info->signal_name;
	if (!signal_name) {
		signal_name = "";
	}

	source = event_info->source;
	if (!source) {
		source = "";
	}

	packet = packet_create((const char *)&cmd, "ssssdddd",
			handle->common->pkgname, handle->common->id,
			signal_name, source,
			event_info->geometry.sx, event_info->geometry.sy,
			event_info->geometry.ex, event_info->geometry.ey);
	if (!packet) {
		ErrPrint("Failed to build a param\n");
		return WIDGET_ERROR_FAULT;
	}

	cbinfo = _widget_create_cb_info(cb, data);
	if (!cbinfo) {
		packet_destroy(packet);
		return WIDGET_ERROR_FAULT;
	}

	ret = master_rpc_async_request(handle, packet, 0, text_signal_cb, cbinfo);
	if (ret < 0) {
		_widget_destroy_cb_info(cbinfo);
	}

	return ret;
}

EAPI int widget_viewer_subscribe_group(const char *cluster, const char *category)
{
	struct packet *packet;
	unsigned int cmd = CMD_SUBSCRIBE;

	/*!
	 * \todo
	 * Validate the group info using DB
	 * If the group info is not valid, do not send this request
	 */

	packet = packet_create_noack((const char *)&cmd, "ss", cluster ? cluster : "", category ? category : "");
	if (!packet) {
		ErrPrint("Failed to create a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	return master_rpc_request_only(NULL, packet);
}

EAPI int widget_viewer_unsubscribe_group(const char *cluster, const char *category)
{
	struct packet *packet;
	unsigned int cmd = CMD_UNSUBSCRIBE;

	/*!
	 * \todo
	 * Validate the group info using DB
	 * If the group info is not valid, do not send this request
	 * AND Check the subscribed or not too
	 */

	packet = packet_create_noack((const char *)&cmd, "ss", cluster ? cluster : "", category ? category : "");
	if (!packet) {
		ErrPrint("Failed to create a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	return master_rpc_request_only(NULL, packet);
}

EAPI int widget_viewer_subscribe_category(const char *category)
{
	struct packet *packet;
	unsigned int cmd = CMD_SUBSCRIBE_CATEGORY;

	if (!category) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "s", category);
	if (!packet) {
		ErrPrint("Failed to create a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	return master_rpc_request_only(NULL, packet);
}

EAPI int widget_viewer_unsubscribe_category(const char *category)
{
	struct packet *packet;
	unsigned int cmd = CMD_UNSUBSCRIBE_CATEGORY;

	if (!category) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "s", category);
	if (!packet) {
		ErrPrint("Failed to create a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	return master_rpc_request_only(NULL, packet);
}

EAPI int widget_viewer_refresh(widget_h handle, int force)
{
	struct packet *packet;
	unsigned int cmd = CMD_UPDATE;

	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "ssi", handle->common->pkgname, handle->common->id, force);
	if (!packet) {
		ErrPrint("Failed to create a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	return master_rpc_request_only(handle, packet);
}

EAPI int widget_viewer_refresh_group(const char *cluster, const char *category, int force)
{
	struct packet *packet;
	unsigned int cmd = CMD_REFRESH_GROUP;

	if (!cluster || !category) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "ssi", cluster, category, force);
	if (!packet) {
		ErrPrint("Failed to create a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	return master_rpc_request_only(NULL, packet);
}

EAPI int widget_viewer_set_visibility(widget_h handle, widget_visible_state_e state)
{
	int old_state;
	int ret;

	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->is_user) {
		/* System cluster widget cannot be changed its visible states */
		if (state == WIDGET_HIDE_WITH_PAUSE) {
			ErrPrint("CA Livebox is not able to change the visibility\n");
			return WIDGET_ERROR_PERMISSION_DENIED;
		}
	}

	if (handle->visible == state) {
		DbgPrint("%s has no changes\n", handle->common->pkgname);
		return WIDGET_ERROR_ALREADY_EXIST;
	}

	old_state = handle->visible;
	handle->visible = state;

	ret = _widget_set_visibility(handle, state);
	if (ret < 0) {
		handle->visible = old_state;
	}

	return ret;
}

EAPI widget_visible_state_e widget_viewer_get_visibility(widget_h handle)
{
	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is invalid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return WIDGET_VISIBLE_ERROR;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is not valid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return WIDGET_VISIBLE_ERROR;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return WIDGET_VISIBLE_ERROR;
	}

	return handle->visible;
}

EAPI int widget_viewer_notify_paused_status_of_viewer(void)
{
	struct packet *packet;
	unsigned int cmd = CMD_CLIENT_PAUSED;

	packet = packet_create_noack((const char *)&cmd, "d", util_timestamp());
	if (!packet) {
		ErrPrint("Failed to create a pause packet\n");
		return WIDGET_ERROR_FAULT;
	}

	return master_rpc_request_only(NULL, packet);
}

EAPI int widget_viewer_notify_resumed_status_of_viewer(void)
{
	struct packet *packet;
	unsigned int cmd = CMD_CLIENT_RESUMED;

	packet = packet_create_noack((const char *)&cmd, "d", util_timestamp());
	if (!packet) {
		ErrPrint("Failed to create a resume packet\n");
		return WIDGET_ERROR_FAULT;
	}

	return master_rpc_request_only(NULL, packet);
}

EAPI int widget_viewer_notify_orientation_of_viewer(int orientation)
{
	struct packet *packet;
	unsigned int cmd = CMD_ORIENTATION;

	if (orientation < 0 || orientation > 360) {
		ErrPrint("Invalid parameter: %d\n", orientation);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "di", util_timestamp(), orientation);
	if (!packet) {
		ErrPrint("Failed to create a orientation packet\n");
		return WIDGET_ERROR_FAULT;
	}

	return master_rpc_request_only(NULL, packet);
}

EAPI int widget_viewer_sync_buffer(widget_h handle, int gbar)
{
	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Invalid handle\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Invalid handle\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Invalid handle\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (gbar) {
		return _widget_sync_gbar_fb(handle->common);
	} else {
		return _widget_sync_widget_fb(handle->common);
	}
}

EAPI const char *widget_viewer_get_alternative_icon(widget_h handle)
{
	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is not valid[%p]\n", handle);
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is not valid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	return handle->common->alt.icon;
}

EAPI const char *widget_viewer_get_alternative_name(widget_h handle)
{
	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is not valid[%p]\n", handle);
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is not valid\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	return handle->common->alt.name;
}

EAPI int widget_viewer_acquire_buffer_lock(widget_h handle, int is_gbar)
{
	int ret = WIDGET_ERROR_NONE;

	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is not valid[%p]\n", handle);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Handler is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid[%p]\n", handle);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (is_gbar) {
		ret = widget_service_acquire_lock(handle->common->gbar.lock);
	} else {
		ret = widget_service_acquire_lock(handle->common->widget.lock);
	}

	return ret == 0 ? WIDGET_ERROR_NONE : WIDGET_ERROR_FAULT;
}

EAPI int widget_viewer_release_buffer_lock(widget_h handle, int is_gbar)
{
	int ret = WIDGET_ERROR_NONE;

	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Invalid handle\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Invalid handle\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid[%p]\n", handle);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (is_gbar) {
		ret = widget_service_release_lock(handle->common->gbar.lock);
	} else {
		ret = widget_service_release_lock(handle->common->widget.lock);
	}

	return ret == 0 ? WIDGET_ERROR_NONE : WIDGET_ERROR_FAULT;
}

EAPI int widget_viewer_set_option(widget_option_type_e option, int state)
{
	int ret = WIDGET_ERROR_NONE;

	switch (option) {
	case WIDGET_OPTION_MANUAL_SYNC:
		conf_set_manual_sync(state);
		break;
	case WIDGET_OPTION_FRAME_DROP_FOR_RESIZE:
		conf_set_frame_drop_for_resizing(state);
		break;
	case WIDGET_OPTION_SHARED_CONTENT:
		conf_set_shared_content(state);
		break;
	case WIDGET_OPTION_DIRECT_UPDATE:
		if (s_info.init_count) {
			DbgPrint("Already intialized, this option is not applied\n");
		}
		conf_set_direct_update(state);
		break;
	case WIDGET_OPTION_EXTRA_BUFFER_CNT:
		ErrPrint("Permission denied\n");
		ret = WIDGET_ERROR_PERMISSION_DENIED;
		break;
	default:
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		break;
	}

	return ret;
}

EAPI int widget_viewer_get_option(widget_option_type_e option)
{
	int ret;

	set_last_result(WIDGET_ERROR_NONE);
	switch (option) {
	case WIDGET_OPTION_MANUAL_SYNC:
		ret = conf_manual_sync();
		break;
	case WIDGET_OPTION_FRAME_DROP_FOR_RESIZE:
		ret = conf_frame_drop_for_resizing();
		break;
	case WIDGET_OPTION_SHARED_CONTENT:
		ret = conf_shared_content();
		break;
	case WIDGET_OPTION_DIRECT_UPDATE:
		ret = conf_direct_update();
		break;
	case WIDGET_OPTION_EXTRA_BUFFER_CNT:
		ret = conf_extra_buffer_count();
		break;
	default:
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		break;
	}

	return ret;
}

EAPI int widget_viewer_set_auto_launch_handler(widget_auto_launch_handler_cb widget_launch_handler, void *data)
{
	s_info.launch.handle = widget_launch_handler;
	s_info.launch.data = data;

	return WIDGET_ERROR_NONE;
}

EAPI int widget_viewer_get_damaged_region(widget_h handle, int gbar, const widget_damage_region_s *region)
{
	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Invalid handle\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Invalid handle\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid[%p]\n", handle);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (gbar) {
		region = &handle->common->widget.last_damage;
	} else {
		region = &handle->common->gbar.last_damage;
	}

	return WIDGET_ERROR_NONE;
}

EAPI int widget_viewer_get_affected_extra_buffer(widget_h handle, int gbar, int *idx, unsigned int *resource_id)
{
	int _idx;
	unsigned int _resource_id;

	if (!handle || handle->state != WIDGET_STATE_CREATE) {
		ErrPrint("Invalid handle\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Invalid handle\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid[%p]\n", handle);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!idx) {
		idx = &_idx;
	}

	if (!resource_id) {
		resource_id = &_resource_id;
	}

	if (gbar) {
		if (!handle->common->gbar.extra_buffer || handle->common->gbar.last_extra_buffer_idx < 0) {
			return WIDGET_ERROR_NOT_EXIST;
		}

		*idx = handle->common->gbar.last_extra_buffer_idx;
		*resource_id = handle->common->gbar.extra_buffer[*idx];
	} else {
		if (!handle->common->widget.extra_buffer || handle->common->widget.last_extra_buffer_idx < 0) {
			return WIDGET_ERROR_NOT_EXIST;
		}

		*idx = handle->common->widget.last_extra_buffer_idx;
		*resource_id = handle->common->widget.extra_buffer[*idx];
	}

	return WIDGET_ERROR_NONE;
}

EAPI int widget_viewer_get_instance_id(widget_h handle, char **instance_id)
{
	if (!handle || handle->state != WIDGET_STATE_CREATE || !instance_id) {
		ErrPrint("Invalid handle\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common || handle->common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Invalid handle\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!handle->common->id) {
		ErrPrint("Handler is not valid[%p]\n", handle);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	*instance_id = strdup(handle->common->id);
	if (!*instance_id) {
		ErrPrint("Out of memory: %d\n", errno);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	return WIDGET_ERROR_NONE;
}

/* End of a file */
