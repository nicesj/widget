/*
 * Copyright 2013  Samsung Electronics Co., Ltd
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
#include <unistd.h>
#include <errno.h>
#include <malloc.h>

#include <Elementary.h>
#include <Eina.h>
#include <Ecore.h>
#include <Ecore_X.h>

#include <app.h>
#include <dlog.h>
#include <aul.h>
#include <sqlite3.h>

#include <widget_provider.h> /* widget_provider */
#include <widget_errno.h> /* widget_service */
#include <widget_script.h> /* widget_service - widget_event_info */
#include <widget_conf.h>
#include <widget/widget_internal.h> /* widget - WIDGET_SYS_EVENT_XXX */
#include <widget/widget.h> /* widget - WIDGET_SYS_EVENT_XXX */

#include "critical_log.h"
#include "debug.h"
#include "client.h"
#include "so_handler.h"
#include "widget.h"
#include "util.h"
#include "conf.h"
#include "main.h"

struct pre_callback_item {
	widget_pre_callback_t cb;
	void *data;
};

static struct info {
	Ecore_Timer *ping_timer;
	Eina_List *widget_pre_callback_list[WIDGET_PRE_CALLBACK_COUNT];
} s_info = {
	.ping_timer = NULL,
	.widget_pre_callback_list = { NULL, },
};

static void invoke_pre_callback(widget_pre_callback_e type, const char *id)
{
	Eina_List *l;
	Eina_List *n;
	struct pre_callback_item *item;

	EINA_LIST_FOREACH_SAFE(s_info.widget_pre_callback_list[type], l, n, item) {
		item->cb(id, item->data);
	}
}

