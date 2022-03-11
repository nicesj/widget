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
#include <string.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>

#include <widget_errno.h>
#include <widget_service.h>
#include <widget_conf.h>
#include <widget_abi.h>
#include <widget_util.h>

#include <app.h>
#include <dlog.h>
#include <pkgmgr-info.h>

#include <Ecore.h>
#include <Eina.h>

#include <packet.h>
#include <com-core_packet.h>

#include "widget_provider.h"
#include "widget_provider_buffer.h"
#include "debug.h"
#include "widget_provider_app.h"
#include "widget_provider_app_internal.h"
#include "client.h"
#include "util.h"
#include "connection.h"
#include "widget_port.h"
#include "widget_cmd_list.h"

#define PKGMGR_COMPONENT_TYPE_WATCH_APP "watchapp"
#define BUFFER_MAX 256

int errno;

enum state {
	STATE_UNKNOWN = 0,
	STATE_RESUMED = 1,
	STATE_PAUSED = 2
};

struct package {
	char *id;

	Eina_List *inst_list;
};

struct item {
	struct package *pkg_info;
	char *id;
	void *update_timer;
	struct widget_provider_event_callback *table;
	enum state state;
	void *data;
	Eina_List *handle_list;
	int degree;
	struct _last_ctrl_mode {
		int cmd;
		int value;
	} last_ctrl_mode;
};

struct internal_item {
	char *id;
	void *data;
};

static struct info {
	int secured;
	char *abi;
	char *name;
	char *hw_acceleration;
	Ecore_Timer *ping_timer;
	Eina_List *pkg_list;
	enum state state;
	int initialized;
	Eina_List *internal_item_list;
	Eina_List *widget_pre_callback_list[WIDGET_PRE_CALLBACK_COUNT];
} s_info = {
	.secured = 0,
	.abi = NULL,
	.name = NULL,
	.hw_acceleration = NULL,
	.ping_timer = NULL,
	.pkg_list = NULL,
	.state = STATE_UNKNOWN,
	.initialized = 0,
	.internal_item_list = NULL,
	.widget_pre_callback_list = { NULL, },
};

struct pre_callback_item {
	widget_pre_callback_t cb;
	void *data;
};

static void instance_destroy(struct item *item);

static void invoke_pre_callback(widget_pre_callback_e type, const char *id)
{
	Eina_List *l;
	Eina_List *n;
	struct pre_callback_item *item;

	EINA_LIST_FOREACH_SAFE(s_info.widget_pre_callback_list[type], l, n, item) {
		item->cb(id, item->data);
	}
}

static inline char *package_get_pkgid(const char *appid)
{
	int ret;
	pkgmgrinfo_appinfo_h handle;
	char *new_appid = NULL;
	char *pkgid = NULL;

	ret = pkgmgrinfo_appinfo_get_appinfo(appid, &handle);
	if (ret != PMINFO_R_OK) {
		ErrPrint("Failed to get appinfo\n");
		return NULL;
	}

	ret = pkgmgrinfo_appinfo_get_pkgid(handle, &new_appid);
	if (ret != PMINFO_R_OK) {
		pkgmgrinfo_appinfo_destroy_appinfo(handle);
		ErrPrint("Failed to get pkgname for (%s)\n", appid);
		return NULL;
	}

	if (new_appid && new_appid[0] != '\0') {
		pkgid = strdup(new_appid);
		if (!pkgid) {
			ErrPrint("strdup: %d\n", errno);
		}
	}
	pkgmgrinfo_appinfo_destroy_appinfo(handle);

	return pkgid;
}

char *widget_pkgname(void)
{
	widget_list_h list_handle;
	const char *abi_pkgname = NULL;
	char *pkgid;
	char *converted_provider_pkgname = NULL;
	int verified;
	char *widget_id;

	pkgid = package_get_pkgid(widget_find_pkgname(NULL));
	list_handle = widget_service_create_widget_list(pkgid, NULL);
	abi_pkgname = s_info.abi ? widget_abi_get_pkgname_by_abi(s_info.abi) : NULL;
	DbgFree(pkgid);
	if (!abi_pkgname) {
		widget_service_destroy_widget_list(list_handle);
		ErrPrint("Failed to get pkgname[%s]", s_info.abi);
		return NULL;
	}

	verified = 0;
	widget_id = NULL;
	while (widget_service_get_item_from_widget_list(list_handle, NULL, &widget_id, NULL) == WIDGET_ERROR_NONE) {
		if (!widget_id) {
			ErrPrint("Invalid widget_id\n");
			continue;
		}

		// tmp == /APPID/.provider <<- PROVIDER UI-APP
		// widget_id == org.tizen.watch-hello <<-- WIDGET ID
		// provider_pkgname == org.tizen.watch-hello.provider
		converted_provider_pkgname = widget_util_replace_string(abi_pkgname, WIDGET_CONF_REPLACE_TAG_APPID, widget_id);
		if (!converted_provider_pkgname) {
			DbgFree(widget_id);
			widget_id = NULL;
			continue;
		}

		/* Verify the Package Name */
		verified = !strcmp(converted_provider_pkgname, widget_find_pkgname(NULL));
		DbgFree(converted_provider_pkgname);

		if (verified) {
			break;
		}

		DbgFree(widget_id);
		widget_id = NULL;
	}

	widget_service_destroy_widget_list(list_handle);
	return widget_id;
}

static Eina_Bool periodic_updator(void *data)
{
	struct item *item = data;

	if (item->table->update) {
		int ret;
		ret = item->table->update(widget_util_uri_to_path(item->id), NULL, 0, item->table->data);
		if (ret < 0) {
			ErrPrint("Provider update: [%s] returns 0x%X\n", item->id, ret);
		}
	}

	return ECORE_CALLBACK_RENEW;
}

static struct method s_table[] = {
	{
		.cmd = NULL,
		.handler = NULL,
	},
};

static struct internal_item *internal_item_create(const char *uri)
{
	struct internal_item *item;

	item = malloc(sizeof(*item));
	if (!item) {
		ErrPrint("malloc: %d\n", errno);
		return NULL;
	}

	item->id = strdup(uri);
	if (!item->id) {
		ErrPrint("strdup: %d\n", errno);
		free(item);
		return NULL;
	}

	s_info.internal_item_list = eina_list_append(s_info.internal_item_list, item);

	return item;
}

static void internal_item_destroy(struct internal_item *item)
{
	s_info.internal_item_list = eina_list_remove(s_info.internal_item_list, item);

	free(item->id);
	free(item);
}

