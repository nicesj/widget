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

#if defined(LOG_TAG)
#undef LOG_TAG
#define LOG_TAG "ICON_PROVIDER"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Elementary.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <Ecore.h>
#include <Ecore_X.h>
#include <app.h>
#include <Edje.h>
#include <Eina.h>

#include <vconf.h>
#include <dlog.h>
#include <bundle.h>
#include <widget_service/widget_service.h>
#include <widget_errno.h>
#include <widget_provider.h>

#include <system_settings.h>

#include <packet.h>
#include <com-core.h>
#include <com-core_packet.h>

#include <shortcut.h>
#include <shortcut_private.h>

#include "util.h"
#include "debug.h"

#define UTILITY_ADDR	"/tmp/.utility.service"
#if !defined(DEFAULT_ICON_LAYOUT)
#define DEFAULT_ICON_LAYOUT "/usr/apps/org.tizen.data-provider-slave/res/edje/icon.edj"
#endif

#if !defined(DEFAULT_ICON_GROUP)
#define DEFAULT_ICON_GROUP "default"
#endif

#define TEXT_CLASS	"tizen"
#define DEFAULT_FONT_SIZE	-100

int script_handler_parse_desc(Evas_Object *edje, const char *descfile);

static struct info {
	int client_fd;
	const char *socket_file;
	char *font_name;
} s_info = {
	.client_fd = -1,
	.socket_file = UTILITY_ADDR,
	.font_name = NULL,
};

#define TTL	30.0f	/* Can alive only 30 seconds from the last event */
#define QUALITY_N_COMPRESS "quality=100 compress=1"

/*!
 * Defined for libwidget
 */
const char *widget_find_pkgname(const char *filename)
{
	return NULL;
}

int widget_request_update_by_id(const char *filename)
{
	return WIDGET_ERROR_NOT_EXIST;
}

int widget_trigger_update_monitor(const char *id, int is_pd)
{
	return WIDGET_ERROR_INVALID_PARAMETER;
}

static inline Evas *create_virtual_canvas(int w, int h)
{
	Ecore_Evas *internal_ee;
	Evas *internal_e;

	// Create virtual canvas
	internal_ee = ecore_evas_buffer_new(w, h);
	if (!internal_ee) {
		ErrPrint("Failed to create a new canvas buffer\n");
		return NULL;
	}

	ecore_evas_alpha_set(internal_ee, EINA_TRUE);
	ecore_evas_manual_render_set(internal_ee, EINA_TRUE);

	// Get the "Evas" object from a virtual canvas
	internal_e = ecore_evas_get(internal_ee);
	if (!internal_e) {
		ecore_evas_free(internal_ee);
		ErrPrint("Faield to get Evas object\n");
		return NULL;
	}

	ecore_evas_resize(internal_ee, w, h);
	ecore_evas_show(internal_ee);

	return internal_e;
}

