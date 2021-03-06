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
#include <errno.h>
#include <dlfcn.h> /* dlopen */
#include <stdlib.h> /* malloc, free */
#include <string.h> /* strcmp */

#include <dlog.h>
#include <Eina.h>
#include <widget_provider.h>
#include <widget_service.h>
#include <widget_service_internal.h>
#include <widget_errno.h>
#include <widget_script.h>
#include <widget_conf.h>

#include "main.h"
#include "critical_log.h"
#include "debug.h"
#include "so_handler.h"
#include "fault.h"
#include "util.h"
#include "conf.h"

int errno;

static struct info {
	Eina_List *widget_list;
	enum current_operations current_op;
} s_info = {
	.widget_list = NULL,
	.current_op = WIDGET_OP_UNKNOWN,
};

static inline struct so_item *find_widget(const char *pkgname)
{
	Eina_List *l;
	struct so_item *item;

	EINA_LIST_FOREACH(s_info.widget_list, l, item) {
		if (!strcmp(item->pkgname, pkgname)) {
			return item;
		}
	}

	return NULL;
}

static inline char *so_adaptor_alloc(const char *abi)
{
	/* TODO: Implement me */
	DbgPrint("ABI[%s] loads %s\n", abi, "/usr/lib/libwidget-cpp.so");
	return strdup("/usr/lib/libwidget-cpp.so");
}

static inline char *so_path_alloc(const char *pkgname)
{
	char *widget_id;
	char *path;

	widget_id = widget_service_get_widget_id(pkgname);
	if (!widget_id) {
		ErrPrint("Failed to get package name\n");
		return NULL;
	} else {
		path = widget_service_get_libexec(widget_id);
		free(widget_id);
	}

	DbgPrint("so path: %s\n", path);
	return path;
}

static void delete_widget(struct so_item *item)
{
	int ret;

	fault_mark_call(item->pkgname, "finalize", __func__, USE_ALARM, DEFAULT_LIFE_TIMER);

	if (item->adaptor.finalize) {
		ret = item->adaptor.finalize(item->pkgname);
	} else if (item->widget.finalize) {
		ret = item->widget.finalize();
	} else {
		ErrPrint("%s has no finalize\n", item->pkgname);
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}

	fault_unmark_call(item->pkgname, "finalize", __func__, USE_ALARM);

	if (ret == WIDGET_ERROR_RESOURCE_BUSY) {
		DbgPrint("Keep SO in a process space (%s)\n", item->so_fname);
	} else {
		if (ret < 0) {
			ErrPrint("Package %s, finalize returns %d\n", item->pkgname, ret);
		}
		DbgPrint("Unload SO from process space (%s)\n", item->so_fname);
		s_info.widget_list = eina_list_remove(s_info.widget_list, item);
		main_heap_monitor_del_target(item->so_fname);
		util_dump_current_so_info(item->so_fname);
		if (dlclose(item->handle) != 0) {
			ErrPrint("dlclose: %s\n", dlerror());
		}
		free(item->so_fname);
		free(item->pkgname);
		free(item);
	}
}

static struct so_item *new_adaptor(const char *pkgname, const char *abi)
{
	struct so_item *item;
	char *errmsg;

	item = calloc(1, sizeof(*item));
	if (!item) {
		ErrPrint("calloc: %d\n", errno);
		return NULL;
	}

	item->pkgname = strdup(pkgname);
	if (!item->pkgname) {
		ErrPrint("strdup: %d\n", errno);
		free(item);
		return NULL;
	}

	/*! \TODO:
	 * item->timeout
	 */

	/*! \TODO
	 * item->has_widget_script
	 */

	item->inst_list = NULL;

	item->so_fname = so_adaptor_alloc(abi);
	if (!item->so_fname) {
		free(item->pkgname);
		free(item);
		return NULL;
	}

	fault_mark_call(pkgname, __func__, __func__, USE_ALARM, DEFAULT_LOAD_TIMER);
	item->handle = dlopen(item->so_fname, RTLD_LOCAL | RTLD_NOW | RTLD_DEEPBIND);
	if (!item->handle) {
		fault_unmark_call(pkgname, __func__, __func__, USE_ALARM);
		ErrPrint("dlopen: %s - %s\n", dlerror(), item->so_fname);
		free(item->so_fname);
		free(item->pkgname);
		free(item);
		return NULL;
	}
	fault_unmark_call(pkgname, __func__, __func__, USE_ALARM);

