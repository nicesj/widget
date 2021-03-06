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
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <mcheck.h>
#include <dlfcn.h>

#include <Elementary.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <Ecore.h>
#include <Ecore_X.h>
#include <service_app.h>
#include <Edje.h>
#include <Eina.h>
#if defined(ENABLE_EFL_ASSIST)
#include <efl_assist.h>
#endif

#include <system_settings.h>

#include <dlog.h>
#include <bundle.h>
#include <widget_service.h>
#include <widget_provider.h>
#include <widget_script.h>
#include <widget_conf.h>
#include <widget/widget.h>
#include <widget/widget_internal.h>
#include <vconf.h>
#include <widget_util.h>

#include "critical_log.h"
#include "debug.h"
#include "fault.h"
#include "update_monitor.h"
#include "client.h"
#include "util.h"
#include "so_handler.h"
#include "widget.h"
#include "conf.h"
#include "theme_loader.h"

#define WVGA_DEFAULT_SCALE 1.8f

static struct info {
	int (*heap_monitor_initialized)(void);
	size_t (*heap_monitor_target_usage)(const char *name);
	int (*heap_monitor_add_target)(const char *name);
	int (*heap_monitor_del_target)(const char *name);
	void *heap_monitor;
#if defined(ENABLE_EFL_ASSIST)
	Ea_Theme_Font_Table *table;
#endif

	app_event_handler_h lang_changed_handler;
	app_event_handler_h region_changed_handler;
} s_info = {
	.heap_monitor_initialized = NULL,
	.heap_monitor_target_usage = NULL,
	.heap_monitor_add_target = NULL,
	.heap_monitor_del_target = NULL,
	.heap_monitor = NULL,
#if defined(ENABLE_EFL_ASSIST)
	.table = NULL,
#endif
	.lang_changed_handler = NULL,
	.region_changed_handler = NULL,
};

#if defined(ENABLE_EFL_ASSIST)
static void font_changed_cb(void *user_data)
{
	DbgPrint("Font change event\n");
	widget_system_event_all(WIDGET_SYS_EVENT_FONT_CHANGED);
}
#endif

static void tts_changed_cb(keynode_t *node, void *user_data)
{
	DbgPrint("TTS status is changed\n");
	widget_system_event_all(WIDGET_SYS_EVENT_TTS_CHANGED);
}

static void mmc_changed_cb(keynode_t *node, void *user_data)
{
	DbgPrint("MMC status is changed\n");
	widget_system_event_all(WIDGET_SYS_EVENT_MMC_STATUS_CHANGED);
}

static void time_changed_cb(keynode_t *node, void *user_data)
{
	DbgPrint("Time is changed\n");
	widget_system_event_all(WIDGET_SYS_EVENT_TIME_CHANGED);
}

static void initialize_glib_type_system(void)
{
	GType type;

	type = G_TYPE_DBUS_ACTION_GROUP;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_ANNOTATION_INFO;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_ARG_INFO;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_AUTH_OBSERVER;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_CALL_FLAGS;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_CAPABILITY_FLAGS;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_CONNECTION;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_CONNECTION_FLAGS;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_ERROR;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_INTERFACE;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_INTERFACE_INFO;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_INTERFACE_SKELETON;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_INTERFACE_SKELETON_FLAGS;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_MENU_MODEL;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_MESSAGE;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_MESSAGE_BYTE_ORDER;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_MESSAGE_FLAGS;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_MESSAGE_HEADER_FIELD;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_MESSAGE_TYPE;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_METHOD_INFO;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_METHOD_INVOCATION;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_NODE_INFO;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_OBJECT;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_OBJECT_MANAGER;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_OBJECT_MANAGER_CLIENT;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_OBJECT_MANAGER_CLIENT_FLAGS;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_OBJECT_MANAGER_SERVER;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_OBJECT_PROXY;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_OBJECT_SKELETON;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_PROPERTY_INFO;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_PROPERTY_INFO_FLAGS;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_PROXY;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_PROXY_FLAGS;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_SEND_MESSAGE_FLAGS;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_SERVER;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_SERVER_FLAGS;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_SIGNAL_FLAGS;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_SIGNAL_INFO;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = G_TYPE_DBUS_SUBTREE_FLAGS;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = JSON_TYPE_NODE;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = JSON_TYPE_OBJECT;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = JSON_TYPE_ARRAY;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = JSON_TYPE_ARRAY;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = JSON_TYPE_SERIALIZABLE;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = JSON_TYPE_PARSER;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
	type = JSON_TYPE_GENERATOR;
	if (type != G_TYPE_OBJECT) {
		DbgPrint("initialized\n");
	}
}