static struct internal_item *internal_item_find(const char *uri)
{
	Eina_List *l;
	struct internal_item *item;

	EINA_LIST_FOREACH(s_info.internal_item_list, l, item) {
		if (!strcmp(item->id, uri)) {
			return item;
		}
	}

	return NULL;
}

static struct package *package_create(const char *pkgid)
{
	struct package *pkg_info;

	pkg_info = calloc(1, sizeof(*pkg_info));
	if (!pkg_info) {
		ErrPrint("malloc : %d\n", errno);
		return NULL;
	}

	pkg_info->id = strdup(pkgid);
	if (!pkg_info->id) {
		ErrPrint("strdup: %d\n", errno);
		free(pkg_info);
		return NULL;
	}

	s_info.pkg_list = eina_list_append(s_info.pkg_list, pkg_info);
	return pkg_info;
}

static struct package *package_find(const char *pkgid)
{
	Eina_List *l;
	struct package *pkg_info;

	EINA_LIST_FOREACH(s_info.pkg_list, l, pkg_info) {
		if (!strcmp(pkg_info->id, pkgid)) {
			return pkg_info;
		}
	}

	return NULL;
}

static void package_destroy(struct package *pkg_info)
{
	s_info.pkg_list = eina_list_remove(s_info.pkg_list, pkg_info);
	free(pkg_info->id);
	free(pkg_info);
}

static struct item *instance_create(const char *pkgid, const char *id, double period, struct widget_provider_event_callback *table, const char *direct_addr)
{
	struct item *item;
	struct package *pkg_info;

	pkg_info = package_find(pkgid);
	if (!pkg_info) {
		pkg_info = package_create(pkgid);
		if (!pkg_info) {
			return NULL;
		}
	}

	item = calloc(1, sizeof(*item));
	if (!item) {
		ErrPrint("calloc: %d\n", errno);
		return NULL;
	}

	item->id = strdup(id);
	if (!item->id) {
		ErrPrint("strdup: %d\n", errno);
		free(item);
		return NULL;
	}

	/*
	 * If the "secured" flag is toggled,
	 * The master will send the update event to this provider app.
	 */
	if (!s_info.secured && period > 0.0f) {
		item->update_timer = util_timer_add(period, periodic_updator, item);
		if (!item->update_timer) {
			ErrPrint("Failed to create a timer\n");
			free(item->id);
			free(item);
			return NULL;
		}
	}

	item->table = table;
	if (direct_addr && direct_addr[0]) {
		struct connection *conn_handle;

		conn_handle = connection_find_by_addr(direct_addr);
		if (!conn_handle) {
			conn_handle = connection_create(direct_addr, (void *)s_table);
			if (!conn_handle) {
				ErrPrint("Failed to create a new connection\n");
			} else {
				DbgPrint("Connection is created: %s\n", direct_addr);
				item->handle_list = eina_list_append(item->handle_list, conn_handle);
			}
		} else {
			(void)connection_ref(conn_handle);
			DbgPrint("Connection is referred: %s\n", direct_addr);
			item->handle_list = eina_list_append(item->handle_list, conn_handle);
		}
	}

	item->state = STATE_UNKNOWN;
	item->pkg_info = pkg_info;
	pkg_info->inst_list = eina_list_append(pkg_info->inst_list, item);

	return item;
}

static void instance_destroy(struct item *item)
{
	struct connection *conn_handle;
	struct package *pkg_info;

	pkg_info = item->pkg_info;
	pkg_info->inst_list = eina_list_remove(pkg_info->inst_list, item);

	if (item->update_timer) {
		util_timer_del(item->update_timer);
	}

	EINA_LIST_FREE(item->handle_list, conn_handle) {
		(void)connection_unref(conn_handle);
	}

	free(item->id);
	free(item);

	if (!pkg_info->inst_list) {
		package_destroy(pkg_info);
	}
}

struct item *instance_find(const char *id)
{
	Eina_List *l;
	Eina_List *pl;
	struct item *item;
	struct package *pkg_info;

	EINA_LIST_FOREACH(s_info.pkg_list, pl, pkg_info) {
		EINA_LIST_FOREACH(pkg_info->inst_list, l, item) {
			if (!strcmp(item->id, id)) {
				return item;
			}
		}
	}

	return NULL;
}

PAPI char *widget_provider_app_get_widget_id(const char *id)
{
	char *uri;
	char *widget_id = NULL;
	struct item *item;

	uri = util_path_to_uri(id);
	if (!uri) {
		return NULL;
	}

	item = instance_find(uri);
	if (item) {
		widget_id = strdup(item->pkg_info->id);
	}

	free(uri);
	return widget_id;
}

const char *instance_uri(struct item *item)
{
	return item->id;
}

const char *instance_pkgid(struct item *item)
{
	return item->pkg_info->id;
}

static int method_new(struct widget_event_arg *arg, int *width, int *height, double *priority, void *data)
{
	struct widget_provider_event_callback *table = data;
	struct item *inst;
	int ret;

	DbgPrint("Create: pkgname[%s], id[%s], content[%s], timeout[%d], has_script[%d], period[%lf], cluster[%s], category[%s], skip[%d], abi[%s], size: %dx%d, ori: %d\n",
			arg->pkgname, arg->id,
			arg->info.widget_create.content,
			arg->info.widget_create.timeout,
			arg->info.widget_create.has_script,
			arg->info.widget_create.period,
			arg->info.widget_create.cluster, arg->info.widget_create.category,
			arg->info.widget_create.skip_need_to_create,
			arg->info.widget_create.abi,
			arg->info.widget_create.width,
			arg->info.widget_create.height,
			arg->info.widget_create.degree);

	invoke_pre_callback(WIDGET_PRE_CREATE_CALLBACK, arg->id);

	if (!table->create) {
		ErrPrint("Function is not implemented\n");
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	inst = instance_create(arg->pkgname, arg->id, arg->info.widget_create.period, table, arg->info.widget_create.direct_addr);
	if (!inst) {
		return WIDGET_ERROR_FAULT;
	}

	inst->degree = arg->info.widget_create.degree;

	ret = table->create(widget_util_uri_to_path(arg->id), arg->info.widget_create.content, arg->info.widget_create.width, arg->info.widget_create.height, table->data);
	if (ret != WIDGET_ERROR_NONE) {
		ErrPrint("Failed to create an instance\n");
		instance_destroy(inst);
	} else {
		struct widget_buffer *buffer;

		buffer = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, inst->pkg_info->id, arg->id);
		if (buffer) {
			int bps;
			(void)widget_provider_buffer_get_size(buffer, width, height, &bps);
		}
	}

	return ret;
}