	errmsg = dlerror();
	if (errmsg) {
		DbgPrint("dlerror(can be ignored): %s\n", errmsg);
	}

	item->adaptor.create = (adaptor_create_t)dlsym(item->handle, "widget_create");
	if (!item->adaptor.create) {
		ErrPrint("symbol: widget_create - %s\n", dlerror());
		delete_widget(item);
		return NULL;
	}

	item->adaptor.destroy = (adaptor_destroy_t)dlsym(item->handle, "widget_destroy");
	if (!item->adaptor.destroy) {
		ErrPrint("symbol: widget_destroy - %s\n", dlerror());
		delete_widget(item);
		return NULL;
	}

	item->adaptor.pinup = (adaptor_pinup_t)dlsym(item->handle, "widget_pinup");
	if (!item->adaptor.pinup) {
		ErrPrint("symbol: widget_pinup - %s\n", dlerror());
	}

	item->adaptor.is_updated = (adaptor_is_updated_t)dlsym(item->handle, "widget_need_to_update");
	if (!item->adaptor.is_updated) {
		ErrPrint("symbol: widget_need_to_update - %s\n", dlerror());
	}

	item->adaptor.update_content = (adaptor_update_content_t)dlsym(item->handle, "widget_update_content");
	if (!item->adaptor.update_content) {
		ErrPrint("symbol: widget_update_content - %s\n", dlerror());
	}

	item->adaptor.clicked = (adaptor_clicked_t)dlsym(item->handle, "widget_clicked");
	if (!item->adaptor.clicked) {
		ErrPrint("symbol: widget_clicked - %s\n", dlerror());
	}

	item->adaptor.text_signal = (adaptor_text_signal_t)dlsym(item->handle, "widget_content_event");
	if (!item->adaptor.text_signal) {
		ErrPrint("symbol: widget_content_event - %s\n", dlerror());
	}

	item->adaptor.resize = (adaptor_resize_t)dlsym(item->handle, "widget_resize");
	if (!item->adaptor.resize) {
		ErrPrint("symbol: widget_resize - %s\n", dlerror());
	}

	item->adaptor.create_needed = (adaptor_create_needed_t)dlsym(item->handle, "widget_need_to_create");
	if (!item->adaptor.create_needed) {
		ErrPrint("symbol: widget_need_to_create - %s\n", dlerror());
	}

	item->adaptor.change_group = (adaptor_change_group_t)dlsym(item->handle, "widget_change_group");
	if (!item->adaptor.change_group) {
		ErrPrint("symbol: widget_change_group - %s\n", dlerror());
	}

	item->adaptor.get_output_info = (adaptor_get_output_info_t)dlsym(item->handle, "widget_get_info");
	if (!item->adaptor.get_output_info) {
		ErrPrint("symbol: widget_get_info - %s\n", dlerror());
	}

	item->adaptor.initialize = (adaptor_initialize_t)dlsym(item->handle, "widget_initialize");
	if (!item->adaptor.initialize) {
		ErrPrint("symbol: widget_initialize - %s\n", dlerror());
	}

	item->adaptor.finalize = (adaptor_finalize_t)dlsym(item->handle, "widget_finalize");
	if (!item->adaptor.finalize) {
		ErrPrint("symbol: widget_finalize - %s\n", dlerror());
	}

	item->adaptor.need_to_destroy = (adaptor_need_to_destroy_t)dlsym(item->handle, "widget_need_to_destroy");
	if (!item->adaptor.need_to_destroy) {
		ErrPrint("symbol: widget_need_to_destroy - %s\n", dlerror());
	}

	item->adaptor.sys_event = (adaptor_system_event_t)dlsym(item->handle, "widget_system_event");
	if (!item->adaptor.sys_event) {
		ErrPrint("symbol: lievbox_system_event - %s\n", dlerror());
	}

	item->adaptor.is_pinned_up = (adaptor_is_pinned_up_t)dlsym(item->handle, "widget_is_pinned_up");
	if (!item->adaptor.is_pinned_up) {
		ErrPrint("symbol: widget_is_pinned_up - %s\n", dlerror());
	}

	item->adaptor.get_alt_info = (adaptor_get_alt_info_t)dlsym(item->handle, "widget_get_alt_info");
	if (!item->adaptor.get_alt_info) {
		ErrPrint("symbol: widget_get_alt_info - %s\n", dlerror());
	}