static bool app_create(void *argv)
{
	int ret;

	elm_app_base_scale_set(WVGA_DEFAULT_SCALE);

	widget_conf_init();
	if (!widget_conf_is_loaded()) {
		ret = widget_conf_load();
		if (ret < 0) {
			DbgPrint("Configureation manager is initiated: %d\n", ret);
		}
	}

	critical_log_init(widget_util_basename(((char **)argv)[0]));

	/*!
	 * Touch the glib type system
	 */
	initialize_glib_type_system();

	DbgPrint("Scale factor: %lf\n", elm_config_scale_get());

	if (WIDGET_CONF_COM_CORE_THREAD) {
		if (setenv("PROVIDER_COM_CORE_THREAD", "true", 0) < 0) {
			ErrPrint("setenv: %d\n", errno);
		}
	} else {
		if (setenv("PROVIDER_COM_CORE_THREAD", "false", 0) < 0){
			ErrPrint("setenv: %d\n", errno);
		}
	}

	ret = widget_service_init();
	if (ret < 0) {
		DbgPrint("Livebox service init: %d\n", ret);
	}

	/**
	 * @note
	 * Slave is not able to initiate system, before
	 * receive its name from the master
	 *
	 * So create callback doesn't do anything.
	 */
	ret = fault_init(argv);
	if (ret < 0) {
		DbgPrint("Crash recover is initiated: %d\n", ret);
	}

	ret = update_monitor_init();
	if (ret < 0) {
		DbgPrint("Content update monitor is initiated: %d\n", ret);
	}

	ret = vconf_notify_key_changed(VCONFKEY_SYSTEM_TIME_CHANGED, time_changed_cb, NULL);
	if (ret < 0) {
		DbgPrint("System time changed event callback added: %d\n", ret);
	}

	ret = vconf_notify_key_changed(VCONFKEY_SYSMAN_MMC_DEVICE_CHANGED, mmc_changed_cb, NULL);
	if (ret < 0) {
		DbgPrint("MMC status changed event callback added: %d\n", ret);
	}

	ret = vconf_notify_key_changed(VCONFKEY_SETAPPL_ACCESSIBILITY_TTS, tts_changed_cb, NULL);
	if (ret < 0) {
		DbgPrint("TTS changed callback is added: %s\n", ret);
	}

#if defined(ENABLE_EFL_ASSIST)
	s_info.table = ea_theme_font_table_new("/usr/share/themes/FontInfoTable.xml");
	if (s_info.table) {
		DbgPrint("FONT TABLE Prepared");
		ea_theme_fonts_set(s_info.table);
	}
	ea_theme_event_callback_add(EA_THEME_CALLBACK_TYPE_FONT, font_changed_cb, NULL);

	font_changed_cb(NULL);
	theme_loader_load(THEME_DIR);
#endif

	widget_viewer_init();

	return TRUE;
}

