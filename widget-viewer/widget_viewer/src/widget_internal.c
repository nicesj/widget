#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <dlog.h>

#include <widget_errno.h>
#include <widget_service.h>
#include <widget_service_internal.h>
#include <widget_buffer.h>

#include <packet.h>

#include "dlist.h"
#include "debug.h"
#include "widget_viewer.h"
#include "widget_viewer_internal.h"
#include "fb.h"
#include "conf.h"
#include "util.h"
#include "master_rpc.h"

int errno;

typedef enum event_state {
	INFO_STATE_CALLBACK_IN_IDLE = 0x00,
	INFO_STATE_CALLBACK_IN_PROCESSING = 0x01
} event_state_e;

struct event_info {
	int is_deleted;
	int (*handler)(widget_h handler, widget_event_type_e event, void *data);
	void *user_data;
};

struct fault_info {
	int is_deleted;
	int (*handler)(widget_fault_type_e event, const char *pkgname, const char *filename, const char *func, void *data);
	void *user_data;
};

static struct info {
	struct dlist *widget_common_list;
	struct dlist *widget_list;
	struct dlist *event_list;
	struct dlist *fault_list;
	event_state_e event_state;
	event_state_e fault_state;
} s_info = {
	.widget_common_list = NULL,
	.widget_list = NULL,
	.event_list = NULL,
	.fault_list = NULL,
	.event_state = INFO_STATE_CALLBACK_IN_IDLE,
	.fault_state = INFO_STATE_CALLBACK_IN_IDLE,
};

static inline void default_delete_cb(widget_h handler, int ret, void *data)
{
	DbgPrint("Default deleted event handler: %d\n", ret);
}