	item->adaptor.set_content_info = (adaptor_set_content_info_t)dlsym(item->handle, "widget_set_content_info");
	if (!item->adaptor.set_content_info) {
		ErrPrint("symbol: widget_set_content_info - %s\n", dlerror());
	}

	if (item->adaptor.initialize) {
		int ret;
		fault_mark_call(pkgname, "initialize", __func__, USE_ALARM, DEFAULT_LIFE_TIMER);

		ret = item->adaptor.initialize(pkgname);

		fault_unmark_call(pkgname, "initialize", __func__, USE_ALARM);
		if (ret < 0) {
			ErrPrint("Failed to initialize package %s\n", pkgname);
			delete_widget(item);
			return NULL;
		}
	}

	s_info.widget_list = eina_list_append(s_info.widget_list, item);
	return item;
}

static struct so_item *new_widget(const char *pkgname)
{
	struct so_item *item;
	char *errmsg;

	item = calloc(1, sizeof(*item));
	if (!item) {
		ErrPrint("calloc: %d\n", errno);
		return NULL;
	}

	item->pkgname = strdup(pkgname);
	if (!item->pkgname) {
		ErrPrint("strdup: %d\n", errno);
		free(item);
		return NULL;
	}

	/*! \TODO:
	 * item->timeout
	 */

	/*! \TODO
	 * item->has_widget_script
	 */

	item->inst_list = NULL;

	item->so_fname = so_path_alloc(pkgname);
	if (!item->so_fname) {
		free(item->pkgname);
		free(item);
		return NULL;
	}

	fault_mark_call(pkgname, __func__, __func__, USE_ALARM, DEFAULT_LOAD_TIMER);
	item->handle = dlopen(item->so_fname, RTLD_LOCAL | RTLD_NOW | RTLD_DEEPBIND);
	if (!item->handle) {
		fault_unmark_call(pkgname, __func__, __func__, USE_ALARM);
		ErrPrint("dlopen: %s - %s\n", dlerror(), item->so_fname);
		free(item->so_fname);
		free(item->pkgname);
		free(item);
		return NULL;
	}
	fault_unmark_call(pkgname, __func__, __func__, USE_ALARM);

	errmsg = dlerror();
	if (errmsg) {
		DbgPrint("dlerror(can be ignored): %s\n", errmsg);
	}

	item->widget.create = (create_t)dlsym(item->handle, "widget_create");
	if (!item->widget.create) {
		ErrPrint("symbol: widget_create - %s\n", dlerror());
		delete_widget(item);
		return NULL;
	}

	item->widget.destroy = (destroy_t)dlsym(item->handle, "widget_destroy");
	if (!item->widget.destroy) {
		ErrPrint("symbol: widget_destroy - %s\n", dlerror());
		delete_widget(item);
		return NULL;
	}

	item->widget.pinup = (pinup_t)dlsym(item->handle, "widget_pinup");
	if (!item->widget.pinup) {
		ErrPrint("symbol: widget_pinup - %s\n", dlerror());
	}

	item->widget.is_updated = (is_updated_t)dlsym(item->handle, "widget_need_to_update");
	if (!item->widget.is_updated) {
		ErrPrint("symbol: widget_need_to_update - %s\n", dlerror());
	}

	item->widget.update_content = (update_content_t)dlsym(item->handle, "widget_update_content");
	if (!item->widget.update_content) {
		ErrPrint("symbol: widget_update_content - %s\n", dlerror());
	}

	item->widget.clicked = (clicked_t)dlsym(item->handle, "widget_clicked");
	if (!item->widget.clicked) {
		ErrPrint("symbol: widget_clicked - %s\n", dlerror());
	}

	item->widget.text_signal = (text_signal_t)dlsym(item->handle, "widget_content_event");
	if (!item->widget.text_signal) {
		ErrPrint("symbol: widget_content_event - %s\n", dlerror());
	}

	item->widget.resize = (resize_t)dlsym(item->handle, "widget_resize");
	if (!item->widget.resize) {
		ErrPrint("symbol: widget_resize - %s\n", dlerror());
	}

	item->widget.create_needed = (create_needed_t)dlsym(item->handle, "widget_need_to_create");
	if (!item->widget.create_needed) {
		ErrPrint("symbol: widget_need_to_create - %s\n", dlerror());
	}