static inline int flush_data_to_file(Evas *e, char *data, const char *filename, int w, int h)
{
	Evas_Object *output;

	output = evas_object_image_add(e);
	if (!output) {
		ErrPrint("Failed to create an image object\n");
		return EXIT_FAILURE;
	}

	evas_object_image_data_set(output, NULL);
	evas_object_image_colorspace_set(output, EVAS_COLORSPACE_ARGB8888);
	evas_object_image_alpha_set(output, EINA_TRUE);
	evas_object_image_size_set(output, w, h);
	evas_object_image_smooth_scale_set(output, EINA_TRUE);
	evas_object_image_data_set(output, data);
	evas_object_image_data_update_add(output, 0, 0, w, h);

	if (evas_object_image_save(output, filename, NULL, QUALITY_N_COMPRESS) == EINA_FALSE) {
		evas_object_del(output);
		ErrPrint("Faield to save a captured image (%s)\n", filename);
		return EXIT_FAILURE;
	}

	evas_object_del(output);

	if (access(filename, F_OK) != 0) {
		ErrPrint("File %s is not found\n", filename);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static inline int flush_to_file(Evas *e, const char *filename, int w, int h)
{
	void *data;
	Ecore_Evas *internal_ee;

	internal_ee = ecore_evas_ecore_evas_get(e);
	if (!internal_ee) {
		ErrPrint("Failed to get ecore evas\n");
		return EXIT_FAILURE;
	}

	ecore_evas_manual_render(internal_ee);

	// Get a pointer of a buffer of the virtual canvas
	data = (void *)ecore_evas_buffer_pixels_get(internal_ee);
	if (!data) {
		ErrPrint("Failed to get pixel data\n");
		return EXIT_FAILURE;
	}

	return flush_data_to_file(e, data, filename, w, h);
}

static inline int destroy_virtual_canvas(Evas *e)
{
	Ecore_Evas *ee;

	ee = ecore_evas_ecore_evas_get(e);
	if (!ee) {
		ErrPrint("Failed to ecore evas object\n");
		return EXIT_FAILURE;
	}

	ecore_evas_free(ee);
	return EXIT_SUCCESS;
}

static int disconnected_cb(int handle, void *data)
{
	s_info.client_fd = -1;
	elm_exit();
	return 0;
}

static inline int convert_shortcut_type_to_lb_type(int shortcut_type, char **str)
{
	char *_str;

	if (!str) {
		str = &_str;
	}

	switch (shortcut_type) {
	case WIDGET_SIZE_TYPE_1x1:
		*str = "1x1";
		return WIDGET_SIZE_TYPE_1x1;
	case WIDGET_SIZE_TYPE_2x1:
		*str = "2x1";
		return WIDGET_SIZE_TYPE_2x1;
	case WIDGET_SIZE_TYPE_2x2:
		*str = "2x2";
		return WIDGET_SIZE_TYPE_2x2;
	case WIDGET_SIZE_TYPE_4x1:
		*str = "4x1";
		return WIDGET_SIZE_TYPE_4x1;
	case WIDGET_SIZE_TYPE_4x2:
		*str = "4x2";
		return WIDGET_SIZE_TYPE_4x2;
	case WIDGET_SIZE_TYPE_4x3:
		*str = "4x3";
		return WIDGET_SIZE_TYPE_4x3;
	case WIDGET_SIZE_TYPE_4x4:
		*str = "4x4";
		return WIDGET_SIZE_TYPE_4x4;
	case WIDGET_SIZE_TYPE_4x5:
		*str = "4x5";
		return WIDGET_SIZE_TYPE_4x5;
	case WIDGET_SIZE_TYPE_4x6:
		*str = "4x6";
		return WIDGET_SIZE_TYPE_4x6;
	case WIDGET_SIZE_TYPE_EASY_1x1:
		*str = "easy,1x1";
		return WIDGET_SIZE_TYPE_EASY_1x1;
	case WIDGET_SIZE_TYPE_EASY_3x1:
		*str = "easy,3x1";
		return WIDGET_SIZE_TYPE_EASY_3x1;
	case WIDGET_SIZE_TYPE_EASY_3x3:
		*str = "easy,3x3";
		return WIDGET_SIZE_TYPE_EASY_3x3;
	default:
		*str = "?x?";
		return WIDGET_SIZE_TYPE_UNKNOWN;
	}
}

static struct packet *icon_create(pid_t pid, int handle, const struct packet *packet)
{
	Evas *e;
	const char *edje_path;
	const char *group;
	const char *desc_file;
	const char *output;
	int size_type;
	int ret;
	int w;
	int h;
	Evas_Object *edje;
	Evas_Object *parent;
	char _group[16];
	char *size_str;

	ret = packet_get(packet, "sssis", &edje_path, &group, &desc_file, &size_type, &output);
	if (ret != 5) {
		ErrPrint("Invalid parameters");
		ret = -EINVAL;
		goto out;
	}

	if (!edje_path || !strlen(edje_path)) {
		edje_path = DEFAULT_ICON_LAYOUT;
	}

	size_type = convert_shortcut_type_to_lb_type(size_type, &size_str);
	if (!group || !strlen(group)) {
		snprintf(_group, sizeof(_group), DEFAULT_ICON_GROUP",%s", size_str);
		group = _group;
	}
	DbgPrint("Selected layout: %s(%s)\n", edje_path, group);

	ret = widget_service_get_size(size_type, &w, &h);
	if (ret != WIDGET_ERROR_NONE) {
		ErrPrint("Unable to get size(%d): %d\n", size_type, ret);
		goto out;
	}

	e = create_virtual_canvas(w, h);
	if (!e) {
		ErrPrint("Unable to create a canvas: %dx%d\n", w, h);
		ret = WIDGET_ERROR_FAULT;
		goto out;
	}

	parent = evas_object_rectangle_add(e);
	if (!parent) {
		ErrPrint("Unable to create a parent\n");
		destroy_virtual_canvas(e);
		ret = WIDGET_ERROR_FAULT;
		goto out;
	}

	evas_object_resize(parent, w, h);
	evas_object_color_set(parent, 0, 0, 0, 0);
	evas_object_show(parent);

	edje = elm_layout_add(parent);
	if (!edje) {
		ErrPrint("Unable to add an edje object\n");
		evas_object_del(parent);
		destroy_virtual_canvas(e);
		goto out;
	}

	if (elm_layout_file_set(edje, edje_path, group) == EINA_FALSE) {
		Edje_Load_Error err;
		err = edje_object_load_error_get(elm_layout_edje_get(edje));
		if (err != EDJE_LOAD_ERROR_NONE) {
			ErrPrint("Uanble to load an edje %s(%s) - %s\n", edje_path, group, edje_load_error_str(err));
		}
		evas_object_del(edje);
		evas_object_del(parent);
		destroy_virtual_canvas(e);
		goto out;
	}

	evas_object_resize(edje, w, h);
	evas_object_show(edje);

	if (script_handler_parse_desc(edje, desc_file) != WIDGET_ERROR_NONE) {
		ErrPrint("Unable to parse the %s\n", desc_file);
	}

	flush_to_file(e, output, w, h);
	evas_object_del(edje);
	evas_object_del(parent);
	destroy_virtual_canvas(e);

out:
	if (ret < 0) {
		/* Desc file should be deleted if it fails to create an icon image */
		if (unlink(desc_file) < 0) {
			ErrPrint("unlink(%s): %d\n", desc_file, errno);
		}
	}

	return packet_create_reply(packet, "i", ret);
}

static inline int client_init(void)
{
	int ret;
	struct packet *packet;
	static struct method service_table[] = {
		{
			.cmd = "icon_create",
			.handler = icon_create,
		},
		{
			.cmd = NULL,
			.handler = NULL,
		},
	};

	com_core_add_event_callback(CONNECTOR_DISCONNECTED, disconnected_cb, NULL);

	s_info.client_fd = com_core_packet_client_init(s_info.socket_file, 0, service_table);
	if (s_info.client_fd < 0) {
		ErrPrint("Failed to make a connection to the master\n");
		return -EFAULT;
	}

	packet = packet_create_noack("service_register", "");
	if (!packet) {
		ErrPrint("Failed to build a packet\n");
		return -EFAULT;
	}

	ret = com_core_packet_send_only(s_info.client_fd, packet);
	DbgPrint("Service register sent: %d\n", ret);
	packet_destroy(packet);
	if (ret != 0) {
		com_core_packet_client_fini(s_info.client_fd);
		s_info.client_fd = -1;
		ret = -EFAULT;
	} else {
		ret = 0;
	}

	DbgPrint("Server FD: %d\n", s_info.client_fd);
	return ret;
}

static inline void client_fini(void)
{
	if (s_info.client_fd < 0) {
		DbgPrint("Client is not initiated\n");
		return;
	}

	com_core_packet_client_fini(s_info.client_fd);
	s_info.client_fd = -1;
}

static void update_font_cb(void *data)
{
	Eina_List *list;
	char *text;

	list = edje_text_class_list();
	DbgPrint("List: %p\n", list);
	if (list) {
		EINA_LIST_FREE(list, text) {
			if (!strncasecmp(text, TEXT_CLASS, strlen(TEXT_CLASS))) {
				DbgPrint("Update text class %s (%s, %d)\n", text, s_info.font_name, DEFAULT_FONT_SIZE);
				edje_text_class_del(text);
				edje_text_class_set(text, s_info.font_name, DEFAULT_FONT_SIZE);
			} else {
				DbgPrint("Skip text class %s\n", text);
			}
		}
	} else {
		DbgPrint("New (%s, %d)\n", s_info.font_name, DEFAULT_FONT_SIZE);
		edje_text_class_set(TEXT_CLASS, s_info.font_name, DEFAULT_FONT_SIZE);
	}
}

static void font_changed_cb(keynode_t *node, void *user_data)
{
	char *font_name;

	evas_font_reinit();

	if (s_info.font_name) {
		font_name = vconf_get_str("db/setting/accessibility/font_name");
		if (!font_name) {
			ErrPrint("Invalid font name (NULL)\n");
			return;
		}

		if (!strcmp(s_info.font_name, font_name)) {
			DbgPrint("Font is not changed (Old: %s(%p) <> New: %s(%p))\n", s_info.font_name, s_info.font_name, font_name, font_name);
			free(font_name);
			return;
		}

		DbgPrint("Release old font name: %s(%p)\n", s_info.font_name, s_info.font_name);
		free(s_info.font_name);
	} else {
		int ret;

		font_name = NULL;
		ret = system_settings_get_value_string(SYSTEM_SETTINGS_KEY_FONT_TYPE, &font_name);
		if (ret != SYSTEM_SETTINGS_ERROR_NONE || !font_name) {
			ErrPrint("system settings: %d, font_name[%p]\n", ret, font_name);
			return;
		}
	}

	s_info.font_name = font_name;
	DbgPrint("Font name is changed to %s(%p)\n", s_info.font_name, s_info.font_name);

	update_font_cb(NULL);
}

static bool app_create(void *data)
{
	int ret;

	if (client_init() < 0) {
		ErrPrint("Unable to initiate the client\n");
		return FALSE;
	}

	/*!
	 * Send a request to reigister as a service.
	 */
	ret = vconf_notify_key_changed("db/setting/accessibility/font_name", font_changed_cb, NULL);
	if (ret < 0) {
		DbgPrint("System font is changed: %d\n", ret);
	}

	font_changed_cb(NULL, NULL);
	return TRUE;
}

static void app_terminate(void *data)
{
	int ret;

	ret = vconf_ignore_key_changed("db/setting/accessibility/font_name", font_changed_cb);
	if (ret < 0) {
		DbgPrint("Remove font change callback: %d\n", ret);
	}

	client_fini();

	free(s_info.font_name);
	s_info.font_name = NULL;
	return;
}

static void app_pause(void *data)
{
	/* Will not be called */
	return;
}

static void app_resume(void *data)
{
	/* Will not be called */
	return;
}

static void app_control(app_control_h service, void *data)
{
}

int main(int argc, char *argv[])
{
	int ret;
	ui_app_lifecycle_callback_s event_callback;

	event_callback.create = app_create;
	event_callback.terminate = app_terminate;
	event_callback.pause = app_pause;
	event_callback.resume = app_resume;
	event_callback.app_control = app_control;

	ret = ui_app_main(argc, argv, &event_callback, NULL);
	return ret;
}

/* End of a file */