static int method_renew(struct widget_event_arg *arg, void *data)
{
	struct widget_provider_event_callback *table = data;
	struct item *inst;
	int ret;

	DbgPrint("Re-create: pkgname[%s], id[%s], content[%s], timeout[%d], has_script[%d], period[%lf], cluster[%s], category[%s], abi[%s], size: %dx%d, ori: %d\n",
			arg->pkgname, arg->id,
			arg->info.widget_recreate.content,
			arg->info.widget_recreate.timeout,
			arg->info.widget_recreate.has_script,
			arg->info.widget_recreate.period,
			arg->info.widget_recreate.cluster,
			arg->info.widget_recreate.category,
			arg->info.widget_recreate.abi,
			arg->info.widget_recreate.width,
			arg->info.widget_recreate.height,
			arg->info.widget_recreate.degree);

	invoke_pre_callback(WIDGET_PRE_CREATE_CALLBACK, arg->id);

	if (!table->create) {
		ErrPrint("Function is not implemented\n");
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	inst = instance_create(arg->pkgname, arg->id, arg->info.widget_recreate.period, table, arg->info.widget_recreate.direct_addr);
	if (!inst) {
		return WIDGET_ERROR_FAULT;
	}

	inst->degree = arg->info.widget_recreate.degree;

	ret = table->create(widget_util_uri_to_path(arg->id), arg->info.widget_recreate.content, arg->info.widget_recreate.width, arg->info.widget_recreate.height, table->data);
	if (ret < 0) {
		instance_destroy(inst);
	}

	return ret;
}

static int method_delete(struct widget_event_arg *arg, void *data)
{
	struct widget_provider_event_callback *table = data;
	struct item *inst;
	int ret;

	DbgPrint("pkgname[%s] id[%s]\n", arg->pkgname, arg->id);

	invoke_pre_callback(WIDGET_PRE_DESTROY_CALLBACK, arg->id);

	if (!table->destroy) {
		ErrPrint("Function is not implemented\n");
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	inst = instance_find(arg->id);
	if (!inst) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	ret = table->destroy(widget_util_uri_to_path(arg->id), arg->info.widget_destroy.type, table->data);
	instance_destroy(inst);
	return ret;
}

static int method_content_event(struct widget_event_arg *arg, void *data)
{
	struct widget_provider_event_callback *table = data;
	int ret;

	if (!table->text_signal) {
		ErrPrint("Function is not implemented\n");
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	ret = table->text_signal(widget_util_uri_to_path(arg->id),
			arg->info.content_event.signal_name, arg->info.content_event.source,
			&arg->info.content_event.info,
			table->data);

	return ret;
}

static int method_clicked(struct widget_event_arg *arg, void *data)
{
	int ret = WIDGET_ERROR_NONE;
#ifdef WIDGET_FEATURE_HANDLE_CLICKED_EVENT
	struct widget_provider_event_callback *table = data;

	DbgPrint("pkgname[%s] id[%s] event[%s] timestamp[%lf] x[%lf] y[%lf]\n",
			arg->pkgname, arg->id,
			arg->info.clicked.event, arg->info.clicked.timestamp,
			arg->info.clicked.x, arg->info.clicked.y);

	if (!table->clicked) {
		ErrPrint("Function is not implemented\n");
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	ret = table->clicked(widget_util_uri_to_path(arg->id), arg->info.clicked.event, arg->info.clicked.x, arg->info.clicked.y, arg->info.clicked.timestamp, table->data);
#endif /* WIDGET_FEATURE_HANDLE_CLICKED_EVENT */

	return ret;
}

static int method_text_signal(struct widget_event_arg *arg, void *data)
{
	struct widget_provider_event_callback *table = data;
	int ret;

	if (!table->text_signal) {
		ErrPrint("Function is not implemented\n");
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	ret = table->text_signal(widget_util_uri_to_path(arg->id),
			arg->info.text_signal.signal_name, arg->info.text_signal.source,
			&arg->info.text_signal.info, table->data);

	return ret;
}

static int method_resize(struct widget_event_arg *arg, void *data)
{
	struct widget_provider_event_callback *table = data;
	int ret;

	invoke_pre_callback(WIDGET_PRE_RESIZE_CALLBACK, arg->id);

	if (!table->resize) {
		ErrPrint("Function is not implemented\n");
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	DbgPrint("pkgname[%s] id[%s] w[%d] h[%d]\n", arg->pkgname, arg->id, arg->info.resize.w, arg->info.resize.h);
	ret = table->resize(widget_util_uri_to_path(arg->id), arg->info.resize.w, arg->info.resize.h, table->data);
	return ret;
}

static int method_set_period(struct widget_event_arg *arg, void *data)
{
	struct item *inst;

	DbgPrint("pkgname[%s] id[%s] period[%lf]\n", arg->pkgname, arg->id, arg->info.set_period.period);
	inst = instance_find(arg->id);
	if (!inst) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (inst->update_timer) {
		util_timer_interval_set(inst->update_timer, arg->info.set_period.period);
	}

	return WIDGET_ERROR_NONE;
}

static int method_change_group(struct widget_event_arg *arg, void *data)
{
	return WIDGET_ERROR_NOT_SUPPORTED;
}

static int method_pinup(struct widget_event_arg *arg, void *data)
{
	return WIDGET_ERROR_NOT_SUPPORTED;
}

static int method_update_content(struct widget_event_arg *arg, void *data)
{
	struct widget_provider_event_callback *table = data;
	struct package *pkg_info;
	struct item *inst;
	Eina_List *l;
	int ret;

	if (!table->update) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	pkg_info = package_find(arg->pkgname);
	if (!pkg_info) {
		return WIDGET_ERROR_NOT_EXIST;
	}

	if (!arg->id || arg->id[0] == '\0') {
		/**
		 * Burst Update required. Every instances should be updated for this package.
		 * Find the instance list of given package and then trigger each instance's update_content event callback
		 */
		int cnt = 0;
		DbgPrint("Burst update is triggerd for [%s]\n", arg->pkgname);

		EINA_LIST_FOREACH(pkg_info->inst_list, l, inst) {
			ret = table->update(widget_util_uri_to_path(inst->id), arg->info.update_content.content, arg->info.update_content.force, table->data);
			DbgPrint("UpdateContent[%s] returns 0x%X\n", widget_util_uri_to_path(inst->id), ret);
			cnt++;
		}

		DbgPrint("%d instnaces update function is triggered\n", cnt);
		ret = cnt > 0 ? WIDGET_ERROR_NONE : WIDGET_ERROR_NOT_EXIST; /** override ret in this case */
	} else {
		/**
		 * Do we need to validate id first?
		 */
		ret = WIDGET_ERROR_NOT_EXIST;
		EINA_LIST_FOREACH(pkg_info->inst_list, l, inst) {
			if (!strcmp(inst->id, arg->id)) {
				ret = WIDGET_ERROR_NONE;
				break;
			}
		}

		if (ret == WIDGET_ERROR_NONE) {
			ret = table->update(widget_util_uri_to_path(arg->id), arg->info.update_content.content, arg->info.update_content.force, table->data);
		} else {
			ErrPrint("Instance is not valid: %s\n", arg->id);
		}
	}

	return ret;
}

static int method_pause(struct widget_event_arg *arg, void *data)
{
	struct widget_provider_event_callback *table = data;
	Eina_List *l;
	Eina_List *pl;
	struct package *pkg_info;
	struct item *inst;

	if (s_info.state == STATE_PAUSED) {
		DbgPrint("Already paused\n");
		return WIDGET_ERROR_NONE;
	}

	s_info.state = STATE_PAUSED;

	EINA_LIST_FOREACH(s_info.pkg_list, pl, pkg_info) {
		EINA_LIST_FOREACH(pkg_info->inst_list, l, inst) {
			if (inst->state != STATE_RESUMED) {
				continue;
			}

			if (inst->update_timer && !WIDGET_CONF_UPDATE_ON_PAUSE) {
				util_timer_freeze(inst->update_timer);
			}

			if (table->pause) {
				table->pause(widget_util_uri_to_path(inst->id), table->data);
			}
		}
	}

	if (s_info.ping_timer) {
		ecore_timer_freeze(s_info.ping_timer);
	}

	return WIDGET_ERROR_NONE;
}

static int method_resume(struct widget_event_arg *arg, void *data)
{
	struct widget_provider_event_callback *table = data;
	Eina_List *l;
	Eina_List *pl;
	struct item *inst;
	struct package *pkg_info;

	if (s_info.state == STATE_RESUMED) {
		DbgPrint("Already resumed\n");
		return WIDGET_ERROR_NONE;
	}

	s_info.state = STATE_RESUMED;

	EINA_LIST_FOREACH(s_info.pkg_list, pl, pkg_info) {
		EINA_LIST_FOREACH(pkg_info->inst_list, l, inst) {
			if (inst->state != STATE_RESUMED) {
				continue;
			}

			if (inst->update_timer) {
				util_timer_thaw(inst->update_timer);
			}

			if (table->resume) {
				table->resume(widget_util_uri_to_path(inst->id), table->data);
			}
		}
	}

	if (s_info.ping_timer) {
		ecore_timer_thaw(s_info.ping_timer);
	}

	return WIDGET_ERROR_NONE;
}

static int method_orientation(struct widget_event_arg *arg, void *data)
{
	struct widget_provider_event_callback *table = data;
	struct item *inst;

	inst = instance_find(arg->id);
	if (!inst) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (inst->degree != arg->info.orientation.degree) {
		inst->degree = arg->info.orientation.degree;

		invoke_pre_callback(WIDGET_PRE_ORIENTATION_CALLBACK, arg->id);

		if (table->orientation) {
			table->orientation(widget_util_uri_to_path(inst->id), inst->degree, table->data);
		} else {
			return WIDGET_ERROR_NOT_SUPPORTED;
		}
	}

	return WIDGET_ERROR_NONE;
}

static int method_ctrl_mode(struct widget_event_arg *arg, void *data)
{
	struct widget_provider_event_callback *table = data;
	struct item *inst;

	inst = instance_find(arg->id);
	if (!inst) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	inst->last_ctrl_mode.cmd = arg->info.ctrl_mode.cmd;
	inst->last_ctrl_mode.value = arg->info.ctrl_mode.value;

	invoke_pre_callback(WIDGET_PRE_CTRL_MODE_CALLBACK, arg->id);

	if (table->ctrl_mode) {
		table->ctrl_mode(widget_util_uri_to_path(inst->id), arg->info.ctrl_mode.cmd, arg->info.ctrl_mode.value, table->data);
	} else {
		return WIDGET_ERROR_NOT_SUPPORTED;
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
	struct widget_provider_event_callback *table = data;
	struct item *item;
	struct package *pkg_info;
	Eina_List *inst_list;
	Eina_List *l;
	Eina_List *n;
	Eina_List *pl;
	Eina_List *pn;

	/* arg == NULL */

	if (s_info.ping_timer) {
		ecore_timer_del(s_info.ping_timer);
		s_info.ping_timer = NULL;
	}

	/**
	 * After clean up all connections to master, (why?)
	 * invoke destroy callback for every instances.
	 */
	EINA_LIST_FOREACH_SAFE(s_info.pkg_list, pl, pn, pkg_info) {
		/**
		* @note
		 * instance_destroy will delete the "pkg_info" when it deletes the last instance.
		 * So we should be able to access the instance list without pkg_info object.
		 */
		inst_list = pkg_info->inst_list;
		EINA_LIST_FOREACH_SAFE(inst_list, l, n, item) {
			invoke_pre_callback(WIDGET_PRE_DESTROY_CALLBACK, item->id);

			if (table->destroy) {
				(void)table->destroy(widget_util_uri_to_path(item->id), WIDGET_DESTROY_TYPE_FAULT, item->table->data);
			}

			/**
			 * @note
			 * instance_destroy will remove the "item" from this "inst_list"
			 */
			instance_destroy(item);
		}
		/**
		 * @note
		 * From now, we should not access the pkg_info anymore.
		 * It will be deleted by instance_destroy.
		 */
	}

	if (table->disconnected) {
		table->disconnected(table->data);
	}

	client_fini();

	return WIDGET_ERROR_NONE;
}

static int method_connected(struct widget_event_arg *arg, void *data)
{
	int ret;
	struct widget_provider_event_callback *table = data;

	ret = widget_provider_send_hello();
	if (ret == 0) {
		s_info.ping_timer = ecore_timer_add(WIDGET_CONF_DEFAULT_PING_TIME / 2.0f, send_ping_cb, NULL);
		if (!s_info.ping_timer) {
			ErrPrint("Failed to add a ping timer\n");
		}
	}

	if (table->connected) {
		table->connected(table->data);
	}

	return WIDGET_ERROR_NONE;
}

static int method_viewer_connected(struct widget_event_arg *arg, void *data)
{
	struct item *item;

	item = instance_find(arg->id);
	if (!item) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (arg->info.viewer_connected.direct_addr && arg->info.viewer_connected.direct_addr[0]) {
		struct connection *handle;
		/**
		 * \TODO: Create a new connection if the direct_addr is valid
		 */
		handle = connection_find_by_addr(arg->info.viewer_connected.direct_addr);
		if (handle) {
			(void)connection_ref(handle);
			DbgPrint("Connection is referred: %s\n", arg->info.viewer_connected.direct_addr);
			item->handle_list = eina_list_append(item->handle_list, handle);
		} else {
			handle = connection_create(arg->info.viewer_connected.direct_addr, (void *)s_table);
			if (!handle) {
				ErrPrint("Failed to create a connection\n");
			} else {
				DbgPrint("Connection is added: %s\n", arg->info.viewer_connected.direct_addr);
				item->handle_list = eina_list_append(item->handle_list, handle);
			}
		}
	} else {
		DbgPrint("Newly comming connection has no valid direct_addr\n");
	}

	return WIDGET_ERROR_NONE;
}

static int method_viewer_disconnected(struct widget_event_arg *arg, void *data)
{
	struct item *item;

	item = instance_find(arg->id);
	if (!item) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (arg->info.viewer_disconnected.direct_addr && arg->info.viewer_disconnected.direct_addr[0]) {
		struct connection *handle;
		/**
		 * \TODO: Create a new connection if the direct_addr is valid
		 */
		handle = connection_find_by_addr(arg->info.viewer_disconnected.direct_addr);
		if (handle) {
			if (eina_list_data_find(item->handle_list, handle)) {
				item->handle_list = eina_list_remove(item->handle_list, handle);
				(void)connection_unref(handle);
			}
		} else {
			ErrPrint("There is no valid connection object: %s\n", arg->info.viewer_disconnected.direct_addr);
		}
	} else {
		DbgPrint("Disconnected connection has no valid direct_addr\n");
	}

	return WIDGET_ERROR_NONE;
}

static int method_gbar_created(struct widget_event_arg *arg, void *data)
{
	int ret = WIDGET_ERROR_NONE;
#ifdef WIDGET_FEATURE_GBAR_SUPPORTED
	struct widget_provider_event_callback *table = data;

	if (!table->gbar.create) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	ret = table->gbar.create(widget_util_uri_to_path(arg->id), arg->info.gbar_create.w, arg->info.gbar_create.h, arg->info.gbar_create.x, arg->info.gbar_create.y, table->data);
#else
	ret = WIDGET_ERROR_NOT_SUPPORTED;
#endif /* WIDGET_FEATURE_GBAR_SUPPORTED */
	return ret;
}

static int method_gbar_destroyed(struct widget_event_arg *arg, void *data)
{
	int ret = WIDGET_ERROR_NONE;
#ifdef WIDGET_FEATURE_GBAR_SUPPORTED
	struct widget_provider_event_callback *table = data;

	if (!table->gbar.destroy) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	ret = table->gbar.destroy(widget_util_uri_to_path(arg->id), arg->info.gbar_destroy.reason, table->data)
#else
	ret = WIDGET_ERROR_NOT_SUPPORTED;
#endif /* WIDGET_FEATURE_GBAR_SUPPORTED */
	return ret;
}

static int method_gbar_moved(struct widget_event_arg *arg, void *data)
{
	int ret = WIDGET_ERROR_NONE;
#ifdef WIDGET_FEATURE_GBAR_SUPPORTED
	struct widget_provider_event_callback *table = data;
	if (!table->gbar.resize_move) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	ret = table->gbar.resize_move(widget_util_uri_to_path(arg->id), arg->info.gbar_move.w, arg->info.gbar_move.h, arg->info.gbar_move.x, arg->info.gbar_move.y, table->data);
#else
	ret = WIDGET_ERROR_NOT_SUPPORTED;
#endif /* WIDGET_FEATURE_GBAR_SUPPORTED */
	return ret;
}

static int method_widget_pause(struct widget_event_arg *arg, void *data)
{
	struct widget_provider_event_callback *table = data;
	int ret;
	struct item *inst;

	inst = instance_find(arg->id);
	if (!inst) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (inst->state == STATE_PAUSED) {
		DbgPrint("Already paused\n");
		return WIDGET_ERROR_ALREADY_EXIST;
	}

	inst->state = STATE_PAUSED;

	if (s_info.state != STATE_RESUMED) {
		return WIDGET_ERROR_NONE;
	}

	if (inst->update_timer && !WIDGET_CONF_UPDATE_ON_PAUSE) {
		util_timer_freeze(inst->update_timer);
	}

	if (!table->pause) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	ret = table->pause(widget_util_uri_to_path(arg->id), table->data);
	return ret;
}

static int method_widget_resume(struct widget_event_arg *arg, void *data)
{
	struct widget_provider_event_callback *table = data;
	struct item *inst;
	int ret;

	inst = instance_find(arg->id);
	if (!inst) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (inst->state == STATE_RESUMED) {
		return WIDGET_ERROR_ALREADY_EXIST;
	}

	inst->state = STATE_RESUMED;

	if (s_info.state != STATE_RESUMED) {
		return WIDGET_ERROR_NONE;
	}

	if (inst->update_timer) {
		util_timer_thaw(inst->update_timer);
	}

	if (!table->resume) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	ret = table->resume(widget_util_uri_to_path(arg->id), table->data);
	return ret;
}

static int connection_disconnected_cb(int handle, void *data)
{
	struct connection *connection;
	struct connection *conn_handle;
	struct package *pkg_info;
	struct item *inst;
	Eina_List *pl;
	Eina_List *l;
	Eina_List *k;
	Eina_List *n;

	connection = connection_find_by_fd(handle);
	if (!connection) {
		return 0;
	}

	EINA_LIST_FOREACH(s_info.pkg_list, pl, pkg_info) {
		EINA_LIST_FOREACH(pkg_info->inst_list, l, inst) {
			EINA_LIST_FOREACH_SAFE(inst->handle_list, k, n, conn_handle) {
				if (conn_handle == connection) {
					/**
					 * @note
					 * This instance has connection to client
					 * but now it is lost.
					 * by reset "handle",
					 * the provider_app_send_updated function will send event to the master.
					 */
					DbgPrint("Disconnected: %s\n", inst->id);

					/**
					 * To prevent from nested callback call.
					 * reset handle first.
					 */
					inst->handle_list = eina_list_remove(inst->handle_list, conn_handle);

					(void)connection_unref(conn_handle);
				}
			}
		}
	}

	return 0;
}

void client_fini(void)
{
	struct internal_item *item;

	if (!s_info.initialized) {
		LOGE("Provider is not initialized\n");
		return;
	}

	DbgPrint("Finalize the Provider App Connection\n");
	s_info.initialized = 0;

	widget_provider_fini();

	DbgPrint("Provider is disconnected(%s)\n", s_info.abi);
	free(s_info.name);
	free(s_info.abi);
	free(s_info.hw_acceleration);

	widget_conf_reset();
	connection_fini();
	widget_abi_fini();

	s_info.name = NULL;
	s_info.abi = NULL;
	s_info.hw_acceleration = NULL;

	connection_del_event_handler(CONNECTION_EVENT_TYPE_DISCONNECTED, connection_disconnected_cb);

	EINA_LIST_FREE(s_info.internal_item_list, item) {
		DbgPrint("Internal item[%s] destroyed\n", item->id);
		free(item->id);
		free(item);
	}
}

int client_is_initialized(void)
{
	return s_info.initialized;
}

static int method_connected_sync(struct widget_event_arg *arg, void *data)
{
	int ret;
	struct widget_provider_event_callback *table = data;

	/**
	 * @note
	 *
	 * hello_sync will invoke the widget_create funcion from its inside.
	 * So we should to call the connected event callback first.
	 * We should keep callback sequence.
	 *
	 * connected -> widget_created
	 */

	if (table->connected) {
		table->connected(table->data);
	}

	ret = widget_provider_send_hello_sync(widget_provider_app_pkgname());

	if (ret == WIDGET_ERROR_NONE) {
		s_info.ping_timer = ecore_timer_add(WIDGET_CONF_DEFAULT_PING_TIME / 2.0f, send_ping_cb, NULL);
		if (!s_info.ping_timer) {
			ErrPrint("Failed to add a ping timer\n");
		}
	}

	return ret;
}

static int is_watchapp(void)
{
	int result = 0;
	pkgmgrinfo_appinfo_h handle = NULL;
	char *value;

	if (pkgmgrinfo_appinfo_get_appinfo(widget_find_pkgname(NULL), &handle) != PMINFO_R_OK) {
		ErrPrint("appid[%s] is invalid\n", widget_find_pkgname(NULL));
		return 0;
	}

	if (pkgmgrinfo_appinfo_get_component_type(handle, &value) == PMINFO_R_OK) {
		if (value) {
			DbgPrint("component type: %s\n", value);
			if (!strcmp(value, PKGMGR_COMPONENT_TYPE_WATCH_APP)) {
				result = 1;
				DbgPrint("this app is watch app");
			}
		}
	}

	pkgmgrinfo_appinfo_destroy_appinfo(handle);

	return result;
}

int client_init_sync(struct widget_provider_event_callback *table)
{
	int ret;
	char *name = NULL;
	char *abi = NULL;
	char *hw_acceleration = NULL;

	struct widget_event_table method_table = {
		.widget_create = method_new,
		.widget_recreate = method_renew,
		.widget_destroy = method_delete,
		.resize = method_resize,
		.update_content = method_update_content,
		.content_event = method_content_event,
		.clicked = method_clicked,
		.text_signal = method_text_signal,
		.set_period = method_set_period,
		.change_group = method_change_group,
		.pinup = method_pinup,
		.pause = method_pause,
		.resume = method_resume,
		.widget_pause = method_widget_pause,
		.widget_resume = method_widget_resume,
		.disconnected = method_disconnected,
		.connected = method_connected_sync,
		.viewer_connected = method_viewer_connected,
		.viewer_disconnected = method_viewer_disconnected,

		/**
		 * Glance Bar
		 */
		.gbar_create = method_gbar_created,
		.gbar_destroy = method_gbar_destroyed,
		.gbar_move = method_gbar_moved,
		.orientation = method_orientation,
		.ctrl_mode = method_ctrl_mode,
	};

	if (s_info.initialized == 1) {
		DbgPrint("Provider App is already initialized\n");
		return WIDGET_ERROR_NONE;
	}

	widget_abi_init();
	connection_init();

	if (s_info.name && s_info.abi) {
		DbgPrint("Name and ABI is assigned already\n");
		connection_fini();
		widget_abi_fini();
		return WIDGET_ERROR_NONE;
	}

	if (!widget_conf_is_loaded()) {
		widget_conf_reset();
		if (widget_conf_load() < 0) {
			ErrPrint("Failed to load conf\n");
		}
	}

	if (!is_watchapp()) {
		ErrPrint("This Provider is not watch app\n");
		widget_conf_reset();
		connection_fini();
		widget_abi_fini();
		return WIDGET_ERROR_NONE;
	}

	name = malloc(BUFFER_MAX);
	if (!name) {
		widget_conf_reset();
		connection_fini();
		widget_abi_fini();
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	snprintf(name, BUFFER_MAX - 1, "%d.%lf", getpid(), util_timestamp());

	abi = strdup("app");
	if (!abi) {
		free(name);
		widget_conf_reset();
		connection_fini();
		widget_abi_fini();
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	hw_acceleration = strdup("use-sw");
	if (!hw_acceleration) {
		free(name);
		free(abi);
		widget_conf_reset();
		connection_fini();
		widget_abi_fini();
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	DbgPrint("Name assigned: %s (%s)\n", name, abi);
	DbgPrint("Secured: %s\n", "true");
	DbgPrint("hw-acceleration: %s\n", hw_acceleration);

	widget_provider_prepare_init(abi, hw_acceleration, 1);
	ret = widget_provider_init(util_screen_get(), name, &method_table, table, 1, 1);
	ErrPrint("widget_provider_init return [%d]\n", ret);
	if (ret < 0) {
		free(hw_acceleration);
		free(name);
		free(abi);
		widget_conf_reset();
		connection_fini();
		widget_abi_fini();
	} else {
		s_info.initialized = 1;
		s_info.name = name;
		s_info.abi = abi;
		s_info.secured = 1;
		s_info.hw_acceleration = hw_acceleration;

		if (connection_add_event_handler(CONNECTION_EVENT_TYPE_DISCONNECTED, connection_disconnected_cb, NULL) < 0) {
			ErrPrint("Failed to add a disconnected event callback\n");
		}
	}

	return ret;
}

int client_init(app_control_h service, struct widget_provider_event_callback *table)
{
	int ret;
	char *secured = NULL;
	char *name = NULL;
	char *abi = NULL;
	char *hw_acceleration = NULL;
	struct widget_event_table method_table = {
		.widget_create = method_new,
		.widget_recreate = method_renew,
		.widget_destroy = method_delete,
		.gbar_create = method_gbar_created,
		.gbar_destroy = method_gbar_destroyed,
		.gbar_move = method_gbar_moved,
		.resize = method_resize,
		.update_content = method_update_content,
		.content_event = method_content_event,
		.clicked = method_clicked,
		.text_signal = method_text_signal,
		.set_period = method_set_period,
		.change_group = method_change_group,
		.pinup = method_pinup,
		.pause = method_pause,
		.resume = method_resume,
		.widget_pause = method_widget_pause,
		.widget_resume = method_widget_resume,
		.disconnected = method_disconnected,
		.connected = method_connected,
		.viewer_connected = method_viewer_connected,
		.viewer_disconnected = method_viewer_disconnected,
		.orientation = method_orientation,
		.ctrl_mode = method_ctrl_mode,
	};

	if (s_info.initialized == 1) {
		DbgPrint("Provider App is already initialized\n");
		return WIDGET_ERROR_NONE;
	}

	widget_abi_init();
	connection_init();

	if (s_info.name && s_info.abi) {
		DbgPrint("Name and ABI is assigned already\n");
		connection_fini();
		widget_abi_fini();
		return WIDGET_ERROR_NONE;
	}

	if (!widget_conf_is_loaded()) {
		widget_conf_reset();
		if (widget_conf_load() < 0) {
			ErrPrint("Failed to load conf\n");
		}
	}

	ret = app_control_get_extra_data(service, WIDGET_CONF_BUNDLE_SLAVE_NAME, &name);
	if (ret != APP_CONTROL_ERROR_NONE) {
		ErrPrint("Name is not valid\n");
		widget_conf_reset();
		connection_fini();
		widget_abi_fini();
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	ret = app_control_get_extra_data(service, WIDGET_CONF_BUNDLE_SLAVE_SECURED, &secured);
	if (ret != APP_CONTROL_ERROR_NONE) {
		free(name);
		ErrPrint("Secured is not valid\n");
		widget_conf_reset();
		connection_fini();
		widget_abi_fini();
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	ret = app_control_get_extra_data(service, WIDGET_CONF_BUNDLE_SLAVE_ABI, &abi);
	if (ret != APP_CONTROL_ERROR_NONE) {
		free(name);
		free(secured);
		widget_conf_reset();
		connection_fini();
		widget_abi_fini();
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	ret = app_control_get_extra_data(service, WIDGET_CONF_BUNDLE_SLAVE_HW_ACCELERATION, &hw_acceleration);
	if (ret != APP_CONTROL_ERROR_NONE) {
		DbgPrint("hw-acceleration is not set\n");
	}

	if (name && abi && secured) {
		DbgPrint("Name assigned: %s (%s)\n", name, abi);
		DbgPrint("Secured: %s\n", s_info.secured);
		DbgPrint("hw-acceleration: %s\n", hw_acceleration);

		widget_provider_prepare_init(abi, hw_acceleration, !strcmp(secured, "true"));
		ret = widget_provider_init(util_screen_get(), name, &method_table, table, 1, 1);
		if (ret < 0) {
			free(name);
			free(abi);
			free(hw_acceleration);
			widget_conf_reset();
			connection_fini();
			widget_abi_fini();
		} else {
			s_info.initialized = 1;
			s_info.name = name;
			s_info.abi = abi;
			s_info.secured = !strcasecmp(secured, "true");
			s_info.hw_acceleration = hw_acceleration;

			if (connection_add_event_handler(CONNECTION_EVENT_TYPE_DISCONNECTED, connection_disconnected_cb, NULL) < 0) {
				ErrPrint("Failed to add a disconnected event callback\n");
			}
		}
		free(secured);
	} else {
		free(name);
		free(abi);
		free(secured);
		free(hw_acceleration);
		widget_conf_reset();
		connection_fini();
		widget_abi_fini();
		ret = WIDGET_ERROR_INVALID_PARAMETER;
	}

	return ret;
}

PAPI int widget_provider_app_send_updated_event(const char *id, int idx, int x, int y, int w, int h, int for_gbar)
{
	struct item *inst;
	char *uri;
	int ret = WIDGET_ERROR_DISABLED;
	char *desc_name = NULL;
	widget_damage_region_s region = {
		.x = x,
		.y = y,
		.w = w,
		.h = h,
	};

	if (!id) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	uri = util_path_to_uri(id);
	if (!uri) {
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	if (for_gbar) {
		int len;

		len = strlen(id) + strlen(".desc") + 3;

		desc_name = malloc(len);
		if (!desc_name) {
			ErrPrint("malloc: %d\n", errno);
			free(uri);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}

		snprintf(desc_name, len, "%s.desc", id);
	}

	/**
	 * Do we have to search instance in this function?
	 * This function is very frequently called one.
	 * So we have to do not heavy operation in here!!!
	 * Keep it in mind.
	 */
	inst = instance_find(uri);
	if (inst) {
		Eina_List *l;
		struct connection *conn_handle;

		EINA_LIST_FOREACH(inst->handle_list, l, conn_handle) {
			ret = widget_provider_send_direct_updated(connection_handle(conn_handle), inst->pkg_info->id, uri, idx, &region, for_gbar, desc_name);
		}

		/**
		 * by this logic,
		 * Even if we lost direct connection to the viewer,
		 * we will send this to the master again.
		 */
		if (ret != WIDGET_ERROR_NONE) {
			ret = widget_provider_send_updated(inst->pkg_info->id, uri, idx, &region, for_gbar, desc_name);
		}
	} else {
		ErrPrint("Instance is not exists (%s)\n", uri);
		ret = WIDGET_ERROR_NOT_EXIST;
	}

	free(desc_name);
	free(uri);

	return ret;
}

PAPI int widget_provider_app_send_buffer_updated_event(widget_buffer_h handle, int idx, int x, int y, int w, int h, int for_gbar)
{
	struct item *inst;
	int ret = WIDGET_ERROR_DISABLED;
	char *desc_name = NULL;
	const char *uri;
	widget_damage_region_s region = {
		.x = x,
		.y = y,
		.w = w,
		.h = h,
	};

	if (!handle) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	uri = widget_provider_buffer_id(handle);
	if (!uri) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (for_gbar) {
		int len;
		const char *id;

		id = widget_util_uri_to_path(uri);
		if (!id) {
			return WIDGET_ERROR_FAULT;
		}

		len = strlen(id) + strlen(".desc") + 3;
		desc_name = malloc(len);
		if (!desc_name) {
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}
		snprintf(desc_name, len, "%s.desc", id);
	}

	/**
	 * Do we have to search instance in this function?
	 * This function is very frequently called one.
	 * So we have to do not heavy operation in here!!!
	 * Keep it in mind.
	 */
	inst = instance_find(uri);
	if (inst && inst->handle_list) {
		Eina_List *l;
		struct connection *conn_handle;

		EINA_LIST_FOREACH(inst->handle_list, l, conn_handle) {
			ret = widget_provider_send_direct_buffer_updated(connection_handle(conn_handle), handle, idx, &region, for_gbar, desc_name);
		}
	}

	/**
	 * by this logic,
	 * Even if we lost direct connection to the viewer,
	 * we will send this to the master again.
	 */
	if (ret != WIDGET_ERROR_NONE) {
		ret = widget_provider_send_buffer_updated(handle, idx, &region, for_gbar, desc_name);
	}

	free(desc_name);

	return ret;
}

PAPI int widget_provider_app_send_extra_info(const char *id, const char *content_info, const char *title)
{
	char *uri;
	int ret;

	if (!id) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	uri = util_path_to_uri(id);
	if (!uri) {
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	ret = widget_provider_send_extra_info(widget_find_pkgname(uri), uri, 1.0f, content_info, title, NULL, NULL);
	free(uri);

	return ret;
}

PAPI int widget_provider_app_set_data(const char *id, void *data)
{
	char *uri;

	if (!id) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	uri = util_path_to_uri(id);
	if (!uri) {
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	if (!strncmp(uri, SCHEMA_INTERNAL, strlen(SCHEMA_INTERNAL))) {
		struct internal_item *item;

		item = internal_item_find(uri);
		if (!item) {
			if (!data) {
				free(uri);
				return WIDGET_ERROR_NOT_EXIST;
			}

			item = internal_item_create(uri);
			if (!item) {
				free(uri);
				return WIDGET_ERROR_FAULT;
			}
			free(uri);
		} else {
			free(uri);
			if (!data) {
				internal_item_destroy(item);
				return WIDGET_ERROR_NONE;
			}
		}

		item->data = data;
	} else {
		struct item *item;

		item = instance_find(uri);
		free(uri);
		if (!item) {
			return WIDGET_ERROR_ALREADY_EXIST;
		}

		item->data = data;
	}

	return WIDGET_ERROR_NONE;
}

PAPI void *widget_provider_app_get_data(const char *id)
{
	char *uri;
	void *data = NULL;

	if (!id) {
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	uri = util_path_to_uri(id);
	if (!uri) {
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	if (!strncmp(uri, SCHEMA_INTERNAL, strlen(SCHEMA_INTERNAL))) {
		struct internal_item *item;

		item = internal_item_find(uri);
		free(uri);
		if (!item) {
			set_last_result(WIDGET_ERROR_NOT_EXIST);
			return NULL;
		}
		data = item->data;
	} else {
		struct item *item;

		item = instance_find(uri);
		free(uri);
		if (!item) {
			set_last_result(WIDGET_ERROR_NOT_EXIST);
			return NULL;
		}
		data = item->data;
	}

	return data;
}

PAPI void *widget_provider_app_get_data_list(void)
{
	struct item *item;
	struct internal_item *internal_item;
	struct package *pkg_info;
	Eina_List *return_list = NULL;
	Eina_List *l;
	Eina_List *pl;

	EINA_LIST_FOREACH(s_info.pkg_list, pl, pkg_info) {
		EINA_LIST_FOREACH(pkg_info->inst_list, l, item) {
			return_list = eina_list_append(return_list, item->data);
		}
	}

	EINA_LIST_FOREACH(s_info.internal_item_list, l, internal_item) {
		return_list = eina_list_append(return_list, internal_item->data);
	}

	return return_list;
}

PAPI int widget_provider_app_create_app(void)
{
	eina_init();
	return WIDGET_ERROR_NONE;
}

PAPI int widget_provider_app_terminate_app(widget_destroy_type_e reason, int destroy_instances)
{
	struct widget_provider_event_callback *table;
	struct package *pkg_info;
	struct item *item;
	Eina_List *inst_list;
	Eina_List *pl;
	Eina_List *pn;
	Eina_List *l;
	Eina_List *n;
	int ret = 0;

	DbgPrint("Reason: %d, %d\n", reason, destroy_instances);

	if (!destroy_instances) {
		DbgPrint("Do not destroy instances\n");
		return WIDGET_ERROR_NONE;
	}

	table = widget_provider_callback_data();
	if (!table) {
		ErrPrint("Provider App is not initialized\n");
		return WIDGET_ERROR_FAULT;
	}

	EINA_LIST_FOREACH_SAFE(s_info.pkg_list, pl, pn, pkg_info) {
		/**
		 * @note
		 * instance_destroy will delete the pkg_info.
		 * So we should not access it after call the instance_destroy
		 */
		inst_list = pkg_info->inst_list;
		EINA_LIST_FOREACH_SAFE(inst_list, l, n, item) {
			invoke_pre_callback(WIDGET_PRE_DESTROY_CALLBACK, item->id);

			if (table->destroy) {
				(void)table->destroy(widget_util_uri_to_path(item->id), reason, table->data);
			}

			instance_destroy(item);
			ret++;
		}
	}

	DbgPrint("%d instances are destroyed\n", ret);
	eina_shutdown();

	return ret ? WIDGET_ERROR_NONE : WIDGET_ERROR_NOT_EXIST;
}

PAPI int widget_provider_app_add_pre_callback(widget_pre_callback_e type, widget_pre_callback_t cb, void *data)
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

PAPI int widget_provider_app_del_pre_callback(widget_pre_callback_e type, widget_pre_callback_t cb, void *data)
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

PAPI int widget_provider_app_get_orientation(const char *id)
{
	char *uri;
	struct item *inst;

	uri = util_path_to_uri(id);
	if (!uri) {
		ErrPrint("Invalid URI [%s]\n", id);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	inst = instance_find(uri);
	free(uri);

	if (!inst) {
		ErrPrint("[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	return inst->degree;
}

PAPI int widget_provider_app_get_last_ctrl_mode(const char *id, int *cmd, int *value)
{
	char *uri;
	struct item *inst;

	uri = util_path_to_uri(id);
	if (!uri) {
		ErrPrint("Invalid URI [%s]\n", id);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	inst = instance_find(uri);
	free(uri);

	if (!inst) {
		ErrPrint("[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	if (cmd) {
		*cmd = inst->last_ctrl_mode.cmd;
	}

	if (value) {
		*value = inst->last_ctrl_mode.value;
	}

	return WIDGET_ERROR_NONE;
}

/**
 * } End of a file
 */