	item->widget.change_group = (change_group_t)dlsym(item->handle, "widget_change_group");
	if (!item->widget.change_group) {
		ErrPrint("symbol: widget_change_group - %s\n", dlerror());
	}

	item->widget.get_output_info = (get_output_info_t)dlsym(item->handle, "widget_get_info");
	if (!item->widget.get_output_info) {
		ErrPrint("symbol: widget_get_info - %s\n", dlerror());
	}

	item->widget.initialize = (initialize_t)dlsym(item->handle, "widget_initialize");
	if (!item->widget.initialize) {
		ErrPrint("symbol: widget_initialize - %s\n", dlerror());
	}

	item->widget.finalize = (finalize_t)dlsym(item->handle, "widget_finalize");
	if (!item->widget.finalize) {
		ErrPrint("symbol: widget_finalize - %s\n", dlerror());
	}

	item->widget.need_to_destroy = (need_to_destroy_t)dlsym(item->handle, "widget_need_to_destroy");
	if (!item->widget.need_to_destroy) {
		ErrPrint("symbol: widget_need_to_destroy - %s\n", dlerror());
	}

	item->widget.sys_event = (system_event_t)dlsym(item->handle, "widget_system_event");
	if (!item->widget.sys_event) {
		ErrPrint("symbol: widget_system_event - %s\n", dlerror());
	}

	item->widget.is_pinned_up = (is_pinned_up_t)dlsym(item->handle, "widget_is_pinned_up");
	if (!item->widget.is_pinned_up) {
		ErrPrint("symbol: widget_is_pinned_up - %s\n", dlerror());
	}

	item->widget.get_alt_info = (get_alt_info_t)dlsym(item->handle, "widget_get_alt_info");
	if (!item->widget.get_alt_info) {
		ErrPrint("symbol: widget_get_alt_info - %s\n", dlerror());
	}

	item->widget.set_content_info = (set_content_info_t)dlsym(item->handle, "widget_set_content_info");
	if (!item->widget.set_content_info) {
		ErrPrint("symbol: widget_set_content_info - %s\n", dlerror());
	}

	main_heap_monitor_add_target(item->so_fname);

	if (item->widget.initialize) {
		int ret;
		fault_mark_call(pkgname, "initialize", __func__, USE_ALARM, DEFAULT_LIFE_TIMER);

		ret = item->widget.initialize(pkgname);

		fault_unmark_call(pkgname, "initialize", __func__, USE_ALARM);
		if (ret < 0) {
			ErrPrint("Failed to initialize package %s\n", pkgname);
			delete_widget(item);
			return NULL;
		}
	}

	s_info.widget_list = eina_list_append(s_info.widget_list, item);
	return item;
}

static inline struct instance *new_instance(const char *id, const char *content, const char *cluster, const char *category)
{
	struct instance *inst;

	inst = calloc(1, sizeof(*inst));
	if (!inst) {
		ErrPrint("calloc: %d\n", errno);
		return NULL;
	}

	inst->id = strdup(id);
	if (!inst->id) {
		ErrPrint("strdup: %d\n", errno);
		free(inst);
		return NULL;
	}

	DbgPrint("Default content: [%s]\n", content);
	if (content) {
		inst->content = strdup(content);
		if (!inst->content) {
			ErrPrint("strdup: %d\n", errno);
			free(inst->id);
			free(inst);
			return NULL;
		}
	}

	if (cluster) {
		inst->cluster = strdup(cluster);
		if (!inst->cluster) {
			ErrPrint("strdup: %d\n", errno);
			free(inst->id);
			free(inst->content);
			free(inst);
			return NULL;
		}
	}

	if (category) {
		inst->category = strdup(category);
		if (!inst->category) {
			ErrPrint("strdup: %d\n", errno);
			free(inst->cluster);
			free(inst->id);
			free(inst->content);
			free(inst);
			return NULL;
		}
	}

	return inst;
}

static inline int delete_instance(struct instance *inst)
{
	free(inst->icon);
	free(inst->name);
	free(inst->cluster);
	free(inst->category);
	free(inst->id);
	free(inst->content);
	free(inst->title);
	free(inst);
	return WIDGET_ERROR_NONE;
}