static void app_terminate(void *data)
{
	int ret;

	DbgPrint("Terminating provider\n");

	widget_viewer_fini();

#if defined(ENABLE_EFL_ASSIST)
	theme_loader_unload();

	if (s_info.table) {
		DbgPrint("FONT TABLE Destroyed");
		ea_theme_font_table_free(s_info.table);
		s_info.table = NULL;
	}

	ea_theme_event_callback_del(EA_THEME_CALLBACK_TYPE_FONT, font_changed_cb);
#endif

	ret = vconf_ignore_key_changed(VCONFKEY_SETAPPL_ACCESSIBILITY_TTS, tts_changed_cb);
	if (ret < 0) {
		DbgPrint("TTS changed callback is added: %s\n", ret);
	}

	ret = vconf_ignore_key_changed(VCONFKEY_SYSTEM_TIME_CHANGED, time_changed_cb);
	if (ret < 0) {
		DbgPrint("Remove time changed callback: %d\n", ret);
	}

	ret = vconf_ignore_key_changed(VCONFKEY_SYSMAN_MMC_DEVICE_CHANGED, mmc_changed_cb);
	if (ret < 0) {
		DbgPrint("Remove MMC status changed callback: %d\n", ret);
	}

	ret = update_monitor_fini();
	if (ret < 0) {
		DbgPrint("Content update monitor is finalized: %d\n", ret);
	}

	ret = fault_fini();
	if (ret < 0) {
		DbgPrint("Crash recover is finalized: %d\n", ret);
	}

	ret = client_fini();
	if (ret < 0) {
		DbgPrint("client finalized: %d\n", ret); 
	}

	ret = widget_service_fini();
	if (ret < 0) {
		DbgPrint("widget service fini: %d\n", ret);
	}

	critical_log_fini();

	exit(0);
	return;
}

static void app_region_changed(app_event_info_h event_info, void *data)
{
	// char *region;
	// app_event_get_region_format(event_info, &region);
	widget_system_event_all(WIDGET_SYS_EVENT_REGION_CHANGED);
}

static void app_language_changed(app_event_info_h event_info, void *data)
{
	// char *lang;
	// app_event_get_language(event_info, &lang);
	widget_system_event_all(WIDGET_SYS_EVENT_LANG_CHANGED);
}

static void app_control(app_control_h service, void *data)
{
	int ret;
	char *name;
	char *secured;
	char *hw_acceleration = NULL;
	char *abi;
	static int initialized = 0;

	if (initialized) {
		ErrPrint("Already initialized\n");
		return;
	}

	ret = app_control_get_extra_data(service, WIDGET_CONF_BUNDLE_SLAVE_NAME, &name);
	if (ret != APP_CONTROL_ERROR_NONE) {
		ErrPrint("Name is not valid\n");
		return;
	}

	ret = app_control_get_extra_data(service, WIDGET_CONF_BUNDLE_SLAVE_SECURED, &secured);
	if (ret != APP_CONTROL_ERROR_NONE) {
		free(name);
		ErrPrint("Secured is not valid\n");
		return;
	}

	ret = app_control_get_extra_data(service, WIDGET_CONF_BUNDLE_SLAVE_HW_ACCELERATION, &hw_acceleration);
	if (ret != APP_CONTROL_ERROR_NONE) {
		DbgPrint("Unable to get option for hw-acceleration\n");
		hw_acceleration = strdup("use-x11");
		/* Go ahead */
	}

	ret = app_control_get_extra_data(service, WIDGET_CONF_BUNDLE_SLAVE_ABI, &abi);
	if (ret != APP_CONTROL_ERROR_NONE) {
		DbgPrint("Unable to get option for hw-acceleration\n");
		abi = strdup(WIDGET_CONF_DEFAULT_ABI);
		/* Go ahead */
	}

	if (!strcasecmp(secured, "true")) {
		/* Don't use the update timer */
		widget_turn_secured_on();
	}

	DbgPrint("Name assigned: %s\n", name);
	DbgPrint("Secured: %s\n", secured);
	DbgPrint("hw-acceleration: %s\n", hw_acceleration);
	DbgPrint("abi: %s\n", abi);
	ret = client_init(name, abi, hw_acceleration, widget_is_secured());

	if (hw_acceleration && !strcasecmp(hw_acceleration, "use-gl")) {
		DbgPrint("Turn on: 3d acceleration\n");
		elm_config_accel_preference_set("3d");
	}

	free(name);
	free(secured);
	free(hw_acceleration);
	free(abi);

	initialized = 1;
	return;
}