static void del_ret_cb(widget_h handler, const struct packet *result, void *data)
{
	struct cb_info *info = data;
	int ret;
	widget_ret_cb cb;
	void *cbdata;

	cb = info->cb;
	cbdata = info->data;
	_widget_destroy_cb_info(info);

	if (!result) {
		ErrPrint("Connection lost?\n");
		ret = WIDGET_ERROR_FAULT;
	} else if (packet_get(result, "i", &ret) != 1) {
		ErrPrint("Invalid argument\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (ret == 0) {
		handler->cbs.deleted.cb = cb;
		handler->cbs.deleted.data = cbdata;
	} else if (cb) {
		cb(handler, ret, cbdata);
	}

	/*!
	 * \note
	 * Do not call the deleted callback from here.
	 * master will send the "deleted" event.
	 * Then invoke this callback.
	 *
	 * if (handler->cbs.deleted.cb)
	 *     handler->cbs.deleted.cb(handler, ret, handler->cbs.deleted.data);
	 */
}

struct widget_common *_widget_create_common_handle(widget_h handle, const char *pkgname, const char *cluster, const char *category)
{
	struct widget_common *common;

	common = calloc(1, sizeof(*common));
	if (!common) {
		ErrPrint("Heap: %d\n", errno);
		set_last_result(WIDGET_ERROR_OUT_OF_MEMORY);
		return NULL;
	}

	common->pkgname = strdup(pkgname);
	if (!common->pkgname) {
		free(common);
		set_last_result(WIDGET_ERROR_OUT_OF_MEMORY);
		return NULL;
	}

	common->cluster = strdup(cluster);
	if (!common->cluster) {
		ErrPrint("Error: %d\n", errno);
		free(common->pkgname);
		free(common);
		set_last_result(WIDGET_ERROR_OUT_OF_MEMORY);
		return NULL;
	}

	common->category = strdup(category);
	if (!common->category) {
		ErrPrint("Error: %d\n", errno);
		free(common->cluster);
		free(common->pkgname);
		free(common);
		set_last_result(WIDGET_ERROR_OUT_OF_MEMORY);
		return NULL;
	}

	/* Data provider will set this */
	common->widget.type = WIDGET_TYPE_FILE;
	common->gbar.type = GBAR_TYPE_SCRIPT;

	/* Used for handling the mouse event on a box */
	common->widget.mouse_event = 0;

	/* Cluster infomration is not determined yet */
	common->nr_of_sizes = 0x01;

	common->timestamp = util_timestamp();
	common->is_user = 1;
	common->delete_type = WIDGET_DELETE_PERMANENTLY;

	common->gbar.lock = NULL;
	common->gbar.last_extra_buffer_idx = WIDGET_UNKNOWN_BUFFER;

	common->widget.lock = NULL;
	common->widget.last_extra_buffer_idx = WIDGET_UNKNOWN_BUFFER;

	common->state = WIDGET_STATE_CREATE;
	common->visible = WIDGET_HIDE_WITH_PAUSE;

	s_info.widget_common_list = dlist_append(s_info.widget_common_list, common);
	return common;
}

int _widget_destroy_common_handle(struct widget_common *common)
{
	dlist_remove_data(s_info.widget_common_list, common);

	common->state = WIDGET_STATE_DESTROYED;

	if (common->filename) {
		(void)util_unlink(common->filename);
	}

	free(common->cluster);
	free(common->category);
	free(common->id);
	free(common->pkgname);
	free(common->filename);
	free(common->widget.auto_launch);
	free(common->alt.icon);
	free(common->alt.name);

	if (common->widget.fb) {
		fb_destroy(common->widget.fb);
		common->widget.fb = NULL;
	}

	if (common->gbar.fb) {
		fb_destroy(common->gbar.fb);
		common->gbar.fb = NULL;
	}

	return 0;
}

int _widget_common_ref(struct widget_common *common, widget_h handle)
{
	common->widget_list = dlist_append(common->widget_list, handle);
	common->refcnt++;

	return common->refcnt;
}

int _widget_common_unref(struct widget_common *common, widget_h handle)
{
	int refcnt;
	dlist_remove_data(common->widget_list, handle);
	refcnt = --common->refcnt;

	return refcnt;
}

int _widget_set_group(struct widget_common *common, const char *cluster, const char *category)
{
	void *pc = NULL;
	void *ps = NULL;

	if (cluster) {
		pc = strdup(cluster);
		if (!pc) {
			ErrPrint("Heap: %d (cluster: %s)\n", errno, cluster);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}
	}

	if (category) {
		ps = strdup(category);
		if (!ps) {
			ErrPrint("Heap: %d (category: %s)\n", errno, category);
			free(pc);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}
	}

	if (common->cluster) {
		free(common->cluster);
	}

	if (common->category) {
		free(common->category);
	}

	common->cluster = pc;
	common->category = ps;

	return WIDGET_ERROR_NONE;
}

void _widget_set_size(struct widget_common *common, int w, int h)
{
	widget_size_type_e size_type;

	common->widget.width = w;
	common->widget.height = h;

	widget_service_get_size_type(w, h, &size_type);
	if (size_type != WIDGET_SIZE_TYPE_UNKNOWN) {
		widget_service_get_need_of_mouse_event(common->pkgname, size_type, (bool*)&common->widget.mouse_event);
	}
}

void _widget_set_update_mode(struct widget_common *common, int active_mode)
{
	common->is_active_update = active_mode;
}

void _widget_set_gbarsize(struct widget_common *common, int w, int h)
{
	common->gbar.width = w;
	common->gbar.height = h;
}

void _widget_set_default_gbarsize(struct widget_common *common, int w, int h)
{
	common->gbar.default_width = w;
	common->gbar.default_height = h;
}

void _widget_invoke_fault_handler(widget_fault_type_e event, const char *pkgname, const char *file, const char *func)
{
	struct dlist *l;
	struct dlist *n;
	struct fault_info *info;

	s_info.fault_state = INFO_STATE_CALLBACK_IN_PROCESSING;

	dlist_foreach_safe(s_info.fault_list, l, n, info) {
		if (!info->is_deleted && info->handler(event, pkgname, file, func, info->user_data) == EXIT_FAILURE) {
			info->is_deleted = 1;
		}

		if (info->is_deleted) {
			s_info.fault_list = dlist_remove(s_info.fault_list, l);
			free(info);
		}
	}

	s_info.fault_state &= ~INFO_STATE_CALLBACK_IN_PROCESSING;
}

void _widget_invoke_event_handler(widget_h handler, widget_event_type_e event)
{
	struct dlist *l;
	struct dlist *n;
	struct event_info *info;

	if (event == WIDGET_EVENT_WIDGET_UPDATED && handler->common->refcnt > 1) {
		if (handler->visible != WIDGET_SHOW) {
			DbgPrint("Update requested(pending) - %s\n", handler->common->pkgname);
			handler->paused_updating++;
			return;
		} else {
			handler->paused_updating = 0;
		}
	}

	s_info.event_state = INFO_STATE_CALLBACK_IN_PROCESSING;

	dlist_foreach_safe(s_info.event_list, l, n, info) {
		if (!info->is_deleted && info->handler(handler, event, info->user_data) == EXIT_FAILURE) {
			DbgPrint("Event handler returns EXIT_FAILURE\n");
			info->is_deleted = 1;
		}

		if (info->is_deleted) {
			s_info.event_list = dlist_remove(s_info.event_list, l);
			free(info);
		}
	}

	s_info.event_state &= ~INFO_STATE_CALLBACK_IN_PROCESSING;
}

struct widget_common *_widget_find_common_handle(const char *pkgname, const char *id)
{
	struct dlist *l;
	struct widget_common *common;

	dlist_foreach(s_info.widget_common_list, l, common) {
		if (!common->id) {
			continue;
		}

		if (!strcmp(common->pkgname, pkgname) && !strcmp(common->id, id)) {
			return common;
		}
	}

	return NULL;
}

struct widget_common *_widget_find_common_handle_by_timestamp(double timestamp)
{
	struct dlist *l;
	struct widget_common *common;

	dlist_foreach(s_info.widget_common_list, l, common) {
		if (common->timestamp == timestamp) {
			return common;
		}
	}

	return NULL;
}

widget_h _widget_new_widget(const char *pkgname, const char *id, double timestamp, const char *cluster, const char *category)
{
	widget_h handler;

	handler = calloc(1, sizeof(*handler));
	if (!handler) {
		ErrPrint("Failed to create a new widget\n");
		return NULL;
	}

	handler->common = _widget_create_common_handle(handler, pkgname, cluster, category);
	if (!handler->common) {
		ErrPrint("Heap: %d\n", errno);
		free(handler);
		return NULL;
	}

	_widget_common_ref(handler->common, handler);
	_widget_set_id(handler->common, id);
	handler->common->timestamp = timestamp;
	handler->common->state = WIDGET_STATE_CREATE;
	handler->visible = WIDGET_HIDE_WITH_PAUSE;
	handler->state = WIDGET_STATE_CREATE;
	handler = _widget_ref(handler);
	s_info.widget_list = dlist_append(s_info.widget_list, handler);

	return _widget_ref(handler);
}

int _widget_delete_all(void)
{
	struct dlist *l;
	struct dlist *n;
	widget_h handler;

	dlist_foreach_safe(s_info.widget_list, l, n, handler) {
		_widget_invoke_event_handler(handler, WIDGET_EVENT_DELETED);
		_widget_unref(handler, 1);
	}

	return WIDGET_ERROR_NONE;
}

int _widget_set_content(struct widget_common *common, const char *content)
{
	char *pc = NULL;

	if (content) {
		pc = strdup(content);
		if (!pc) {
			ErrPrint("heap: %d [%s]\n", errno, content);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}
	}

	free(common->content);
	common->content = pc;
	return WIDGET_ERROR_NONE;
}

int _widget_set_title(struct widget_common *common, const char *title)
{
	char *pt = NULL;

	if (title) {
		pt = strdup(title);
		if (!pt) {
			ErrPrint("heap: %d [%s]\n", errno, title);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}
	}

	free(common->title);
	common->title = pt;
	return WIDGET_ERROR_NONE;
}

void _widget_set_size_list(struct widget_common *common, int size_list)
{
	common->widget.size_list = size_list;
}

void _widget_set_auto_launch(struct widget_common *common, const char *auto_launch)
{
	char *pa = NULL;

	if (!auto_launch || !strlen(auto_launch)) {
		return;
	}

	pa = strdup(auto_launch);
	if (!pa) {
		ErrPrint("heap: %d, [%s]\n", errno, auto_launch);
		return;
	}

	free(common->widget.auto_launch);
	common->widget.auto_launch = pa;
}

void _widget_set_priority(struct widget_common *common, double priority)
{
	common->widget.priority = priority;
}

void _widget_set_id(struct widget_common *common, const char *id)
{
	char *pi = NULL;

	if (id) {
		pi = strdup(id);
		if (!pi) {
			ErrPrint("heap: %d [%s]\n", errno, pi);
			return;
		}
	}

	free(common->id);
	common->id = pi;
}

void _widget_unlink_filename(struct widget_common *common)
{
	if (common->widget.type == WIDGET_TYPE_FILE || common->widget.type == WIDGET_TYPE_TEXT) {
		if (common->filename && common->filename[0] && unlink(common->filename) < 0) {
			ErrPrint("unlink: %d (%s)\n", errno, common->filename);
		}
	}
}

void _widget_set_filename(struct widget_common *common, const char *filename)
{
	if (common->filename) {
		free(common->filename);
	}

	common->filename = strdup(filename);
	if (!common->filename) {
		ErrPrint("Heap: %d\n", errno);
	}
}

void _widget_set_alt_icon(struct widget_common *common, const char *icon)
{
	char *_icon = NULL;

	if (icon && strlen(icon)) {
		_icon = strdup(icon);
		if (!_icon) {
			ErrPrint("Heap: %d\n", errno);
		}
	}

	free(common->alt.icon);
	common->alt.icon = _icon;
}

void _widget_set_alt_name(struct widget_common *common, const char *name)
{
	char *_name = NULL;

	if (name && strlen(name)) {
		_name = strdup(name);
		if (!_name) {
			ErrPrint("Heap: %d\n", errno);
		}
	}

	free(common->alt.name);
	common->alt.name = _name;
}

int _widget_set_widget_fb(struct widget_common *common, const char *filename)
{
	struct fb_info *fb;

	if (!common) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	fb = common->widget.fb;
	if (fb && !strcmp(fb_id(fb), filename)) { /*!< BUFFER is not changed, */
		return WIDGET_ERROR_NONE;
	}

	common->widget.fb = NULL;

	if (!filename || filename[0] == '\0') {
		if (fb) {
			fb_destroy(fb);
		}
		return WIDGET_ERROR_NONE;
	}

	common->widget.fb = fb_create(filename, common->widget.width, common->widget.height);
	if (!common->widget.fb) {
		ErrPrint("Faield to create a FB\n");
		if (fb) {
			fb_destroy(fb);
		}
		return WIDGET_ERROR_FAULT;
	}

	if (fb) {
		fb_destroy(fb);
	}

	return WIDGET_ERROR_NONE;
}

int _widget_set_gbar_fb(struct widget_common *common, const char *filename)
{
	struct fb_info *fb;

	if (!common || common->state != WIDGET_STATE_CREATE) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	fb = common->gbar.fb;
	if (fb && !strcmp(fb_id(fb), filename)) {
		/* BUFFER is not changed, just update the content */
		return WIDGET_ERROR_ALREADY_EXIST;
	}
	common->gbar.fb = NULL;

	if (!filename || filename[0] == '\0') {
		if (fb) {
			fb_destroy(fb);
		}
		return WIDGET_ERROR_NONE;
	}

	common->gbar.fb = fb_create(filename, common->gbar.width, common->gbar.height);
	if (!common->gbar.fb) {
		ErrPrint("Failed to create a FB\n");
		if (fb) {
			fb_destroy(fb);
		}
		return WIDGET_ERROR_FAULT;
	}

	if (fb) {
		fb_destroy(fb);
	}
	return WIDGET_ERROR_NONE;
}

struct fb_info *_widget_get_widget_fb(struct widget_common *common)
{
	return common->widget.fb;
}

struct fb_info *_widget_get_gbar_fb(struct widget_common *common)
{
	return common->gbar.fb;
}

void _widget_set_user(struct widget_common *common, int user)
{
	common->is_user = user;
}

void _widget_set_pinup(struct widget_common *common, int pinup_supported)
{
	common->widget.pinup_supported = pinup_supported;
}

void _widget_set_text_widget(struct widget_common *common)
{
	common->widget.type = WIDGET_TYPE_TEXT;
}

void _widget_set_text_gbar(struct widget_common *common)
{
	common->gbar.type = GBAR_TYPE_TEXT;
}

int _widget_text_widget(struct widget_common *common)
{
	return common->widget.type == WIDGET_TYPE_TEXT;
}

int _widget_text_gbar(struct widget_common *common)
{
	return common->gbar.type == GBAR_TYPE_TEXT;
}

void _widget_set_period(struct widget_common *common, double period)
{
	common->widget.period = period;
}

widget_h _widget_ref(widget_h handler)
{
	if (!handler) {
		return NULL;
	}

	handler->refcnt++;
	return handler;
}

widget_h _widget_unref(widget_h handler, int destroy_common)
{
	if (!handler) {
		return NULL;
	}

	handler->refcnt--;
	if (handler->refcnt > 0) {
		return handler;
	}

	if (handler->cbs.created.cb) {
		handler->cbs.created.cb(handler, WIDGET_ERROR_FAULT, handler->cbs.created.data);
		handler->cbs.created.cb = NULL;
		handler->cbs.created.data = NULL;
	}

	if (handler->cbs.deleted.cb) {
		handler->cbs.deleted.cb(handler, WIDGET_ERROR_FAULT, handler->cbs.deleted.data);
		handler->cbs.deleted.cb = NULL;
		handler->cbs.deleted.data = NULL;
	}

	if (handler->cbs.pinup.cb) {
		handler->cbs.pinup.cb(handler, WIDGET_ERROR_FAULT, handler->cbs.pinup.data);
		handler->cbs.pinup.cb = NULL;
		handler->cbs.pinup.data = NULL;
	}

	if (handler->cbs.group_changed.cb) {
		handler->cbs.group_changed.cb(handler, WIDGET_ERROR_FAULT, handler->cbs.group_changed.data);
		handler->cbs.group_changed.cb = NULL;
		handler->cbs.group_changed.data = NULL;
	}

	if (handler->cbs.period_changed.cb) {
		handler->cbs.period_changed.cb(handler, WIDGET_ERROR_FAULT, handler->cbs.period_changed.data);
		handler->cbs.period_changed.cb = NULL;
		handler->cbs.period_changed.data = NULL;
	}

	if (handler->cbs.size_changed.cb) {
		handler->cbs.size_changed.cb(handler, WIDGET_ERROR_FAULT, handler->cbs.size_changed.data);
		handler->cbs.size_changed.cb = NULL;
		handler->cbs.size_changed.data = NULL;
	}

	if (handler->cbs.gbar_created.cb) {
		handler->cbs.gbar_created.cb(handler, WIDGET_ERROR_FAULT, handler->cbs.gbar_created.data);
		handler->cbs.gbar_created.cb = NULL;
		handler->cbs.gbar_created.data = NULL;
	}

	if (handler->cbs.gbar_destroyed.cb) {
		handler->cbs.gbar_destroyed.cb(handler, WIDGET_ERROR_FAULT, handler->cbs.gbar_destroyed.data);
		handler->cbs.gbar_destroyed.cb = NULL;
		handler->cbs.gbar_destroyed.data = NULL;
	}

	if (handler->cbs.update_mode.cb) {
		handler->cbs.update_mode.cb(handler, WIDGET_ERROR_FAULT, handler->cbs.update_mode.data);
		handler->cbs.update_mode.cb = NULL;
		handler->cbs.update_mode.data = NULL;
	}

	if (handler->cbs.access_event.cb) {
		handler->cbs.access_event.cb(handler, WIDGET_ACCESS_STATUS_ERROR, handler->cbs.access_event.data);
		handler->cbs.access_event.cb = NULL;
		handler->cbs.access_event.data = NULL;
	}

	if (handler->cbs.key_event.cb) {
		handler->cbs.key_event.cb(handler, WIDGET_KEY_STATUS_ERROR, handler->cbs.key_event.data);
		handler->cbs.key_event.cb = NULL;
		handler->cbs.key_event.data = NULL;
	}

	dlist_remove_data(s_info.widget_list, handler);

	handler->state = WIDGET_STATE_DESTROYED;
	if (_widget_common_unref(handler->common, handler) == 0) {
		if (destroy_common) {
			/*!
			 * \note
			 * Lock file should be deleted after all callbacks are processed.
			 */
			(void)widget_service_destroy_lock(handler->common->widget.lock, 0);
			handler->common->widget.lock = NULL;
			_widget_destroy_common_handle(handler->common);
		}
	}
	free(handler);
	DbgPrint("Handler is released\n");
	return NULL;
}

int _widget_send_delete(widget_h handler, int type, widget_ret_cb cb, void *data)
{
	struct packet *packet;
	struct cb_info *cbinfo;
	int ret;

	if (handler->common->request.deleted) {
		ErrPrint("Already in-progress\n");
		if (cb) {
			cb(handler, WIDGET_ERROR_NONE, data);
		}
		return WIDGET_ERROR_RESOURCE_BUSY;
	}

	if (!cb) {
		cb = default_delete_cb;
	}

	packet = packet_create("delete", "ssid", handler->common->pkgname, handler->common->id, type, handler->common->timestamp);
	if (!packet) {
		ErrPrint("Failed to build a param\n");
		if (cb) {
			cb(handler, WIDGET_ERROR_FAULT, data);
		}

		return WIDGET_ERROR_FAULT;
	}

	cbinfo = _widget_create_cb_info(cb, data);
	if (!cbinfo) {
		packet_destroy(packet);
		ErrPrint("Failed to create cbinfo\n");
		if (cb) {
			cb(handler, WIDGET_ERROR_FAULT, data);
		}

		return WIDGET_ERROR_FAULT;
	}

	ret = master_rpc_async_request(handler, packet, 0, del_ret_cb, cbinfo);
	if (ret < 0) {
		/*!
		 * Packet is destroyed by master_rpc_async_request.
		 */
		_widget_destroy_cb_info(cbinfo);

		if (cb) {
			cb(handler, WIDGET_ERROR_FAULT, data);
		}
	} else {
		handler->common->request.deleted = 1;
	}

	return ret;
}

int _widget_sync_widget_fb(struct widget_common *common)
{
	int ret;

	if (fb_type(_widget_get_widget_fb(common)) == WIDGET_FB_TYPE_FILE) {
		(void)widget_service_acquire_lock(common->widget.lock);
		ret = fb_sync(_widget_get_widget_fb(common), common->widget.last_damage.x, common->widget.last_damage.y, common->widget.last_damage.w, common->widget.last_damage.h);
		(void)widget_service_release_lock(common->widget.lock);
	} else {
		ret = fb_sync(_widget_get_widget_fb(common), common->widget.last_damage.x, common->widget.last_damage.y, common->widget.last_damage.w, common->widget.last_damage.h);
	}

	return ret;
}

int _widget_sync_gbar_fb(struct widget_common *common)
{
	int ret;

	if (fb_type(_widget_get_gbar_fb(common)) == WIDGET_FB_TYPE_FILE) {
		(void)widget_service_acquire_lock(common->gbar.lock);
		ret = fb_sync(_widget_get_gbar_fb(common), common->gbar.last_damage.x, common->gbar.last_damage.y, common->gbar.last_damage.w, common->gbar.last_damage.h);
		(void)widget_service_release_lock(common->gbar.lock);
	} else {
		ret = fb_sync(_widget_get_gbar_fb(common), common->gbar.last_damage.x, common->gbar.last_damage.y, common->gbar.last_damage.w, common->gbar.last_damage.h);
	}

	return ret;
}

struct widget_common *_widget_find_sharable_common_handle(const char *pkgname, const char *content, int w, int h, const char *cluster, const char *category)
{
	struct dlist *l;
	struct widget_common *common;

	if (!conf_shared_content()) {
		/*!
		 * Shared content option is turnned off.
		 */
		return NULL;
	}

	dlist_foreach(s_info.widget_common_list, l, common) {
		if (common->state != WIDGET_STATE_CREATE) {
			continue;
		}

		if (strcmp(common->pkgname, pkgname)) {
			continue;
		}

		if (strcmp(common->cluster, cluster)) {
			DbgPrint("Cluster mismatched\n");
			continue;
		}

		if (strcmp(common->category, category)) {
			DbgPrint("Category mismatched\n");
			continue;
		}

		if (common->content && content) {
			if (strcmp(common->content, content)) {
				DbgPrint("%s Content ([%s] <> [%s])\n", common->pkgname, common->content, content);
				continue;
			}
		} else {
			int c1_len;
			int c2_len;

			/*!
			 * \note
			 * We assumes "" (ZERO length string) to NULL
			 */
			c1_len = common->content ? strlen(common->content) : 0;
			c2_len = content ? strlen(content) : 0;
			if (c1_len != c2_len) {
				DbgPrint("%s Content %p <> %p\n", common->pkgname, common->content, content);
				continue;
			}
		}

		if (common->request.size_changed) {
			DbgPrint("Changing size\n");
			/*!
			 * \note
			 * Do not re-use resizing instance.
			 * We will not use predicted size.
			 */
			continue;
		}

		if (common->request.created) {
			DbgPrint("Creating now but re-use it (%s)\n", common->pkgname);
		}

		if (common->widget.width != w || common->widget.height != h) {
			DbgPrint("Size mismatched\n");
			continue;
		}

		DbgPrint("common handle is found: %p\n", common);
		return common;
	}

	return NULL;
}

widget_h _widget_find_widget_in_show(struct widget_common *common)
{
	struct dlist *l;
	widget_h item;

	dlist_foreach(common->widget_list, l, item) {
		if (item->visible == WIDGET_SHOW) {
			DbgPrint("%s visibility is not changed\n", common->pkgname);
			return item;
		}
	}

	return NULL;
}

widget_h _widget_get_widget_nth(struct widget_common *common, int nth)
{
	widget_h item;
	struct dlist *l;

	l = dlist_nth(common->widget_list, nth);
	item = dlist_data(l);

	return item;
}

int _widget_add_event_handler(widget_event_handler_cb widget_cb, void *data)
{
	struct event_info *info;
	info = malloc(sizeof(*info));
	if (!info) {
		ErrPrint("Heap: %d\n", errno);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	info->handler = widget_cb;
	info->user_data = data;
	info->is_deleted = 0;

	s_info.event_list = dlist_append(s_info.event_list, info);
	return WIDGET_ERROR_NONE;
}

void *_widget_remove_event_handler(widget_event_handler_cb widget_cb)
{
	struct event_info *info;
	struct dlist *l;

	dlist_foreach(s_info.event_list, l, info) {
		if (info->handler == widget_cb) {
			void *data;

			data = info->user_data;

			if (s_info.event_state == INFO_STATE_CALLBACK_IN_PROCESSING) {
				info->is_deleted = 1;
			} else {
				s_info.event_list = dlist_remove(s_info.event_list, l);
				free(info);
			}

			return data;
		}
	}

	return NULL;
}

int _widget_add_fault_handler(widget_fault_handler_cb widget_cb, void *data)
{
	struct fault_info *info;
	info = malloc(sizeof(*info));
	if (!info) {
		ErrPrint("Heap: %d\n", errno);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	info->handler = widget_cb;
	info->user_data = data;
	info->is_deleted = 0;

	s_info.fault_list = dlist_append(s_info.fault_list, info);
	return WIDGET_ERROR_NONE;
}

void *_widget_remove_fault_handler(widget_fault_handler_cb widget_cb)
{
	struct fault_info *info;
	struct dlist *l;

	dlist_foreach(s_info.fault_list, l, info) {
		if (info->handler == widget_cb) {
			void *data;

			data = info->user_data;

			if (s_info.fault_state == INFO_STATE_CALLBACK_IN_PROCESSING) {
				info->is_deleted = 1;
			} else {
				s_info.fault_list = dlist_remove(s_info.fault_list, l);
				free(info);
			}

			return data;
		}
	}

	return NULL;
}

struct cb_info *_widget_create_cb_info(widget_ret_cb cb, void *data)
{
	struct cb_info *info;

	info = malloc(sizeof(*info));
	if (!info) {
		ErrPrint("Heap: %d\n", errno);
		return NULL;
	}

	info->cb = cb;
	info->data = data;
	return info;
}

void _widget_destroy_cb_info(struct cb_info *info)
{
	free(info);
}

/* End of a file */