static inline struct instance *find_instance(struct so_item *item, const char *id)
{
	struct instance *inst;
	Eina_List *l;

	EINA_LIST_FOREACH(item->inst_list, l, inst) {
		if (!strcmp(inst->id, id)) {
			return inst;
		}
	}

	return NULL;
}

HAPI struct instance *so_find_instance(const char *pkgname, const char *id)
{
	struct so_item *item;

	item = find_widget(pkgname);
	if (!item) {
		return NULL;
	}

	return find_instance(item, id);
}

HAPI int so_create(const char *pkgname, const char *id, const char *content_info, int timeout, int has_widget_script, const char *cluster, const char *category, const char *abi, struct instance **out)
{
	struct so_item *item;
	struct instance *inst;
	int ret;

	item = find_widget(pkgname);
	if (item) {
		inst = find_instance(item, id);
		if (inst) {
			ErrPrint("Instance: %s - %s is already exists\n", pkgname, id);
			return WIDGET_ERROR_ALREADY_EXIST;
		}
	} else {
		if (!strcasecmp(abi, "c")) {
			item = new_widget(pkgname);
		} else {
			item = new_adaptor(pkgname, abi);
		}

		if (!item) {
			return WIDGET_ERROR_FAULT;
		}
	}

	inst = new_instance(id, content_info, cluster, category);
	if (!inst) {
		if (!item->inst_list) {
			delete_widget(item);
		}

		return WIDGET_ERROR_FAULT;
	}

	item->inst_list = eina_list_append(item->inst_list, inst);
	item->has_widget_script = has_widget_script;
	item->timeout = timeout;

	fault_mark_call(pkgname, id, __func__, USE_ALARM, DEFAULT_LIFE_TIMER);

	s_info.current_op = WIDGET_OP_CREATE;
	if (item->adaptor.create) {
		ret = item->adaptor.create(pkgname, util_uri_to_path(id), content_info, cluster, category);
	} else if (item->widget.create) {
		ret = item->widget.create(util_uri_to_path(id), content_info, cluster, category);
	} else { /*! \NOTE: This is not possible, but for the exceptional handling */
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}
	s_info.current_op = WIDGET_OP_UNKNOWN;

	fault_unmark_call(pkgname, id, __func__, USE_ALARM);

	if (ret < 0) {
		item->inst_list = eina_list_remove(item->inst_list, inst);
		delete_instance(inst);

		if (!item->inst_list) {
			/* There is no instances, unload this widget */
			delete_widget(item);
		}
		return ret;
	}

	inst->item = item;
	*out = inst;
	return ret;
}

HAPI int so_destroy(struct instance *inst, int unload)
{
	struct so_item *item;
	int ret;

	item = inst->item;
	if (!item) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	fault_mark_call(item->pkgname, inst->id, __func__, USE_ALARM, DEFAULT_LIFE_TIMER);

	s_info.current_op = WIDGET_OP_DESTROY;
	if (item->adaptor.destroy) {
		ret = item->adaptor.destroy(item->pkgname, util_uri_to_path(inst->id));
	} else if (item->widget.destroy) {
		ret = item->widget.destroy(util_uri_to_path(inst->id));
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}
	s_info.current_op = WIDGET_OP_UNKNOWN;

	fault_unmark_call(item->pkgname, inst->id, __func__, USE_ALARM);

	item->inst_list = eina_list_remove(item->inst_list, inst);
	delete_instance(inst);

	if (unload && !item->inst_list) {
		delete_widget(item);
	}

	return ret;
}

HAPI char *so_pinup(struct instance *inst, int pinup)
{
	struct so_item *item;
	char *ret;

	item = inst->item;
	if (!item) {
		return NULL;
	}

	fault_mark_call(item->pkgname, inst->id, __func__, USE_ALARM, DEFAULT_LIFE_TIMER);

	s_info.current_op = WIDGET_OP_PINUP;
	if (item->adaptor.pinup) {
		ret = item->adaptor.pinup(item->pkgname, util_uri_to_path(inst->id), pinup);
	} else if (item->widget.pinup) {
		ret = item->widget.pinup(util_uri_to_path(inst->id), pinup);
	} else {
		ret = NULL;
	}
	s_info.current_op = WIDGET_OP_UNKNOWN;

	fault_unmark_call(item->pkgname, inst->id, __func__, USE_ALARM);
	return ret;
}