#if defined(_ENABLE_MCHECK)
static inline void mcheck_cb(enum mcheck_status status)
{
	char *ptr;

	ptr = util_get_current_module(NULL);

	switch (status) {
	case MCHECK_DISABLED:
		ErrPrint("[DISABLED] Heap incosistency detected: %s\n", ptr);
		break;
	case MCHECK_OK:
		ErrPrint("[OK] Heap incosistency detected: %s\n", ptr);
		break;
	case MCHECK_HEAD:
		ErrPrint("[HEAD] Heap incosistency detected: %s\n", ptr);
		break;
	case MCHECK_TAIL:
		ErrPrint("[TAIL] Heap incosistency detected: %s\n", ptr);
		break;
	case MCHECK_FREE:
		ErrPrint("[FREE] Heap incosistency detected: %s\n", ptr);
		break;
	default:
		break;
	}
}
#endif

#define HEAP_MONITOR_PATH "/usr/lib/libheap-monitor.so"
#define BIN_PATH "/usr/apps/org.tizen.data-provider-slave/bin/"
int main(int argc, char *argv[])
{
	int ret;
	service_app_lifecycle_callback_s event_callback;

	const char *option;

	memset(argv[0], 0, strlen(argv[0]));
	strcpy(argv[0], BIN_PATH "data-provider-slave.svc");
	DbgPrint("Replace argv[0] with %s\n", argv[0]);

#if defined(_ENABLE_MCHECK)
	mcheck(mcheck_cb);
#endif
	option = getenv("PROVIDER_DISABLE_CALL_OPTION");
	if (option && !strcasecmp(option, "true")) {
		fault_disable_call_option();
	}

	option = getenv("PROVIDER_HEAP_MONITOR_START");
	if (option && !strcasecmp(option, "true")) {
		s_info.heap_monitor = dlopen(HEAP_MONITOR_PATH, RTLD_NOW);
		if (s_info.heap_monitor) {
			s_info.heap_monitor_initialized = dlsym(s_info.heap_monitor, "heap_monitor_initialized");
			s_info.heap_monitor_target_usage = dlsym(s_info.heap_monitor, "heap_monitor_target_usage");
			s_info.heap_monitor_add_target = dlsym(s_info.heap_monitor, "heap_monitor_add_target");
			s_info.heap_monitor_del_target = dlsym(s_info.heap_monitor, "heap_monitor_del_target");
		}
	}

	if (setenv("BUFMGR_LOCK_TYPE", "once", 0) < 0) {
		ErrPrint("setenv: %d\n", errno);
	}

	if (setenv("BUFMGR_MAP_CACHE", "true", 0) < 0) {
		ErrPrint("setenv: %d\n", errno);
	}

	event_callback.create = app_create;
	event_callback.terminate = app_terminate;
	event_callback.app_control = app_control;

	service_app_add_event_handler(&s_info.lang_changed_handler, APP_EVENT_LANGUAGE_CHANGED, app_language_changed, argv);
	service_app_add_event_handler(&s_info.region_changed_handler, APP_EVENT_REGION_FORMAT_CHANGED, app_region_changed, argv);
	// APP_EVENT_DEVICE_ORIENTATION_CHANGED
	// APP_EVENT_LOW_MEMORY
	// APP_EVENT_LOW_BATTERY

	ret = service_app_main(argc, argv, &event_callback, (void *)argv);
	ErrPrint("service_app_main: %d\n", ret);

	if (s_info.heap_monitor) {
		if (dlclose(s_info.heap_monitor) < 0) {
			ErrPrint("dlclose: %d\n", errno);
		}
	}

	return ret;
}

HAPI int main_heap_monitor_is_enabled(void)
{
	return s_info.heap_monitor_initialized ? s_info.heap_monitor_initialized() : 0;
}

HAPI size_t main_heap_monitor_target_usage(const char *name)
{
	return s_info.heap_monitor_target_usage ? s_info.heap_monitor_target_usage(name) : 0;
}

HAPI int main_heap_monitor_add_target(const char *name)
{
	return s_info.heap_monitor_add_target ? s_info.heap_monitor_add_target(name) : 0;
}

HAPI int main_heap_monitor_del_target(const char *name)
{
	return s_info.heap_monitor_del_target ? s_info.heap_monitor_del_target(name) : 0;
}

HAPI void main_app_exit(void)
{
	service_app_exit();
}
/* End of a file */