int widget_provider_app_add_pre_callback(widget_pre_callback_e type, widget_pre_callback_t cb, void *data)
{
	struct pre_callback_item *item;
	Eina_List *l;

	if (!cb || type == WIDGET_PRE_CALLBACK_COUNT) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	EINA_LIST_FOREACH(s_info.widget_pre_callback_list[type], l, item) {
		if (item->cb == cb && item->data == data) {
			return WIDGET_ERROR_ALREADY_EXIST;
		}
	}

	item = malloc(sizeof(*item));
	if (!item) {
		ErrPrint("malloc: %d\n", errno);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	item->cb = cb;
	item->data = data;

	s_info.widget_pre_callback_list[type] = eina_list_append(s_info.widget_pre_callback_list[type], item);
	return 0;
}

int widget_provider_app_del_pre_callback(widget_pre_callback_e type, widget_pre_callback_t cb, void *data)
{
	Eina_List *l;
	Eina_List *n;
	struct pre_callback_item *item;

	if (!cb || type == WIDGET_PRE_CALLBACK_COUNT) {
		ErrPrint("Invalid parameter\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	EINA_LIST_FOREACH_SAFE(s_info.widget_pre_callback_list[type], l, n, item) {
		if (item->cb == cb && item->data == data) {
			s_info.widget_pre_callback_list[type] = eina_list_remove_list(s_info.widget_pre_callback_list[type], l);
			free(item);
			return WIDGET_ERROR_NONE;
		}
	}

	return WIDGET_ERROR_NOT_EXIST;
}

static int method_new(struct widget_event_arg *arg, int *width, int *height, double *priority, void *data)
{
	int ret;
	struct widget_create_arg _arg;
	DbgPrint("Create: pkgname[%s], id[%s], content[%s], timeout[%d], has_script[%d], period[%lf], cluster[%s], category[%s], skip[%d], abi[%s], size: %dx%d\n",
			arg->pkgname,
			arg->id,
			arg->info.widget_create.content,
			arg->info.widget_create.timeout,
			arg->info.widget_create.has_script,
			arg->info.widget_create.period,
			arg->info.widget_create.cluster, arg->info.widget_create.category,
			arg->info.widget_create.skip_need_to_create,
			arg->info.widget_create.abi,
			arg->info.widget_create.width,
			arg->info.widget_create.height);

	if (!arg->info.widget_create.content || !strlen(arg->info.widget_create.content)) {
		DbgPrint("Use default content: \"%s\"\n", WIDGET_CONF_DEFAULT_CONTENT);
		arg->info.widget_create.content = WIDGET_CONF_DEFAULT_CONTENT;
	}

	_arg.content = arg->info.widget_create.content;
	_arg.timeout = arg->info.widget_create.timeout;
	_arg.has_widget_script = arg->info.widget_create.has_script;
	_arg.period = arg->info.widget_create.period;
	_arg.cluster = arg->info.widget_create.cluster;
	_arg.category = arg->info.widget_create.category;
	_arg.abi = arg->info.widget_create.abi;
	_arg.skip_need_to_create = arg->info.widget_create.skip_need_to_create;
	_arg.direct_addr = arg->info.widget_create.direct_addr;

	invoke_pre_callback(WIDGET_PRE_CREATE_CALLBACK, arg->id);

	ret = widget_create(arg->pkgname, arg->id,
			&_arg,
			width, height, priority,
			&arg->info.widget_create.out_content,
			&arg->info.widget_create.out_title);

	if (ret == 0) {
		if (widget_set_orientation(arg->pkgname, arg->id, arg->info.widget_create.degree) < 0) {
			ErrPrint("Failed to set orientation %s - %d\n", arg->id, arg->info.widget_create.degree);
		}
		if (arg->info.widget_create.width > 0 && arg->info.widget_create.height > 0) {
			DbgPrint("Create size: %dx%d (created: %dx%d)\n", arg->info.widget_create.width, arg->info.widget_create.height, *width, *height);
			if (*width != arg->info.widget_create.width || *height != arg->info.widget_create.height) {
				int tmp;
				tmp = widget_viewer_resize_widget(arg->pkgname, arg->id, arg->info.widget_create.width, arg->info.widget_create.height);
				DbgPrint("widget_resize returns: %d\n", tmp);
				if (tmp == (int)WIDGET_ERROR_NONE) {
					/*!
					 * \note
					 * Just returns resized canvas size.
					 * Even if it is not ready to render contents.
					 * Provider will allocate render buffer using this size.
					 */
					*width = arg->info.widget_create.width;
					*height = arg->info.widget_create.height;
				}
			}
		}

		arg->info.widget_create.out_is_pinned_up = (widget_viewer_is_pinned_up(arg->pkgname, arg->id) == 1);
	} else {
		ErrPrint("widget_create returns %d\n", ret);
	}

	if (widget_is_all_paused()) {
		DbgPrint("Box is paused\n");
		(void)widget_system_event(arg->pkgname, arg->id, WIDGET_SYS_EVENT_PAUSED);
	}

	return ret;
}

static int method_renew(struct widget_event_arg *arg, void *data)
{
	int ret;
	int w;
	int h;
	double priority;
	struct widget_create_arg _arg;

	DbgPrint("Re-create: pkgname[%s], id[%s], content[%s], timeout[%d], has_script[%d], period[%lf], cluster[%s], category[%s], abi[%s]\n",
			arg->pkgname, arg->id,
			arg->info.widget_recreate.content,
			arg->info.widget_recreate.timeout,
			arg->info.widget_recreate.has_script,
			arg->info.widget_recreate.period,
			arg->info.widget_recreate.cluster,
			arg->info.widget_recreate.category,
			arg->info.widget_recreate.abi);

	if (!arg->info.widget_recreate.content || !strlen(arg->info.widget_recreate.content)) {
		DbgPrint("Use default content: \"%s\"\n", WIDGET_CONF_DEFAULT_CONTENT);
		arg->info.widget_recreate.content = WIDGET_CONF_DEFAULT_CONTENT;
	}

	_arg.content = arg->info.widget_recreate.content;
	_arg.timeout = arg->info.widget_recreate.timeout;
	_arg.has_widget_script = arg->info.widget_recreate.has_script;
	_arg.period = arg->info.widget_recreate.period;
	_arg.cluster = arg->info.widget_recreate.cluster;
	_arg.category = arg->info.widget_recreate.category;
	_arg.abi = arg->info.widget_recreate.abi;
	_arg.skip_need_to_create = 1;
	_arg.direct_addr = arg->info.widget_recreate.direct_addr;

	invoke_pre_callback(WIDGET_PRE_CREATE_CALLBACK, arg->id);

	ret = widget_create(arg->pkgname, arg->id,
			&_arg,
			&w, &h, &priority,
			&arg->info.widget_recreate.out_content,
			&arg->info.widget_recreate.out_title);
	if (ret == 0) {
		if (widget_set_orientation(arg->pkgname, arg->id, arg->info.widget_recreate.degree) < 0) {
			ErrPrint("Failed to set orientation %s - %d\n", arg->id, arg->info.widget_recreate.degree);
		}

		if (w != arg->info.widget_recreate.width || h != arg->info.widget_recreate.height) {
			int tmp;
			tmp = widget_viewer_resize_widget(arg->pkgname, arg->id, arg->info.widget_recreate.width, arg->info.widget_recreate.height);
			if (tmp < 0) {
				DbgPrint("Resize[%dx%d] returns: %d\n", arg->info.widget_recreate.width, arg->info.widget_recreate.height, tmp);
			}
		} else {
			DbgPrint("No need to change the size: %dx%d\n", w, h);
		}

		arg->info.widget_recreate.out_is_pinned_up = (widget_viewer_is_pinned_up(arg->pkgname, arg->id) == 1);
	} else {
		ErrPrint("widget_create returns %d\n", ret);
	}

	if (widget_is_all_paused()) {
		DbgPrint("Box is paused\n");
		(void)widget_system_event(arg->pkgname, arg->id, WIDGET_SYS_EVENT_PAUSED);
	}

	return ret;
}

static int method_delete(struct widget_event_arg *arg, void *data)
{
	int ret;

	DbgPrint("pkgname[%s] id[%s]\n", arg->pkgname, arg->id);

	if (arg->info.widget_destroy.type == WIDGET_DESTROY_TYPE_DEFAULT || arg->info.widget_destroy.type == WIDGET_DESTROY_TYPE_UNINSTALL) {
		DbgPrint("Box is deleted from the viewer\n");
		(void)widget_system_event(arg->pkgname, arg->id, WIDGET_SYS_EVENT_DELETED);
	}

	invoke_pre_callback(WIDGET_PRE_DESTROY_CALLBACK, arg->id);
	ret = widget_destroy(arg->pkgname, arg->id, 0);
	return ret;
}

static int method_content_event(struct widget_event_arg *arg, void *data)
{
	int ret;
	struct widget_event_info info;

	info = arg->info.content_event.info;

	ret = widget_script_event(arg->pkgname, arg->id,
			arg->info.content_event.signal_name, arg->info.content_event.source,
			&info);
	return ret;
}

static int method_clicked(struct widget_event_arg *arg, void *data)
{
	int ret;

	DbgPrint("pkgname[%s] id[%s] event[%s] timestamp[%lf] x[%lf] y[%lf]\n",
			arg->pkgname, arg->id,
			arg->info.clicked.event, arg->info.clicked.timestamp,
			arg->info.clicked.x, arg->info.clicked.y);
	ret = widget_clicked(arg->pkgname, arg->id,
			arg->info.clicked.event,
			arg->info.clicked.timestamp, arg->info.clicked.x, arg->info.clicked.y);

	return ret;
}

static int method_text_signal(struct widget_event_arg *arg, void *data)
{
	int ret;
	struct widget_event_info info;

	info = arg->info.text_signal.info;

	DbgPrint("pkgname[%s] id[%s] signal_name[%s] source[%s]\n", arg->pkgname, arg->id, arg->info.text_signal.signal_name, arg->info.text_signal.source);
	ret = widget_script_event(arg->pkgname, arg->id,
			arg->info.text_signal.signal_name, arg->info.text_signal.source,
			&info);

	return ret;
}

static int method_resize(struct widget_event_arg *arg, void *data)
{
	int ret;

	invoke_pre_callback(WIDGET_PRE_RESIZE_CALLBACK, arg->id);

	DbgPrint("pkgname[%s] id[%s] w[%d] h[%d]\n", arg->pkgname, arg->id, arg->info.resize.w, arg->info.resize.h);
	ret = widget_viewer_resize_widget(arg->pkgname, arg->id, arg->info.resize.w, arg->info.resize.h);

	return ret;
}

static int method_set_period(struct widget_event_arg *arg, void *data)
{
	int ret;
	DbgPrint("pkgname[%s] id[%s] period[%lf]\n", arg->pkgname, arg->id, arg->info.set_period.period);
	ret = widget_viewer_set_period(arg->pkgname, arg->id, arg->info.set_period.period);
	return ret;
}

static int method_change_group(struct widget_event_arg *arg, void *data)
{
	int ret;
	DbgPrint("pkgname[%s] id[%s] cluster[%s] category[%s]\n", arg->pkgname, arg->id, arg->info.change_group.cluster, arg->info.change_group.category);
	ret = widget_change_group(arg->pkgname, arg->id, arg->info.change_group.cluster, arg->info.change_group.category);
	return ret;
}

static int method_pinup(struct widget_event_arg *arg, void *data)
{
	DbgPrint("pkgname[%s] id[%s] state[%d]\n", arg->pkgname, arg->id, arg->info.pinup.state);
	arg->info.pinup.content_info = widget_pinup(arg->pkgname, arg->id, arg->info.pinup.state);
	return arg->info.pinup.content_info ? WIDGET_ERROR_NONE : WIDGET_ERROR_NOT_SUPPORTED;
}

static int method_update_content(struct widget_event_arg *arg, void *data)
{
	int ret;

	if (!arg->id || !strlen(arg->id)) {
		if (arg->info.update_content.content && strlen(arg->info.update_content.content)) {
			DbgPrint("pkgname[%s] content[%s]\n", arg->pkgname, arg->info.update_content.content);
			ret = widget_set_content_info_all(arg->pkgname, arg->info.update_content.content);
		} else {
			DbgPrint("pkgname[%s] cluster[%s] category[%s]\n", arg->pkgname, arg->info.update_content.cluster, arg->info.update_content.category);
			ret = widget_update_all(arg->pkgname, arg->info.update_content.cluster, arg->info.update_content.category, arg->info.update_content.force);
		}
	} else {
		if (arg->info.update_content.content && strlen(arg->info.update_content.content)) {
			DbgPrint("id[%s] content[%s]\n", arg->id, arg->info.update_content.content);
			ret = widget_set_content_info(arg->pkgname, arg->id, arg->info.update_content.content);
		} else {
			DbgPrint("Update [%s]\n", arg->id);
			ret = widget_update(arg->pkgname, arg->id, arg->info.update_content.force);
		}
	}

	return ret;
}

static int method_pause(struct widget_event_arg *arg, void *data)
{
	widget_pause_all();

	if (s_info.ping_timer) {
		ecore_timer_freeze(s_info.ping_timer);
	}

	if (WIDGET_CONF_SLAVE_AUTO_CACHE_FLUSH) {
		elm_cache_all_flush();
		sqlite3_release_memory(WIDGET_CONF_SQLITE_FLUSH_MAX);
		malloc_trim(0);
	}

	return WIDGET_ERROR_NONE;
}

static int method_resume(struct widget_event_arg *arg, void *data)
{
	widget_resume_all();

	if (s_info.ping_timer) {
		ecore_timer_thaw(s_info.ping_timer);
	}

	return WIDGET_ERROR_NONE;
}

static Eina_Bool send_ping_cb(void *data)
{
	widget_provider_send_ping();
	return ECORE_CALLBACK_RENEW;
}

static int method_disconnected(struct widget_event_arg *arg, void *data)
{
	if (s_info.ping_timer) {
		ecore_timer_del(s_info.ping_timer);
		s_info.ping_timer = NULL;
	}

	main_app_exit();
	return WIDGET_ERROR_NONE;
}

static int method_connected(struct widget_event_arg *arg, void *data)
{
	int ret;
	ret = widget_provider_send_hello();
	if (ret == 0) {
		double ping_interval;

		ping_interval = WIDGET_CONF_DEFAULT_PING_TIME / 2.0f;
		DbgPrint("Ping Timer: %lf\n", ping_interval);

		s_info.ping_timer = ecore_timer_add(ping_interval, send_ping_cb, NULL);
		if (!s_info.ping_timer) {
			ErrPrint("Failed to add a ping timer\n");
		}
	}

	return WIDGET_ERROR_NONE;
}

static int method_gbar_created(struct widget_event_arg *arg, void *data)
{
	int ret;

	ret = widget_open_gbar(arg->pkgname, arg->id);
	if (ret < 0) {
		DbgPrint("%s Open PD: %d\n", arg->id, ret);
	}

	return WIDGET_ERROR_NONE;
}

static int method_gbar_destroyed(struct widget_event_arg *arg, void *data)
{
	int ret;

	ret = widget_close_gbar(arg->pkgname, arg->id);
	if (ret < 0) {
		DbgPrint("%s Close PD: %d\n", arg->id, ret);
	}

	return WIDGET_ERROR_NONE;
}

static int method_gbar_moved(struct widget_event_arg *arg, void *data)
{
	int ret;
	struct widget_event_info info;

	memset(&info, 0, sizeof(info));
	info.pointer.x = arg->info.gbar_move.x;
	info.pointer.y = arg->info.gbar_move.y;
	info.pointer.down = 0;

	ret = widget_script_event(arg->pkgname, arg->id,
			"gbar,move", util_uri_to_path(arg->id), &info);
	return ret;
}

static int method_widget_pause(struct widget_event_arg *arg, void *data)
{
	int ret;

	ret = widget_pause(arg->pkgname, arg->id);

	if (WIDGET_CONF_SLAVE_AUTO_CACHE_FLUSH) {
		elm_cache_all_flush();
		sqlite3_release_memory(WIDGET_CONF_SQLITE_FLUSH_MAX);
		malloc_trim(0);
	}

	return ret;
}

static int method_widget_resume(struct widget_event_arg *arg, void *data)
{
	return widget_resume(arg->pkgname, arg->id);
}

static int method_viewer_connected(struct widget_event_arg *arg, void *data)
{
	return widget_viewer_connected(arg->pkgname, arg->id, arg->info.viewer_connected.direct_addr);
}

static int method_viewer_disconnected(struct widget_event_arg *arg, void *data)
{
	return widget_viewer_disconnected(arg->pkgname, arg->id, arg->info.viewer_disconnected.direct_addr);
}

static int method_orientation(struct widget_event_arg *arg, void *data)
{
	int ret;

	ret = widget_set_orientation(arg->pkgname, arg->id, arg->info.orientation.degree);
	invoke_pre_callback(WIDGET_PRE_ORIENTATION_CALLBACK, arg->id);

	return ret;
}

HAPI int client_init(const char *name, const char *abi, const char *accel, int secured)
{
	struct widget_event_table table = {
		.widget_create = method_new,
		.widget_recreate = method_renew,
		.widget_destroy = method_delete,
		.content_event = method_content_event,
		.clicked = method_clicked,
		.text_signal = method_text_signal,
		.resize = method_resize,
		.set_period = method_set_period,
		.change_group = method_change_group,
		.pinup = method_pinup,
		.update_content = method_update_content,
		.pause = method_pause,
		.resume = method_resume,
		.disconnected = method_disconnected,
		.connected = method_connected,
		.gbar_create = method_gbar_created,
		.gbar_destroy = method_gbar_destroyed,
		.gbar_move = method_gbar_moved,
		.widget_pause = method_widget_pause,
		.widget_resume = method_widget_resume,
		.viewer_connected = method_viewer_connected,
		.viewer_disconnected = method_viewer_disconnected,
		.orientation = method_orientation,
	};

	widget_provider_prepare_init(abi, accel, secured);
	return widget_provider_init(util_screen_get(), name, &table, NULL, 1, 1);
}

HAPI int client_fini(void)
{
	(void)widget_provider_fini();
	return WIDGET_ERROR_NONE;
}

/* End of a file */