HAPI int so_is_pinned_up(struct instance *inst)
{
	struct so_item *item;
	int ret;

	item = inst->item;
	if (!item) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	fault_mark_call(item->pkgname, inst->id, __func__, USE_ALARM, DEFAULT_LIFE_TIMER);

	s_info.current_op = WIDGET_OP_IS_PINNED_UP;
	if (item->adaptor.is_pinned_up) {
		ret = item->adaptor.is_pinned_up(item->pkgname, util_uri_to_path(inst->id));
	} else if (item->widget.is_pinned_up) {
		ret = item->widget.is_pinned_up(util_uri_to_path(inst->id));
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}
	s_info.current_op = WIDGET_OP_UNKNOWN;

	fault_unmark_call(item->pkgname, inst->id, __func__, USE_ALARM);
	return ret;
}

HAPI int so_is_updated(struct instance *inst)
{
	struct so_item *item;
	int ret;

	item = inst->item;
	if (!item) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	fault_mark_call(item->pkgname, inst->id, __func__, USE_ALARM, DEFAULT_LIFE_TIMER);

	s_info.current_op = WIDGET_OP_NEED_TO_UPDATE;
	if (item->adaptor.is_updated) {
		ret = item->adaptor.is_updated(item->pkgname, util_uri_to_path(inst->id));
	} else if (item->widget.is_updated) {
		ret = item->widget.is_updated(util_uri_to_path(inst->id));
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}
	s_info.current_op = WIDGET_OP_UNKNOWN;

	fault_unmark_call(item->pkgname, inst->id, __func__, USE_ALARM);

	return ret;
}

HAPI int so_need_to_destroy(struct instance *inst)
{
	struct so_item *item;
	int ret;

	item = inst->item;
	if (!item) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	fault_mark_call(item->pkgname, inst->id, __func__, USE_ALARM, DEFAULT_LIFE_TIMER);

	s_info.current_op = WIDGET_OP_NEED_TO_DESTROY;
	if (item->adaptor.need_to_destroy) {
		ret = item->adaptor.need_to_destroy(item->pkgname, util_uri_to_path(inst->id));
	} else if (item->widget.need_to_destroy) {
		ret = item->widget.need_to_destroy(util_uri_to_path(inst->id));
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}
	s_info.current_op = WIDGET_OP_UNKNOWN;

	fault_unmark_call(item->pkgname, inst->id, __func__, USE_ALARM);

	return ret;
}

HAPI int so_update(struct instance *inst)
{
	struct so_item *item;
	int ret;

	item = inst->item;
	if (!item) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	fault_mark_call(item->pkgname, inst->id, __func__, USE_ALARM, DEFAULT_LIFE_TIMER);

	s_info.current_op = WIDGET_OP_UPDATE_CONTENT;
	if (item->adaptor.update_content) {
		ret = item->adaptor.update_content(item->pkgname, util_uri_to_path(inst->id));
	} else if (item->widget.update_content) {
		ret = item->widget.update_content(util_uri_to_path(inst->id));
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}
	s_info.current_op = WIDGET_OP_UNKNOWN;

	fault_unmark_call(item->pkgname, inst->id, __func__, USE_ALARM);
	return ret;
}

HAPI int so_clicked(struct instance *inst, const char *event, double timestamp, double x, double y)
{
	struct so_item *item;
	int ret;

	item = inst->item;
	if (!item) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	fault_mark_call(item->pkgname, inst->id, __func__, USE_ALARM, DEFAULT_LIFE_TIMER);

	DbgPrint("PERF_WIDGET\n");

	s_info.current_op = WIDGET_OP_CLICKED;
	if (item->adaptor.clicked) {
		ret = item->adaptor.clicked(item->pkgname, util_uri_to_path(inst->id), event, timestamp, x, y);
	} else if (item->widget.clicked) {
		ret = item->widget.clicked(util_uri_to_path(inst->id), event, timestamp, x, y);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}
	s_info.current_op = WIDGET_OP_UNKNOWN;

	fault_unmark_call(item->pkgname, inst->id, __func__, USE_ALARM);

	return ret;
}

HAPI int so_script_event(struct instance *inst, const char *signal_name, const char *source, widget_event_info_s event_info)
{
	struct so_item *item;
	int ret;

	item = inst->item;
	if (!item) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	fault_mark_call(item->pkgname, inst->id, __func__, USE_ALARM, DEFAULT_LIFE_TIMER);

	s_info.current_op = WIDGET_OP_CONTENT_EVENT;
	if (item->adaptor.text_signal) {
		ret = item->adaptor.text_signal(item->pkgname, util_uri_to_path(inst->id), signal_name, source, event_info);
	} else if (item->widget.text_signal) {
		ret = item->widget.text_signal(util_uri_to_path(inst->id), signal_name, source, event_info);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}
	s_info.current_op = WIDGET_OP_UNKNOWN;

	fault_unmark_call(item->pkgname, inst->id, __func__, USE_ALARM);

	return ret;
}

HAPI int so_resize(struct instance *inst, int w, int h)
{
	struct so_item *item;
	int ret;
	widget_size_type_e type = WIDGET_SIZE_TYPE_UNKNOWN;

	item = inst->item;
	if (!item) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	widget_service_get_size_type(w, h, &type);
	if (type == WIDGET_SIZE_TYPE_UNKNOWN) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	fault_mark_call(item->pkgname, inst->id, __func__, USE_ALARM, DEFAULT_LIFE_TIMER);

	s_info.current_op = WIDGET_OP_RESIZE;
	if (item->adaptor.resize) {
		ret = item->adaptor.resize(item->pkgname, util_uri_to_path(inst->id), type);
	} else if (item->widget.resize) {
		ret = item->widget.resize(util_uri_to_path(inst->id), type);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}
	s_info.current_op = WIDGET_OP_UNKNOWN;

	fault_unmark_call(item->pkgname, inst->id, __func__, USE_ALARM);

	return ret;
}

HAPI int so_set_content_info(struct instance *inst, const char *content_info)
{
	struct so_item *item;
	bundle *b;
	int ret;
	int len;

	item = inst->item;
	if (!item) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (content_info && (len = strlen(content_info))) {
		b = bundle_decode((const bundle_raw *)content_info, len);
	} else {
		b = NULL;
	}

	fault_mark_call(item->pkgname, inst->id, __func__, USE_ALARM, DEFAULT_LIFE_TIMER);
	s_info.current_op = WIDGET_OP_SET_CONTENT_INFO;
	if (item->adaptor.set_content_info) {
		ret = item->adaptor.set_content_info(item->pkgname, util_uri_to_path(inst->id), b);
	} else if (item->widget.set_content_info) {
		ret = item->widget.set_content_info(util_uri_to_path(inst->id), b);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (b) {
		bundle_free(b);
	}

	s_info.current_op = WIDGET_OP_UNKNOWN;
	fault_unmark_call(item->pkgname, inst->id, __func__, USE_ALARM);

	return ret;
}

HAPI int so_create_needed(const char *pkgname, const char *cluster, const char *category, const char *abi)
{
	struct so_item *item;
	int ret;

	item = find_widget(pkgname);
	if (!item) {
		if (!strcasecmp(abi, "c")) {
			item = new_widget(pkgname);
		} else {
			item = new_adaptor(pkgname, abi);
		}

		if (!item) {
			return WIDGET_ERROR_FAULT;
		}
	}

	fault_mark_call(item->pkgname, __func__, __func__, USE_ALARM, DEFAULT_LIFE_TIMER);

	s_info.current_op = WIDGET_OP_NEED_TO_CREATE;
	if (item->adaptor.create_needed) {
		ret = item->adaptor.create_needed(pkgname, cluster, category);
	} else if (item->widget.create_needed) {
		ret = item->widget.create_needed(cluster, category);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}
	s_info.current_op = WIDGET_OP_UNKNOWN;

	fault_unmark_call(item->pkgname, __func__, __func__, USE_ALARM);

	DbgPrint("[%s] returns %d\n", pkgname, ret);
	return ret;
}

HAPI int so_change_group(struct instance *inst, const char *cluster, const char *category)
{
	struct so_item *item;
	int ret;
	char *tmp_cluster;
	char *tmp_category;

	item = inst->item;
	if (!item) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	tmp_cluster = strdup(cluster);
	if (!tmp_cluster) {
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	tmp_category = strdup(category);
	if (!tmp_category) {
		free(tmp_cluster);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	fault_mark_call(item->pkgname, inst->id, __func__, USE_ALARM, DEFAULT_LIFE_TIMER);

	s_info.current_op = WIDGET_OP_CHANGE_GROUP;
	if (item->adaptor.change_group) {
		ret = item->adaptor.change_group(item->pkgname, util_uri_to_path(inst->id), cluster, category);
	} else if (item->widget.change_group) {
		ret = item->widget.change_group(util_uri_to_path(inst->id), cluster, category);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}
	s_info.current_op = WIDGET_OP_UNKNOWN;

	fault_unmark_call(item->pkgname, inst->id, __func__, USE_ALARM);
	if (ret >= 0) {
		free(inst->cluster);
		free(inst->category);

		inst->cluster = tmp_cluster;
		inst->category = tmp_category;
	} else {
		free(tmp_cluster);
		free(tmp_category);
	}

	return ret;
}

HAPI int so_get_alt_info(struct instance *inst, char **icon, char **name)
{
	struct so_item *item;
	int ret;

	item = inst->item;
	if (!item) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	*icon = NULL;
	*name = NULL;

	fault_mark_call(item->pkgname, inst->id, __func__, USE_ALARM, DEFAULT_LIFE_TIMER);

	s_info.current_op = WIDGET_OP_GET_ALT_INFO;
	if (item->adaptor.get_alt_info) {
		ret = item->adaptor.get_alt_info(item->pkgname, util_uri_to_path(inst->id), icon, name);
	} else if (item->widget.get_alt_info) {
		ret = item->widget.get_alt_info(util_uri_to_path(inst->id), icon, name);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}
	s_info.current_op = WIDGET_OP_UNKNOWN;

	fault_unmark_call(item->pkgname, inst->id, __func__, USE_ALARM);
	if (ret >= 0) {
		if (*icon) {
			free(inst->icon);
			inst->icon = *icon;
		}

		if (*name) {
			free(inst->name);
			inst->name = *name;
		}
	}

	if (main_heap_monitor_is_enabled()) {
		DbgPrint("%s allocates %d bytes\n", item->pkgname, main_heap_monitor_target_usage(item->so_fname));
	}

	return ret;
}

HAPI int so_get_output_info(struct instance *inst, int *w, int *h, double *priority, char **content, char **title)
{
	struct so_item *item;
	int ret;

	item = inst->item;
	if (!item) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	*content = NULL;
	*title = NULL;

	fault_mark_call(item->pkgname, inst->id, __func__, USE_ALARM, DEFAULT_LIFE_TIMER);

	s_info.current_op = WIDGET_OP_GET_INFO;
	if (item->adaptor.get_output_info) {
		ret = item->adaptor.get_output_info(item->pkgname, util_uri_to_path(inst->id), w, h, priority, content, title);
	} else if (item->widget.get_output_info) {
		ret = item->widget.get_output_info(util_uri_to_path(inst->id), w, h, priority, content, title);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}
	s_info.current_op = WIDGET_OP_UNKNOWN;

	fault_unmark_call(item->pkgname, inst->id, __func__, USE_ALARM);
	if (ret >= 0) {
		inst->w = *w;
		inst->h = *h;
		inst->priority = *priority;

		/*!
		 * \todo
		 * Add "*content" "*title" address validation code.
		 * using mcheck?
		 */

		if (*content) {
			free(inst->content);
			inst->content = *content;
		}

		if (*title) {
			free(inst->title);
			inst->title = *title;
		}
	}

	if (main_heap_monitor_is_enabled()) {
		DbgPrint("%s allocates %d bytes\n", item->pkgname, main_heap_monitor_target_usage(item->so_fname));
	}

	return ret;
}

HAPI int so_sys_event(struct instance *inst, int event)
{
	struct so_item *item;
	int ret;

	item = inst->item;
	if (!item) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	fault_mark_call(item->pkgname, inst->id, __func__, USE_ALARM, DEFAULT_LIFE_TIMER);

	s_info.current_op = WIDGET_OP_SYSTEM_EVENT;
	if (item->adaptor.sys_event) {
		ret = item->adaptor.sys_event(item->pkgname, util_uri_to_path(inst->id), event);
	} else if (item->widget.sys_event) {
		ret = item->widget.sys_event(util_uri_to_path(inst->id), event);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}
	s_info.current_op = WIDGET_OP_UNKNOWN;

	fault_unmark_call(item->pkgname, inst->id, __func__, USE_ALARM);
	return ret;
}

HAPI enum current_operations so_current_op(void)
{
	return s_info.current_op;
}

/* End of a file */
