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
#define _GNU_SOURCE

#include <stdio.h>
#include <libgen.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <dlfcn.h>

#include <Elementary.h>
#include <Evas.h>
#include <Edje.h>
#include <Eina.h>
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Eet.h>
#if defined(ENABLE_EFL_ASSIST)
#include <efl_assist.h>
#endif

#include <system_settings.h>

#include <dlog.h>
#include <debug.h>
#include <vconf.h>
#include <widget_errno.h>
#include <widget_service.h>
#include <widget_service_internal.h>
#include <widget_script.h>

#include "script_port.h"
#include "abi.h"

#define TEXT_CLASS	"tizen"
#define DEFAULT_FONT_SIZE	-100

#define PUBLIC __attribute__((visibility("default")))

#define ACCESS_TYPE_DOWN 0
#define ACCESS_TYPE_MOVE 1
#define ACCESS_TYPE_UP 2
#define ACCESS_TYPE_CUR 0
#define ACCESS_TYPE_NEXT 1
#define ACCESS_TYPE_PREV 2
#define ACCESS_TYPE_OFF 3

struct image_option {
	int orient;
	int aspect;
	enum {
		FILL_DISABLE,
		FILL_IN_SIZE,
		FILL_OVER_SIZE,
		FILL_FIT_SIZE
	} fill;

	struct shadow {
		int enabled;
		int angle;
		int offset;
		int softness;
		int color;
	} shadow;

	int width;
	int height;
};

struct info {
	char *file;
	char *group;
	char *category;

	int is_mouse_down;
	void *buffer_handle;

	Ecore_Evas *ee;
	Evas *e;

	Evas_Object *parent;

	Eina_List *obj_list;

	int (*render_pre)(void *buffer_handle, void *data);
	int (*render_post)(void *render_handle, void *data);
	void *render_data;
};

struct child {
	Evas_Object *obj;
	char *part;
};

struct obj_info {
	char *id;
	Eina_List *children;
	Evas_Object *parent;
	int delete_me;
};

static struct {
	char *font_name;
	int font_size;
	int access_on;

	Eina_List *handle_list;
	int premultiplied;
	Ecore_Evas *(*alloc_canvas)(int w, int h, void *(*a)(void *data, int size), void (*f)(void *data, void *ptr), void *data);
	Ecore_Evas *(*alloc_canvas_with_stride)(int w, int h, void *(*a)(void *data, int size, int *stride, int *bpp), void (*f)(void *data, void *ptr), void *data);
} s_info = {
	.font_name = NULL,
	.font_size = -100,

	.handle_list = NULL,
	.access_on = 0,
	.premultiplied = 1,
	.alloc_canvas = NULL,
	.alloc_canvas_with_stride = NULL,
};

static inline Evas_Object *find_edje(struct info *handle, const char *id)
{
	Eina_List *l;
	Evas_Object *edje;
	struct obj_info *obj_info;

	EINA_LIST_FOREACH(handle->obj_list, l, edje) {
		obj_info = evas_object_data_get(edje, "obj_info");
		if (!obj_info) {
			ErrPrint("Object info is not valid\n");
			continue;
		}

		if (!id) {
			if (!obj_info->id) {
				return edje;
			}

			continue;
		} else if (!obj_info->id) {
			continue;
		}

		if (!strcmp(obj_info->id, id)) {
			return edje;
		}
	}

	DbgPrint("EDJE[%s] is not found\n", id);
	return NULL;
}

PUBLIC const char *script_magic_id(void)
{
	return "edje";
}

PUBLIC int script_update_color(void *h, const char *id, const char *part, const char *rgba)
{
	struct info *handle = h;
	Evas_Object *edje;
	int r[3], g[3], b[3], a[3];
	int ret;

	edje = find_edje(handle, id);
	if (!edje) {
		return WIDGET_ERROR_NOT_EXIST;
	}

	ret = sscanf(rgba, "%d %d %d %d %d %d %d %d %d %d %d %d",
			r, g, b, a,			/* OBJECT */
			r + 1, g + 1, b + 1, a + 1,	/* OUTLINE */
			r + 2, g + 2, b + 2, a + 2);	/* SHADOW */
	if (ret != 12) {
		DbgPrint("id[%s] part[%s] rgba[%s]\n", id, part, rgba);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	ret = edje_object_color_class_set(elm_layout_edje_get(edje), part,
			r[0], g[0], b[0], a[0], /* OBJECT */
			r[1], g[1], b[1], a[1], /* OUTLINE */
			r[2], g[2], b[2], a[2]); /* SHADOW */

	DbgPrint("EDJE[%s] color class is %s changed", id, ret == EINA_TRUE ? "successfully" : "not");
	return WIDGET_ERROR_NONE;
}

static void activate_cb(void *data, Evas_Object *part_obj, Elm_Object_Item *item)
{
	Evas *e;
	int x;
	int y;
	int w;
	int h;
	double timestamp;

	e = evas_object_evas_get(part_obj);
	evas_object_geometry_get(part_obj, &x, &y, &w, &h);
	x += w / 2;
	y += h / 2;

#if defined(_USE_ECORE_TIME_GET)
	timestamp = ecore_time_get();
#else
	struct timeval tv;
	if (gettimeofday(&tv, NULL) < 0) {
		ErrPrint("Failed to get time\n");
		timestamp = 0.0f;
	} else {
		timestamp = (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0f);
	}
#endif

	DbgPrint("Cursor is on %dx%d\n", x, y);
	evas_event_feed_mouse_move(e, x, y, timestamp * 1000, NULL);
	evas_event_feed_mouse_down(e, 1, EVAS_BUTTON_NONE, (timestamp + 0.01f) * 1000, NULL);
	evas_event_feed_mouse_move(e, x, y, (timestamp + 0.02f) * 1000, NULL);
	evas_event_feed_mouse_up(e, 1, EVAS_BUTTON_NONE, (timestamp + 0.03f) * 1000, NULL);
}

static void update_focus_chain(struct info *handle, Evas_Object *ao)
{
	const Eina_List *list;

	list = elm_object_focus_custom_chain_get(handle->parent);
	if (!eina_list_data_find(list, ao)) {
		DbgPrint("Append again to the focus chain\n");
		elm_object_focus_custom_chain_append(handle->parent, ao, NULL);
	}
}

PUBLIC int script_update_text(void *h, const char *id, const char *part, const char *text)
{
	struct obj_info *obj_info;
	struct info *handle = h;
	Evas_Object *edje;
	Evas_Object *edje_part;

	edje = find_edje(handle, id);
	if (!edje) {
		ErrPrint("Failed to find EDJE\n");
		return WIDGET_ERROR_NOT_EXIST;
	}

	obj_info = evas_object_data_get(edje, "obj_info");
	if (!obj_info) {
		ErrPrint("Object info is not available\n");
		return WIDGET_ERROR_FAULT;
	}

	elm_object_part_text_set(edje, part, text ? text : "");

	edje_part = (Evas_Object *)edje_object_part_object_get(elm_layout_edje_get(edje), part);
	if (edje_part) {
		Evas_Object *ao;
		char *utf8;

		ao = evas_object_data_get(edje_part, "ao");
		if (!ao) {
			ao = elm_access_object_register(edje_part, handle->parent);
			if (!ao) {
				ErrPrint("Unable to register an access object(%s)\n", part);
				goto out;
			}

			evas_object_data_set(edje_part, "ao", ao);
			elm_access_activate_cb_set(ao, activate_cb, NULL);
			elm_object_focus_custom_chain_append(handle->parent, ao, NULL);

			DbgPrint("[%s] Register access info: (%s) to, %p\n", part, text, handle->parent);
		}

		if (!text || !strlen(text)) {
			/*!
			 * \note
			 * Delete callback will be called
			 */
			DbgPrint("[%s] Remove access object(%p)\n", part, ao);
			elm_access_object_unregister(ao);

			goto out;
		}

		utf8 = elm_entry_markup_to_utf8(text);
		if ((!utf8 || !strlen(utf8))) {
			free(utf8);
			/*!
			 * \note
			 * Delete callback will be called
			 */
			DbgPrint("[%s] Remove access object(%p)\n", part, ao);
			elm_access_object_unregister(ao);

			goto out;
		}

		elm_access_info_set(ao, ELM_ACCESS_INFO, utf8);
		free(utf8);

		update_focus_chain(handle, ao);
	} else {
		ErrPrint("Unable to get text part[%s]\n", part);
	}

out:
	return WIDGET_ERROR_NONE;
}

static void parse_aspect(struct image_option *img_opt, const char *value, int len)
{
	while (len > 0 && *value == ' ') {
		value++;
		len--;
	}

	if (len < 4) {
		return;
	}

	img_opt->aspect = !strncasecmp(value, "true", 4);
	DbgPrint("Parsed ASPECT: %d (%s)\n", img_opt->aspect, value);
}

static void parse_orient(struct image_option *img_opt, const char *value, int len)
{
	while (len > 0 && *value == ' ') {
		value++;
		len--;
	}

	if (len < 4) {
		return;
	}

	img_opt->orient = !strncasecmp(value, "true", 4);
	DbgPrint("Parsed ORIENT: %d (%s)\n", img_opt->orient, value);
}

static void parse_size(struct image_option *img_opt, const char *value, int len)
{
	int width;
	int height;
	char *buf;

	while (len > 0 && *value == ' ') {
		value++;
		len--;
	}

	buf = strndup(value, len);
	if (!buf) {
		ErrPrint("Heap: %d\n", errno);
		return;
	}

	if (sscanf(buf, "%dx%d", &width, &height) == 2) {
		img_opt->width = width;
		img_opt->height = height;
		DbgPrint("Parsed size : %dx%d (%s)\n", width, height, buf);
	} else {
		DbgPrint("Invalid size tag[%s]\n", buf);
	}

	free(buf);
}

static void parse_shadow(struct image_option *img_opt, const char *value, int len)
{
	int angle;
	int offset;
	int softness;
	int color;

	if (sscanf(value, "%d,%d,%d,%x", &angle, &offset, &softness, &color) != 4) {
		ErrPrint("Invalid shadow [%s]\n", value);
	} else {
		img_opt->shadow.enabled = 1;
		img_opt->shadow.angle = angle;
		img_opt->shadow.offset = offset;
		img_opt->shadow.softness = softness;
		img_opt->shadow.color = color;
	}
}

static void parse_fill(struct image_option *img_opt, const char *value, int len)
{
	while (len > 0 && *value == ' ') {
		value++;
		len--;
	}

	if (!strncasecmp(value, "in-size", len)) {
		img_opt->fill = FILL_IN_SIZE;
	} else if (!strncasecmp(value, "over-size", len)) {
		img_opt->fill = FILL_OVER_SIZE;
	} else if (!strncasecmp(value, "fit-size", len)) {
		img_opt->fill = FILL_FIT_SIZE;
	} else {
		img_opt->fill = FILL_DISABLE;
	}

	DbgPrint("Parsed FILL: %d (%s)\n", img_opt->fill, value);
}

static inline void parse_image_option(const char *option, struct image_option *img_opt)
{
	const char *ptr;
	const char *cmd;
	const char *value;
	struct {
		const char *cmd;
		void (*handler)(struct image_option *img_opt, const char *value, int len);
	} cmd_list[] = {
		{
			.cmd = "aspect", /* Keep the aspect ratio */
			.handler = parse_aspect,
		},
		{
			.cmd = "orient", /* Keep the orientation value: for the rotated images */
			.handler = parse_orient,
		},
		{
			.cmd = "fill", /* Fill the image to its container */
			.handler = parse_fill, /* Value: in-size, over-size, disable(default) */
		},
		{
			.cmd = "size",
			.handler = parse_size,
		},
		{
			.cmd = "shadow",
			.handler = parse_shadow,
		},
	};
	enum {
		STATE_START,
		STATE_TOKEN,
		STATE_DATA,
		STATE_IGNORE,
		STATE_ERROR,
		STATE_END
	} state;
	int idx;
	int tag;

	if (!option || !*option) {
		return;
	}

	state = STATE_START;
	/*!
	 * \note
	 * GCC 4.7 warnings uninitialized idx and tag value.
	 * But it will be initialized by the state machine. :(
	 * Anyway, I just reset idx and tag for reducing the GCC4.7 complains.
	 */
	idx = 0;
	tag = 0;
	cmd = NULL;
	value = NULL;

	for (ptr = option; state != STATE_END; ptr++) {
		switch (state) {
		case STATE_START:
			if (*ptr == '\0') {
				state = STATE_END;
				continue;
			}

			if (isalpha(*ptr)) {
				state = STATE_TOKEN;
				ptr--;
			}
			tag = 0;
			idx = 0;

			cmd = cmd_list[tag].cmd;
			break;
		case STATE_IGNORE:
			if (*ptr == '=') {
				state = STATE_DATA;
				value = ptr;
			} else if (*ptr == '\0') {
				state = STATE_END;
			}
			break;
		case STATE_TOKEN:
			if (cmd[idx] == '\0' && (*ptr == ' ' || *ptr == '\t' || *ptr == '=')) {
				if (*ptr == '=') {
					value = ptr;
					state = STATE_DATA;
				} else {
					state = STATE_IGNORE;
				}
				idx = 0;
			} else if (*ptr == '\0') {
				state = STATE_END;
			} else if (cmd[idx] == *ptr) {
				idx++;
			} else {
				ptr -= (idx + 1);

				tag++;
				if (tag == sizeof(cmd_list) / sizeof(cmd_list[0])) {
					tag = 0;
					state = STATE_ERROR;
				} else {
					cmd = cmd_list[tag].cmd;
				}
				idx = 0;
			}
			break;
		case STATE_DATA:
			if (*ptr == ';' || *ptr == '\0') {
				cmd_list[tag].handler(img_opt, value + 1, idx);
				state = *ptr ? STATE_START : STATE_END;
			} else {
				idx++;
			}
			break;
		case STATE_ERROR:
			if (*ptr == ';') {
				state = STATE_START;
			} else if (*ptr == '\0') {
				state = STATE_END;
			}
			break;
		default:
			break;
		}
	}
}

PUBLIC int script_update_access(void *_h, const char *id, const char *part, const char *text, const char *option)
{
	struct info *handle = _h;
	Evas_Object *edje;
	struct obj_info *obj_info;
	Evas_Object *edje_part;

	edje = find_edje(handle, id);
	if (!edje) {
		ErrPrint("No such object: %s\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	obj_info = evas_object_data_get(edje, "obj_info");
	if (!obj_info) {
		ErrPrint("Object info is not available\n");
		return WIDGET_ERROR_FAULT;
	}

	edje_part = (Evas_Object *)edje_object_part_object_get(elm_layout_edje_get(edje), part);
	if (edje_part) {
		Evas_Object *ao;

		ao = evas_object_data_get(edje_part, "ao");
		if (ao) {
			if (text && strlen(text)) {
				elm_access_info_set(ao, ELM_ACCESS_INFO, text);
				DbgPrint("Access info is updated: %s [%s], %p\n", part, text, ao);
				update_focus_chain(handle, ao);
			} else {
				/*!
				 * \note
				 * Delete clalback will be called
				 */
				DbgPrint("[%s] Remove access object(%p)\n", part, ao);
				elm_access_object_unregister(ao);
			}
		} else if (text && strlen(text)) {
			ao = elm_access_object_register(edje_part, handle->parent);
			if (!ao) {
				ErrPrint("Unable to register an access object(%s)\n", part);
			} else {
				elm_access_info_set(ao, ELM_ACCESS_INFO, text);

				evas_object_data_set(edje_part, "ao", ao);
				elm_object_focus_custom_chain_append(handle->parent, ao, NULL);
				elm_access_activate_cb_set(ao, activate_cb, NULL);
				DbgPrint("[%s] Register access info: (%s) to, %p (%p)\n", part, text, handle->parent, ao);
			}
		}
	} else {
		ErrPrint("[%s] is not exists\n", part);
	}

	return WIDGET_ERROR_NONE;
}

PUBLIC int script_operate_access(void *_h, const char *id, const char *part, const char *operation, const char *option)
{
	struct info *handle = _h;
	Evas_Object *edje;
	struct obj_info *obj_info;
	Elm_Access_Action_Info action_info;
	int ret;

	if (!operation || !strlen(operation)) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	edje = find_edje(handle, id);
	if (!edje) {
		ErrPrint("No such object: %s\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	obj_info = evas_object_data_get(edje, "obj_info");
	if (!obj_info) {
		ErrPrint("Object info is not available\n");
		return WIDGET_ERROR_FAULT;
	}

	memset(&action_info, 0, sizeof(action_info));

	/* OPERATION is defined in libwidget package */
	if (!strcasecmp(operation, "set,hl")) {
		if (part) {
			Evas_Object *edje_part;
			Evas_Coord x;
			Evas_Coord y;
			Evas_Coord w;
			Evas_Coord h;

			edje_part = (Evas_Object *)edje_object_part_object_get(elm_layout_edje_get(edje), part);
			if (!edje_part) {
				ErrPrint("Invalid part: %s\n", part);
				goto out;
			}

			evas_object_geometry_get(edje_part, &x, &y, &w, &h);

			action_info.x = x + w / 2;
			action_info.y = x + h / 2;
		} else if (option && sscanf(option, "%dx%d", &action_info.x, &action_info.y) == 2) {
		} else {
			ErrPrint("Insufficient info for HL\n");
			goto out;
		}

		DbgPrint("TXxTY: %dx%d\n", action_info.x, action_info.y);
		ret = elm_access_action(edje, ELM_ACCESS_ACTION_HIGHLIGHT, &action_info);
		if (ret == EINA_FALSE) {
			ErrPrint("Action error\n");
		}
	} else if (!strcasecmp(operation, "unset,hl")) {
		ret = elm_access_action(edje, ELM_ACCESS_ACTION_UNHIGHLIGHT, &action_info);
		if (ret == EINA_FALSE) {
			ErrPrint("Action error\n");
		}
	} else if (!strcasecmp(operation, "next,hl")) {
		action_info.highlight_cycle = (!!option) && (!!strcasecmp(option, "no,cycle"));

		ret = elm_access_action(edje, ELM_ACCESS_ACTION_HIGHLIGHT_NEXT, &action_info);
		if (ret == EINA_FALSE) {
			ErrPrint("Action error\n");
		}
	} else if (!strcasecmp(operation, "prev,hl")) {
		action_info.highlight_cycle = EINA_TRUE;
		ret = elm_access_action(edje, ELM_ACCESS_ACTION_HIGHLIGHT_PREV, &action_info);
		if (ret == EINA_FALSE) {
			ErrPrint("Action error\n");
		}
	} else if (!strcasecmp(operation, "reset,focus")) {
		DbgPrint("Reset Focus\n");
		elm_object_focus_custom_chain_set(edje, NULL);
	}

out:
	return WIDGET_ERROR_NONE;
}

static inline void apply_shadow_effect(struct image_option *img_opt, Evas_Object *img)
{
	if (!img_opt->shadow.enabled) {
		return;
	}

#if defined(ENABLE_EFL_ASSIST)
	ea_effect_h *ea_effect;

	ea_effect = ea_image_effect_create();
	if (!ea_effect) {
		return;
	}

	// -90, 2, 4, 0x99000000
	ea_image_effect_add_outer_shadow(ea_effect, img_opt->shadow.angle, img_opt->shadow.offset, img_opt->shadow.softness, img_opt->shadow.color);
	ea_object_image_effect_set(img, ea_effect);

	ea_image_effect_destroy(ea_effect);
#endif
}

static Evas_Object *crop_image(Evas_Object *img, const char *path, int part_w, int part_h, int w, int h, struct image_option *img_opt)
{
	Ecore_Evas *ee;
	Evas *e;
	Evas_Object *src_img;
	Evas_Coord rw, rh;
	const void *data;
	Evas_Load_Error err;
	Evas_Object *_img;

	ee = ecore_evas_buffer_new(part_w, part_h);
	if (!ee) {
		ErrPrint("Failed to create a EE\n");
		return img;
	}

	ecore_evas_alpha_set(ee, EINA_TRUE);

	e = ecore_evas_get(ee);
	if (!e) {
		ErrPrint("Unable to get Evas\n");
		ecore_evas_free(ee);
		return img;
	}

	src_img = evas_object_image_filled_add(e);
	if (!src_img) {
		ErrPrint("Unable to add an image\n");
		ecore_evas_free(ee);
		return img;
	}

	evas_object_image_alpha_set(src_img, EINA_TRUE);
	evas_object_image_colorspace_set(src_img, EVAS_COLORSPACE_ARGB8888);
	evas_object_image_smooth_scale_set(src_img, EINA_TRUE);
	evas_object_image_load_orientation_set(src_img, img_opt->orient);
	evas_object_image_file_set(src_img, path, NULL);
	err = evas_object_image_load_error_get(src_img);
	if (err != EVAS_LOAD_ERROR_NONE) {
		ErrPrint("Load error: %s\n", evas_load_error_str(err));
		evas_object_del(src_img);
		ecore_evas_free(ee);
		return img;
	}
	evas_object_image_size_get(src_img, &rw, &rh);
	evas_object_image_fill_set(src_img, 0, 0, rw, rh);
	evas_object_resize(src_img, w, h);
	evas_object_move(src_img, -(w - part_w) / 2, -(h - part_h) / 2);
	evas_object_show(src_img);

	data = ecore_evas_buffer_pixels_get(ee);
	if (!data) {
		ErrPrint("Unable to get pixels\n");
		evas_object_del(src_img);
		ecore_evas_free(ee);
		return img;
	}

	e = evas_object_evas_get(img);
	_img = evas_object_image_filled_add(e);
	if (!_img) {
		evas_object_del(src_img);
		ecore_evas_free(ee);
		return img;
	}

	evas_object_image_colorspace_set(_img, EVAS_COLORSPACE_ARGB8888);
	evas_object_image_smooth_scale_set(_img, EINA_TRUE);
	evas_object_image_alpha_set(_img, EINA_TRUE);
	evas_object_image_data_set(_img, NULL);
	evas_object_image_size_set(_img, part_w, part_h);
	evas_object_resize(_img, part_w, part_h);
	evas_object_image_data_copy_set(_img, (void *)data);
	evas_object_image_fill_set(_img, 0, 0, part_w, part_h);
	evas_object_image_data_update_add(_img, 0, 0, part_w, part_h);

	evas_object_del(src_img);
	ecore_evas_free(ee);

	evas_object_del(img);
	return _img;
}

PUBLIC int script_update_image(void *_h, const char *id, const char *part, const char *path, const char *option)
{
	struct info *handle = _h;
	Evas_Load_Error err;
	Evas_Object *edje;
	Evas_Object *img;
	Evas_Coord w, h;
	struct obj_info *obj_info;
	struct image_option img_opt = {
		.aspect = 0,
		.orient = 0,
		.fill = FILL_DISABLE,
		.width = -1,
		.height = -1,
		.shadow = {
			.enabled = 0,
		},
	};

	edje = find_edje(handle, id);
	if (!edje) {
		ErrPrint("No such object: %s\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	obj_info = evas_object_data_get(edje, "obj_info");
	if (!obj_info) {
		ErrPrint("Object info is not available\n");
		return WIDGET_ERROR_FAULT;
	}

	img = elm_object_part_content_unset(edje, part);
	if (img) {
		DbgPrint("delete object %s %p\n", part, img);
		evas_object_del(img);
	}

	if (!path || !strlen(path) || access(path, R_OK) != 0) {
		DbgPrint("SKIP - Path: [%s]\n", path);
		return WIDGET_ERROR_NONE;
	}

	img = evas_object_image_add(handle->e);
	if (!img) {
		ErrPrint("Failed to add an image object\n");
		return WIDGET_ERROR_FAULT;
	}

	evas_object_image_preload(img, EINA_FALSE);
	parse_image_option(option, &img_opt);
	evas_object_image_load_orientation_set(img, img_opt.orient);

	evas_object_image_file_set(img, path, NULL);
	err = evas_object_image_load_error_get(img);
	if (err != EVAS_LOAD_ERROR_NONE) {
		ErrPrint("Load error: %s\n", evas_load_error_str(err));
		evas_object_del(img);
		return WIDGET_ERROR_IO_ERROR;
	}

	apply_shadow_effect(&img_opt, img);

	evas_object_image_size_get(img, &w, &h);
	if (img_opt.aspect) {
		if (img_opt.fill == FILL_OVER_SIZE) {
			Evas_Coord part_w;
			Evas_Coord part_h;

			if (img_opt.width >= 0 && img_opt.height >= 0) {
				part_w = img_opt.width * elm_config_scale_get();
				part_h = img_opt.height * elm_config_scale_get();
			} else {
				part_w = 0;
				part_h = 0;
				edje_object_part_geometry_get(elm_layout_edje_get(edje), part, NULL, NULL, &part_w, &part_h);
			}
			DbgPrint("Original %dx%d (part: %dx%d)\n", w, h, part_w, part_h);

			if (part_w > w || part_h > h) {
				double fw;
				double fh;

				fw = (double)part_w / (double)w;
				fh = (double)part_h / (double)h;

				if (fw > fh) {
					w = part_w;
					h = (double)h * fw;
				} else {
					h = part_h;
					w = (double)w * fh;
				}
			}

			if (!part_w || !part_h || !w || !h) {
				evas_object_del(img);
				return WIDGET_ERROR_INVALID_PARAMETER;
			}

			img = crop_image(img, path, part_w, part_h, w, h, &img_opt);
		} else if (img_opt.fill == FILL_IN_SIZE) {
			Evas_Coord part_w;
			Evas_Coord part_h;

			if (img_opt.width >= 0 && img_opt.height >= 0) {
				part_w = img_opt.width * elm_config_scale_get();
				part_h = img_opt.height * elm_config_scale_get();
			} else {
				part_w = 0;
				part_h = 0;
				edje_object_part_geometry_get(elm_layout_edje_get(edje), part, NULL, NULL, &part_w, &part_h);
			}
			DbgPrint("Original %dx%d (part: %dx%d)\n", w, h, part_w, part_h);

			if (w > part_w || h > part_h) {
				double fw;
				double fh;

				fw = (double)part_w / (double)w;
				fh = (double)part_h / (double)h;

				if (fw > fh) {
					h = part_h;
					w = (double)w * fh;
				} else {
					w = part_w;
					h = (double)h * fw;
				}
			}

			if (!part_w || !part_h || !w || !h) {
				evas_object_del(img);
				return WIDGET_ERROR_INVALID_PARAMETER;
			}

			img = crop_image(img, path, part_w, part_h, w, h, &img_opt);
		} else if (img_opt.fill == FILL_FIT_SIZE) {
			Evas_Coord part_w;
			Evas_Coord part_h;
			double fw;
			double fh;

			if (img_opt.width >= 0 && img_opt.height >= 0) {
				part_w = img_opt.width * elm_config_scale_get();
				part_h = img_opt.height * elm_config_scale_get();
			} else {
				part_w = 0;
				part_h = 0;
				edje_object_part_geometry_get(elm_layout_edje_get(edje), part, NULL, NULL, &part_w, &part_h);
			}
			DbgPrint("Original %dx%d (part: %dx%d)\n", w, h, part_w, part_h);

			fw = (double)part_w / (double)w;
			fh = (double)part_h / (double)h;

			if (fw < fh) {
				h = part_h;
				w = (double)w * fh;
			} else {
				w = part_w;
				h = (double)h * fw;
			}

			if (!part_w || !part_h || !w || !h) {
				evas_object_del(img);
				return WIDGET_ERROR_INVALID_PARAMETER;
			}

			img = crop_image(img, path, part_w, part_h, w, h, &img_opt);
		} else {
			evas_object_image_fill_set(img, 0, 0, w, h);
			evas_object_size_hint_fill_set(img, EVAS_HINT_FILL, EVAS_HINT_FILL);
			evas_object_size_hint_aspect_set(img, EVAS_ASPECT_CONTROL_BOTH, w, h);
		}

		apply_shadow_effect(&img_opt, img);
	} else {
		if (img_opt.width >= 0 && img_opt.height >= 0) {
			w = img_opt.width;
			h = img_opt.height;
			DbgPrint("Using given image size: %dx%d\n", w, h);
		}

		evas_object_image_fill_set(img, 0, 0, w, h);
		evas_object_size_hint_fill_set(img, EVAS_HINT_FILL, EVAS_HINT_FILL);
		evas_object_size_hint_weight_set(img, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
		evas_object_image_filled_set(img, EINA_TRUE);
	}


	/*!
	 * \note
	 * object will be shown by below statement automatically
	 */
	DbgPrint("%s part swallow image %p (%dx%d)\n", part, img, w, h);
	elm_object_part_content_set(edje, part, img);

	/*!
	 * \note
	 * This object is not registered as an access object.
	 * So the developer should add it to access list manually, using DESC_ACCESS block.
	 */
	return WIDGET_ERROR_NONE;
}

static void script_signal_cb(void *data, Evas_Object *obj, const char *signal_name, const char *source)
{
	struct info *handle = data;
	Evas_Coord w;
	Evas_Coord h;
	Evas_Coord px = 0;
	Evas_Coord py = 0;
	Evas_Coord pw = 0;
	Evas_Coord ph = 0;
	double sx;
	double sy;
	double ex;
	double ey;

	evas_object_geometry_get(obj, NULL, NULL, &w, &h);
	edje_object_part_geometry_get(elm_layout_edje_get(obj), source, &px, &py, &pw, &ph);

	sx = ex = 0.0f;
	if (w) {
		sx = (double)px / (double)w;
		ex = (double)(px + pw) / (double)w;
	}

	sy = ey = 0.0f;
	if (h) {
		sy = (double)py / (double)h;
		ey = (double)(py + ph) / (double)h;
	}

	DbgPrint("[%s] [%s]\n", signal_name, source);

	script_buffer_signal_emit(handle->buffer_handle, source, signal_name, sx, sy, ex, ey);
}

static void edje_del_cb(void *_info, Evas *e, Evas_Object *obj, void *event_info)
{
	struct info *handle = _info;
	struct obj_info *obj_info;
	struct obj_info *parent_obj_info;
	struct child *child;

	handle->obj_list = eina_list_remove(handle->obj_list, obj);

	obj_info = evas_object_data_get(obj, "obj_info");
	if (!obj_info) {
		ErrPrint("Object info is not valid\n");
		return;
	}

	elm_object_signal_callback_del(obj, "*", "*", script_signal_cb);

	DbgPrint("delete object %s %p\n", obj_info->id, obj);
	if (obj_info->parent == obj) {
		DbgPrint("Parent EDJE\n");
	} else if (obj_info->parent) {
		Eina_List *l;
		Eina_List *n;

		parent_obj_info = evas_object_data_get(obj_info->parent, "obj_info");
		if (parent_obj_info) {
			EINA_LIST_FOREACH_SAFE(parent_obj_info->children, l, n, child) {
				if (child->obj != obj) {
					continue;
				}

				/*!
				 * \note
				 * If this code is executed,
				 * The parent is not deleted by desc, this object is deleted by itself.
				 * It is not possible, but we care it.
				 */
				DbgPrint("Children is updated: %s (%s)\n", child->part, parent_obj_info->id);
				parent_obj_info->children = eina_list_remove(parent_obj_info->children, child);
				free(child->part);
				free(child);
				break;
			}

			if (!parent_obj_info->children && parent_obj_info->delete_me == 1) {
				DbgPrint("Children is cleared: %s (by a child)\n", parent_obj_info->id);
				evas_object_data_del(obj_info->parent, "obj_info");
				free(parent_obj_info->id);
				free(parent_obj_info);
			}
		}
	} else {
		DbgPrint("obj_info->parent is NULL (skipped)\n");
	}

	if (!obj_info->children) {
		DbgPrint("Children is cleared: %s\n", obj_info->id);
		evas_object_data_del(obj, "obj_info");
		free(obj_info->id);
		free(obj_info);
	} else {
		DbgPrint("Children is remained: %s\n", obj_info->id);
		obj_info->delete_me = 1;
	}
}

static inline Evas_Object *get_highlighted_object(Evas_Object *obj)
{
	Evas_Object *o, *ho;

	o = evas_object_name_find(evas_object_evas_get(obj), "_elm_access_disp");
	if (!o) {
		return NULL;      
	}

	ho = evas_object_data_get(o, "_elm_access_target");
	return ho;
}

/*!
  WIDGET_ACCESS_HIGHLIGHT		0
  WIDGET_ACCESS_HIGHLIGHT_NEXT	1
  WIDGET_ACCESS_HIGHLIGHT_PREV	2
  WIDGET_ACCESS_ACTIVATE		3
  WIDGET_ACCESS_ACTION		4
  WIDGET_ACCESS_SCROLL		5
 */
PUBLIC int script_feed_event(void *h, int event_type, int x, int y, int type, unsigned int keycode, double timestamp)
{
	struct info *handle = h;
	Evas_Object *edje;
	struct obj_info *obj_info;
	int ret = WIDGET_ERROR_NONE;

	edje = find_edje(handle, NULL); /*!< Get the base layout */
	if (!edje) {
		ErrPrint("Base layout is not exist\n");
		return WIDGET_ERROR_NOT_EXIST;
	}

	obj_info = evas_object_data_get(edje, "obj_info");
	if (!obj_info) {
		ErrPrint("Object info is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (event_type & WIDGET_SCRIPT_ACCESS_EVENT) {
		Elm_Access_Action_Info info;
		Elm_Access_Action_Type action;

		memset(&info, 0, sizeof(info));

		if ((event_type & WIDGET_SCRIPT_ACCESS_HIGHLIGHT) == WIDGET_SCRIPT_ACCESS_HIGHLIGHT) {
			action = ELM_ACCESS_ACTION_HIGHLIGHT;
			info.x = x;
			info.y = y;
			ret = elm_access_action(edje, action, &info);
			DbgPrint("ACCESS_HIGHLIGHT: %dx%d returns %d\n", x, y, ret);
			if (ret == EINA_TRUE) {
				if (!get_highlighted_object(edje)) {
					ErrPrint("Highlighted object is not found\n");
					ret = WIDGET_ACCESS_STATUS_ERROR;
				} else {
					DbgPrint("Highlighted object is found\n");
					ret = WIDGET_ACCESS_STATUS_DONE;
				}
			} else {
				ErrPrint("Action error\n");
				ret = WIDGET_ACCESS_STATUS_ERROR;
			}
		} else if ((event_type & WIDGET_SCRIPT_ACCESS_HIGHLIGHT_NEXT) == WIDGET_SCRIPT_ACCESS_HIGHLIGHT_NEXT) {
			action = ELM_ACCESS_ACTION_HIGHLIGHT_NEXT;
			info.highlight_cycle = EINA_FALSE;
			ret = elm_access_action(edje, action, &info);
			DbgPrint("ACCESS_HIGHLIGHT_NEXT, returns %d\n", ret);
			ret = (ret == EINA_FALSE) ? WIDGET_ACCESS_STATUS_LAST : WIDGET_ACCESS_STATUS_DONE;
		} else if ((event_type & WIDGET_SCRIPT_ACCESS_HIGHLIGHT_PREV) == WIDGET_SCRIPT_ACCESS_HIGHLIGHT_PREV) {
			action = ELM_ACCESS_ACTION_HIGHLIGHT_PREV;
			info.highlight_cycle = EINA_FALSE;
			ret = elm_access_action(edje, action, &info);
			DbgPrint("ACCESS_HIGHLIGHT_PREV, returns %d\n", ret);
			ret = (ret == EINA_FALSE) ? WIDGET_ACCESS_STATUS_FIRST : WIDGET_ACCESS_STATUS_DONE;
		} else if ((event_type & WIDGET_SCRIPT_ACCESS_ACTIVATE) == WIDGET_SCRIPT_ACCESS_ACTIVATE) {
			action = ELM_ACCESS_ACTION_ACTIVATE;
			ret = elm_access_action(edje, action, &info);
			DbgPrint("ACCESS_ACTIVATE, returns %d\n", ret);
			ret = (ret == EINA_FALSE) ? WIDGET_ACCESS_STATUS_ERROR : WIDGET_ACCESS_STATUS_DONE;
		} else if ((event_type & WIDGET_SCRIPT_ACCESS_ACTION) == WIDGET_SCRIPT_ACCESS_ACTION) {
			switch (type) {
			case ACCESS_TYPE_UP:
				action = ELM_ACCESS_ACTION_UP;
				ret = elm_access_action(edje, action, &info);
				DbgPrint("ACCESS_ACTION(%d), returns %d\n", type, ret);
				ret = (ret == EINA_FALSE) ? WIDGET_ACCESS_STATUS_ERROR : WIDGET_ACCESS_STATUS_DONE;
				break;
			case ACCESS_TYPE_DOWN:
				action = ELM_ACCESS_ACTION_DOWN;
				ret = elm_access_action(edje, action, &info);
				DbgPrint("ACCESS_ACTION(%d), returns %d\n", type, ret);
				ret = (ret == EINA_FALSE) ? WIDGET_ACCESS_STATUS_ERROR : WIDGET_ACCESS_STATUS_DONE;
				break;
			default:
				ErrPrint("Invalid access event\n");
				ret = WIDGET_ACCESS_STATUS_ERROR;
				break;
			}
		} else if ((event_type & WIDGET_SCRIPT_ACCESS_SCROLL) == WIDGET_SCRIPT_ACCESS_SCROLL) {
			action = ELM_ACCESS_ACTION_SCROLL;
			info.x = x;
			info.y = y;
			switch (type) {
			case ACCESS_TYPE_DOWN:
				info.mouse_type = 0;
				ret = elm_access_action(edje, action, &info);
				DbgPrint("ACCESS_HIGHLIGHT_SCROLL, returns %d\n", ret);
				ret = (ret == EINA_FALSE) ? WIDGET_ACCESS_STATUS_ERROR : WIDGET_ACCESS_STATUS_DONE;
				break;
			case ACCESS_TYPE_MOVE:
				info.mouse_type = 1;
				ret = elm_access_action(edje, action, &info);
				DbgPrint("ACCESS_HIGHLIGHT_SCROLL, returns %d\n", ret);
				ret = (ret == EINA_FALSE) ? WIDGET_ACCESS_STATUS_ERROR : WIDGET_ACCESS_STATUS_DONE;
				break;
			case ACCESS_TYPE_UP:
				info.mouse_type = 2;
				ret = elm_access_action(edje, action, &info);
				DbgPrint("ACCESS_HIGHLIGHT_SCROLL, returns %d\n", ret);
				ret = (ret == EINA_FALSE) ? WIDGET_ACCESS_STATUS_ERROR : WIDGET_ACCESS_STATUS_DONE;
				break;
			default:
				ret = WIDGET_ACCESS_STATUS_ERROR;
				break;
			}
		} else if ((event_type & WIDGET_SCRIPT_ACCESS_UNHIGHLIGHT) == WIDGET_SCRIPT_ACCESS_UNHIGHLIGHT) {
			action = ELM_ACCESS_ACTION_UNHIGHLIGHT;
			ret = elm_access_action(edje, action, &info);
			DbgPrint("ACCESS_UNHIGHLIGHT, returns %d\n", ret);
			ret = (ret == EINA_FALSE) ? WIDGET_ACCESS_STATUS_ERROR : WIDGET_ACCESS_STATUS_DONE;
		} else if ((event_type & WIDGET_SCRIPT_ACCESS_VALUE_CHANGE) == WIDGET_SCRIPT_ACCESS_VALUE_CHANGE) {
			action = ELM_ACCESS_ACTION_VALUE_CHANGE;
			info.mouse_type = type;
			info.x = x;
			info.y = y;
			ret = elm_access_action(edje, action, &info);
			ret = (ret == EINA_FALSE) ? WIDGET_ACCESS_STATUS_ERROR : WIDGET_ACCESS_STATUS_DONE;
		} else if ((event_type & WIDGET_SCRIPT_ACCESS_MOUSE) == WIDGET_SCRIPT_ACCESS_MOUSE) {
			action = ELM_ACCESS_ACTION_MOUSE;
			info.mouse_type = type;
			info.x = x;
			info.y = y;
			ret = elm_access_action(edje, action, &info);
			ret = (ret == EINA_FALSE) ? WIDGET_ACCESS_STATUS_ERROR : WIDGET_ACCESS_STATUS_DONE;
		} else if ((event_type & WIDGET_SCRIPT_ACCESS_BACK) == WIDGET_SCRIPT_ACCESS_BACK) {
			action = ELM_ACCESS_ACTION_BACK;
			info.mouse_type = type;
			info.x = x;
			info.y = y;
			ret = elm_access_action(edje, action, &info);
			ret = (ret == EINA_FALSE) ? WIDGET_ACCESS_STATUS_ERROR : WIDGET_ACCESS_STATUS_DONE;
		} else if ((event_type & WIDGET_SCRIPT_ACCESS_OVER) == WIDGET_SCRIPT_ACCESS_OVER) {
			action = ELM_ACCESS_ACTION_OVER;
			info.mouse_type = type;
			info.x = x;
			info.y = y;
			ret = elm_access_action(edje, action, &info);
			ret = (ret == EINA_FALSE) ? WIDGET_ACCESS_STATUS_ERROR : WIDGET_ACCESS_STATUS_DONE;
		} else if ((event_type & WIDGET_SCRIPT_ACCESS_READ) == WIDGET_SCRIPT_ACCESS_READ) {
			action = ELM_ACCESS_ACTION_READ;
			info.mouse_type = type;
			info.x = x;
			info.y = y;
			ret = elm_access_action(edje, action, &info);
			ret = (ret == EINA_FALSE) ? WIDGET_ACCESS_STATUS_ERROR : WIDGET_ACCESS_STATUS_DONE;
		} else if ((event_type & WIDGET_SCRIPT_ACCESS_ENABLE) == WIDGET_SCRIPT_ACCESS_ENABLE) {
			action = ELM_ACCESS_ACTION_ENABLE;
			info.mouse_type = type;
			info.x = x;
			info.y = y;
			ret = elm_access_action(edje, action, &info);
			ret = (ret == EINA_FALSE) ? WIDGET_ACCESS_STATUS_ERROR : WIDGET_ACCESS_STATUS_DONE;
		} else if ((event_type & WIDGET_SCRIPT_ACCESS_DISABLE) == WIDGET_SCRIPT_ACCESS_DISABLE) {
			action = ELM_ACCESS_ACTION_ENABLE;
			info.mouse_type = type;
			info.x = x;
			info.y = y;
			ret = elm_access_action(edje, action, &info);
			ret = (ret == EINA_FALSE) ? WIDGET_ACCESS_STATUS_ERROR : WIDGET_ACCESS_STATUS_DONE;
		} else {
			DbgPrint("Invalid event\n");
			ret = WIDGET_ACCESS_STATUS_ERROR;
		}

	} else if (event_type & WIDGET_SCRIPT_MOUSE_EVENT) {
		double cur_timestamp;
		unsigned int flags;

#if defined(_USE_ECORE_TIME_GET)
		cur_timestamp = ecore_time_get();
#else
		struct timeval tv;
		if (gettimeofday(&tv, NULL) < 0) {
			ErrPrint("Failed to get time\n");
			cur_timestamp = 0.0f;
		} else {
			cur_timestamp = (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0f);
		}
#endif
		if (cur_timestamp - timestamp > 0.1f && handle->is_mouse_down == 0) {
			DbgPrint("Discard lazy event : %lf\n", cur_timestamp - timestamp);
			return WIDGET_ERROR_NONE;
		}

		switch (event_type) {
		case WIDGET_SCRIPT_MOUSE_DOWN:
			if (handle->is_mouse_down == 0) {
				flags = evas_event_default_flags_get(handle->e);
				flags &= ~EVAS_EVENT_FLAG_ON_SCROLL;
				flags &= ~EVAS_EVENT_FLAG_ON_HOLD;
				evas_event_default_flags_set(handle->e, flags);

				evas_event_feed_mouse_move(handle->e, x, y, timestamp * 1000, NULL);
				evas_event_feed_mouse_down(handle->e, 1, EVAS_BUTTON_NONE, (timestamp + 0.01f) * 1000, NULL);
				handle->is_mouse_down = 1;
			}
			break;
		case WIDGET_SCRIPT_MOUSE_MOVE:
			evas_event_feed_mouse_move(handle->e, x, y, timestamp * 1000, NULL);
			break;
		case WIDGET_SCRIPT_MOUSE_UP:
			if (handle->is_mouse_down == 1) {
				evas_event_feed_mouse_move(handle->e, x, y, timestamp * 1000, NULL);
				evas_event_feed_mouse_up(handle->e, 1, EVAS_BUTTON_NONE, (timestamp + 0.01f) * 1000, NULL);
				handle->is_mouse_down = 0;
			}
			break;
		case WIDGET_SCRIPT_MOUSE_IN:
			evas_event_feed_mouse_in(handle->e, timestamp * 1000, NULL);
			break;
		case WIDGET_SCRIPT_MOUSE_OUT:
			evas_event_feed_mouse_out(handle->e, timestamp * 1000, NULL);
			break;
		case WIDGET_SCRIPT_MOUSE_ON_SCROLL:
			flags = evas_event_default_flags_get(handle->e);
			flags |= EVAS_EVENT_FLAG_ON_SCROLL;
			evas_event_default_flags_set(handle->e, flags);
			break;
		case WIDGET_SCRIPT_MOUSE_ON_HOLD:	// To cancel the clicked, enable this
			flags = evas_event_default_flags_get(handle->e);
			flags |= EVAS_EVENT_FLAG_ON_HOLD;
			evas_event_default_flags_set(handle->e, flags);
			break;
		case WIDGET_SCRIPT_MOUSE_OFF_SCROLL:
			flags = evas_event_default_flags_get(handle->e);
			flags &= ~EVAS_EVENT_FLAG_ON_SCROLL;
			evas_event_default_flags_set(handle->e, flags);
			break;
		case WIDGET_SCRIPT_MOUSE_OFF_HOLD:
			flags = evas_event_default_flags_get(handle->e);
			flags &= ~EVAS_EVENT_FLAG_ON_HOLD;
			evas_event_default_flags_set(handle->e, flags);
			break;
		default:
			return WIDGET_ERROR_INVALID_PARAMETER;
		}
	} else if (event_type & WIDGET_SCRIPT_KEY_EVENT) {
		const char *keyname = "";
		const char *key = "";
		const char *string = "";
		const char *compose = "";

		switch (event_type) {
		case WIDGET_SCRIPT_KEY_DOWN:
			evas_event_feed_key_down(handle->e, keyname, key, string, compose, timestamp * 1000, NULL);
			ret = WIDGET_KEY_STATUS_DONE;
			/*!
			 * \TODO
			 * If the keyname == RIGHT, Need to check that
			 * Does it reach to the last focusable object?
			 */

			/*!
			 * if (REACH to the LAST) {
			 *    ret = WIDGET_KEY_STATUS_LAST;
			 * } else {
			 *    ret = WIDGET_KEY_STATUS_DONE;
			 * }
			 *
			 * if (REACH to the FIRST) {
			 *    ret = WIDGET_KEY_STATUS_FIRST;
			 * } else {
			 *    ret = WIDGET_KEY_STATUS_DONE;
			 * }
			 */
			break;
		case WIDGET_SCRIPT_KEY_UP:
			evas_event_feed_key_up(handle->e, keyname, key, string, compose, timestamp * 1000, NULL);
			ret = WIDGET_KEY_STATUS_DONE;
			break;
		case WIDGET_SCRIPT_KEY_FOCUS_IN:
			// evas_event_callback_call(handle->e, EVAS_CALLBACK_CANVAS_FOCUS_IN, NULL);
			ret = WIDGET_KEY_STATUS_DONE;
			break;
		case WIDGET_SCRIPT_KEY_FOCUS_OUT:
			// evas_event_callback_call(handle->e, EVAS_CALLBACK_CANVAS_FOCUS_OUT, NULL);
			ret = WIDGET_KEY_STATUS_DONE;
			break;
		default:
			DbgPrint("Event is not implemented\n");
			ret = WIDGET_KEY_STATUS_ERROR;
			break;
		}
	}

	return ret;
}

PUBLIC int script_update_script(void *h, const char *src_id, const char *target_id, const char *part, const char *path, const char *group)
{
	struct info *handle = h;
	Evas_Object *edje;
	Evas_Object *obj;
	struct obj_info *obj_info;
	struct obj_info *parent_obj_info;
	struct child *child;
	char _target_id[32];

	edje = find_edje(handle, src_id);
	if (!edje) {
		ErrPrint("Edje is not exists (%s)\n", src_id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	parent_obj_info = evas_object_data_get(edje, "obj_info");
	if (!parent_obj_info) {
		ErrPrint("Object info is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	obj = elm_object_part_content_unset(edje, part);
	if (obj) {
		DbgPrint("delete object %s %p\n", part, obj);
		/*!
		 * \note
		 * This will call the edje_del_cb.
		 * It will delete all access objects
		 */
		evas_object_del(obj);
	}

	if (!path || !strlen(path) || access(path, R_OK) != 0) {
		DbgPrint("SKIP - Path: [%s]\n", path);
		return WIDGET_ERROR_NONE;
	}

	if (!target_id) {
		if (find_edje(handle, part)) {
			double timestamp;

			do {
#if defined(_USE_ECORE_TIME_GET)
				timestamp = ecore_time_get();
#else
				struct timeval tv;
				if (gettimeofday(&tv, NULL) < 0) {
					static int local_idx = 0;
					timestamp = (double)(local_idx++);
				} else {
					timestamp = (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0f);
				}
#endif

				snprintf(_target_id, sizeof(_target_id), "%lf", timestamp);
			} while (find_edje(handle, _target_id));

			target_id = _target_id;
		} else {
			target_id = part;
		}

		DbgPrint("Anonymouse target id: %s\n", target_id);
	}

	obj = elm_layout_add(edje);
	if (!obj) {
		ErrPrint("Failed to add a new edje object\n");
		return WIDGET_ERROR_FAULT;
	}

	edje_object_scale_set(elm_layout_edje_get(obj), elm_config_scale_get());

	if (!elm_layout_file_set(obj, path, group)) {
		int err;
		err = edje_object_load_error_get(elm_layout_edje_get(obj));
		if (err != EDJE_LOAD_ERROR_NONE) {
			ErrPrint("Could not load %s from %s: %s\n", group, path, edje_load_error_str(err));
		}
		evas_object_del(obj);
		return WIDGET_ERROR_IO_ERROR;
	}

	evas_object_show(obj);

	obj_info = calloc(1, sizeof(*obj_info));
	if (!obj_info) {
		ErrPrint("Failed to add a obj_info\n");
		evas_object_del(obj);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	obj_info->id = strdup(target_id);
	if (!obj_info->id) {
		ErrPrint("Failed to add a obj_info\n");
		free(obj_info);
		evas_object_del(obj);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	obj_info->parent = edje;

	child = malloc(sizeof(*child));
	if (!child) {
		ErrPrint("Error: %d\n", errno);
		free(obj_info->id);
		free(obj_info);
		evas_object_del(obj);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	child->part = strdup(part);
	if (!child->part) {
		ErrPrint("Error: %d\n", errno);
		free(child);
		free(obj_info->id);
		free(obj_info);
		evas_object_del(obj);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	child->obj = obj;

	evas_object_data_set(obj, "obj_info", obj_info);
	evas_object_event_callback_add(obj, EVAS_CALLBACK_DEL, edje_del_cb, handle);
	elm_object_signal_callback_add(obj, "*", "*", script_signal_cb, handle);
	handle->obj_list = eina_list_append(handle->obj_list, obj);

	DbgPrint("%s part swallow edje %p\n", part, obj);
	elm_object_part_content_set(edje, part, obj);

	parent_obj_info->children = eina_list_append(parent_obj_info->children, child);
	return WIDGET_ERROR_NONE;
}

PUBLIC int script_update_signal(void *h, const char *id, const char *part, const char *signal)
{
	struct info *handle = h;
	Evas_Object *edje;

	edje = find_edje(handle, id);
	if (!edje) {
		return WIDGET_ERROR_NOT_EXIST;
	}

	elm_object_signal_emit(edje, signal, part);
	return WIDGET_ERROR_NONE;
}

PUBLIC int script_update_drag(void *h, const char *id, const char *part, double x, double y)
{
	struct info *handle = h;
	Evas_Object *edje;

	edje = find_edje(handle, id);
	if (!edje) {
		return WIDGET_ERROR_NOT_EXIST;
	}

	edje_object_part_drag_value_set(elm_layout_edje_get(edje), part, x, y);
	return WIDGET_ERROR_NONE;
}

PUBLIC int script_update_size(void *han, const char *id, int w, int h)
{
	struct info *handle = han;
	Evas_Object *edje;

	edje = find_edje(handle, id);
	if (!edje) {
		return WIDGET_ERROR_NOT_EXIST;
	}

	if (!id) {
		/*!
		 * \note
		 * Need to resize the canvas too
		 */
		ecore_evas_resize(handle->ee, w, h);
	}

	DbgPrint("Resize object to %dx%d\n", w, h);
	evas_object_resize(edje, w, h);
	return WIDGET_ERROR_NONE;
}

PUBLIC int script_update_category(void *h, const char *id, const char *category)
{
	struct info *handle = h;

	if (handle->category) {
		free(handle->category);
		handle->category = NULL;
	}

	if (!category) {
		return WIDGET_ERROR_NONE;
	}

	handle->category = strdup(category);
	if (!handle->category) {
		ErrPrint("Error: %d\n", errno);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	return WIDGET_ERROR_NONE;
}

PUBLIC void *script_create(void *buffer_handle, const char *file, const char *group)
{
	struct info *handle;

	handle = calloc(1, sizeof(*handle));
	if (!handle) {
		ErrPrint("Error: %d\n", errno);
		return NULL;
	}

	handle->file = strdup(file);
	if (!handle->file) {
		ErrPrint("Error: %d\n", errno);
		free(handle);
		return NULL;
	}

	handle->group = strdup(group);
	if (!handle->group) {
		ErrPrint("Error: %d\n", errno);
		free(handle->file);
		free(handle);
		return NULL;
	}

	handle->buffer_handle = buffer_handle;

	s_info.handle_list = eina_list_append(s_info.handle_list, handle);

	return handle;
}

PUBLIC int script_destroy(void *_handle)
{
	struct info *handle;
	Evas_Object *edje;

	handle = _handle;

	if (!eina_list_data_find(s_info.handle_list, handle)) {
		DbgPrint("Not found (already deleted?)\n");
		return WIDGET_ERROR_NOT_EXIST;
	}

	s_info.handle_list = eina_list_remove(s_info.handle_list, handle);

	edje = eina_list_nth(handle->obj_list, 0);
	if (edje) {
		evas_object_del(edje);
	}

	DbgPrint("Release handle\n");
	free(handle->category);
	free(handle->file);
	free(handle->group);
	free(handle);
	return WIDGET_ERROR_NONE;
}

static void sw_render_pre_cb(void *data, Evas *e, void *event_info)
{
	struct info *handle = data;

	if (handle->render_pre) {
		handle->render_pre(handle->buffer_handle, handle->render_data);
	}

	script_buffer_lock(handle->buffer_handle);

	if (s_info.premultiplied) {
		int w;
		int h;

		script_buffer_get_size(handle->buffer_handle, &w, &h);
		evas_damage_rectangle_add(handle->e, 0, 0, w, h);
	}
}

static void sw_render_post_cb(void *data, Evas *e, void *event_info)
{
	struct info *handle = data;

	if (s_info.premultiplied) {
		void *canvas;
		int x, y, w, h;

		// Get a pointer of a buffer of the virtual canvas
		canvas = (void *)ecore_evas_buffer_pixels_get(handle->ee);
		if (!canvas) {
			ErrPrint("Failed to get pixel canvas\n");
			return;
		}

		ecore_evas_geometry_get(handle->ee, &x, &y, &w, &h);
		evas_data_argb_unpremul(canvas, w * h);
	}

	script_buffer_unlock(handle->buffer_handle);

	if (handle->render_post) {
		handle->render_post(handle->buffer_handle, handle->render_data);
	}
}

static void render_pre_cb(void *data, Evas *e, void *event_info)
{
	struct info *handle = data;
	void *canvas;

	canvas = script_buffer_pixmap_acquire_buffer(handle->buffer_handle);
	if (!canvas) {
		ErrPrint("Acquired buffer is NULL\n");
	}

	sw_render_pre_cb(data, handle->e, event_info);
}

static void render_post_cb(void *data, Evas *e, void *event_info)
{
	struct info *handle = data;
	void *canvas;

	sw_render_post_cb(data, handle->e, event_info);
	canvas = script_buffer_pixmap_buffer(handle->buffer_handle);
	if (!canvas) {
		ErrPrint("Acquired buffer is NULL\n");
	} else {
		script_buffer_pixmap_release_buffer(canvas);
	}
}

static void *alloc_fb(void *data, int size)
{
	struct info *handle = data;

	if (script_buffer_load(handle->buffer_handle) < 0) {
		ErrPrint("Failed to load buffer handler\n");
		return NULL;
	}

	return script_buffer_fb(handle->buffer_handle);
}

static void *alloc_with_stride_fb(void *data, int size, int *stride, int *bpp)
{
	void *canvas;
	struct info *handle = data;
	int _stride;
	int _bpp;

	canvas = alloc_fb(data, size);
	if (!canvas) {
		ErrPrint("Unable to allocate canvas buffer\n");
	}

	_bpp = script_buffer_pixels(handle->buffer_handle);
	if (_bpp < 0) {
		ErrPrint("Failed to get pixel size, fallback to 4\n");
		_bpp = sizeof(int);
	}

	_stride = script_buffer_stride(handle->buffer_handle);
	if (_stride < 0) {
		int w = 0;

		ecore_evas_geometry_get(handle->ee, NULL, NULL, &w, NULL);

		_stride = w * _bpp;
		ErrPrint("Failed to get stride info, fallback to %d\n", _stride);
	}

	*stride = _stride;
	*bpp = _bpp << 3;

	return canvas;
}

static void free_fb(void *data, void *ptr)
{
	struct info *handle = data;

	if (!handle->buffer_handle) {
		ErrPrint("Buffer is not valid (maybe already released)\n");
		return;
	}

	if (script_buffer_fb(handle->buffer_handle) != ptr) {
		ErrPrint("Buffer pointer is not matched\n");
	}

	(void)script_buffer_unload(handle->buffer_handle);
}

static int destroy_ecore_evas(struct info *handle)
{
	if (!handle->ee) {
		return WIDGET_ERROR_NONE;
	}
	ecore_evas_free(handle->ee);
	handle->ee = NULL;
	handle->e = NULL;
	return WIDGET_ERROR_NONE;
}

static int create_ecore_evas(struct info *handle, int *w, int *h)
{
	script_buffer_get_size(handle->buffer_handle, w, h);
	if (*w == 0 && *h == 0) {
		ErrPrint("ZERO size FB accessed\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (handle->ee) {
		int ow = 0;
		int oh = 0;

		ecore_evas_geometry_get(handle->ee, NULL, NULL, &ow, &oh);
		if (*w != ow || *h != oh) {
			ErrPrint("EE exists, But different size - buffer_handle(%dx%d) -> ee(%dx%d)\n", ow, oh, *w, *h);
			ecore_evas_resize(handle->ee, *w, *h);
		}

		return WIDGET_ERROR_NONE;
	}

	if (!script_buffer_auto_align() && s_info.alloc_canvas_with_stride) {
		handle->ee = s_info.alloc_canvas_with_stride(*w, *h, alloc_with_stride_fb, free_fb, handle);
	} else if (s_info.alloc_canvas) {
		handle->ee = s_info.alloc_canvas(*w, *h, alloc_fb, free_fb, handle);
	} else {
		ErrPrint("Failed to allocate canvas\n");
		return WIDGET_ERROR_FAULT;
	}

	if (!handle->ee) {
		ErrPrint("Failed to create a buffer\n");
		return WIDGET_ERROR_FAULT;
	}

	handle->e = ecore_evas_get(handle->ee);
	if (!handle->e) {
		ErrPrint("Failed to get an Evas\n");
		ecore_evas_free(handle->ee);
		handle->ee = NULL;
		return WIDGET_ERROR_FAULT;
	}

	if (script_buffer_type(handle->buffer_handle) == BUFFER_TYPE_PIXMAP) {
		void *canvas;

		evas_event_callback_add(handle->e, EVAS_CALLBACK_RENDER_PRE, render_pre_cb, handle);
		evas_event_callback_add(handle->e, EVAS_CALLBACK_RENDER_POST, render_post_cb, handle);

		/*
		 * \note
		 * ecore_evas_alpha_set tries to access the canvas buffer.
		 * Without any render_pre/render_post callback.
		 */
		canvas = script_buffer_pixmap_acquire_buffer(handle->buffer_handle);
		if (!canvas) {
			ErrPrint("Acquired buffer is NULL\n");
		} else {
			ecore_evas_alpha_set(handle->ee, EINA_TRUE);
			script_buffer_pixmap_release_buffer(canvas);
		}
	} else {
		evas_event_callback_add(handle->e, EVAS_CALLBACK_RENDER_PRE, sw_render_pre_cb, handle);
		evas_event_callback_add(handle->e, EVAS_CALLBACK_RENDER_POST, sw_render_post_cb, handle);
		ecore_evas_alpha_set(handle->ee, EINA_TRUE);
	}

	ecore_evas_manual_render_set(handle->ee, EINA_FALSE);
	ecore_evas_resize(handle->ee, *w, *h);
	ecore_evas_show(handle->ee);
	ecore_evas_activate(handle->ee);

	return WIDGET_ERROR_NONE;
}

PUBLIC int script_load(void *_handle, int (*render_pre)(void *buffer_handle, void *data), int (*render_post)(void *render_handle, void *data), void *data)
{
	struct info *handle;
	Evas_Object *edje;
	struct obj_info *obj_info;
	int ret;
	int w;
	int h;

	/*!
	 * \TODO
	 * Create "Ecore_Evas *"
	 */

	handle = _handle;

	handle->render_pre = render_pre;
	handle->render_post = render_post;
	handle->render_data = data;

	ret = create_ecore_evas(handle, &w, &h);
	if (ret < 0) {
		return ret;
	}

	obj_info = calloc(1, sizeof(*obj_info));
	if (!obj_info) {
		ErrPrint("Heap: %d\n", errno);
		destroy_ecore_evas(handle);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	obj_info->parent = evas_object_rectangle_add(handle->e);
	if (!obj_info->parent) {
		ErrPrint("Unable to create a parent box\n");
		free(obj_info);
		destroy_ecore_evas(handle);
		return WIDGET_ERROR_FAULT;
	}

	edje = elm_layout_add(obj_info->parent);
	if (!edje) {
		ErrPrint("Failed to create an edje object\n");
		evas_object_del(obj_info->parent);
		free(obj_info);
		destroy_ecore_evas(handle);
		return WIDGET_ERROR_FAULT;
	}

	edje_object_scale_set(elm_layout_edje_get(edje), elm_config_scale_get());

	if (!elm_layout_file_set(edje, handle->file, handle->group)) {
		int err;

		err = edje_object_load_error_get(elm_layout_edje_get(edje));
		if (err != EDJE_LOAD_ERROR_NONE) {
			ErrPrint("Could not load %s from %s: %s\n", handle->group, handle->file, edje_load_error_str(err));
		}
		evas_object_del(edje);
		evas_object_del(obj_info->parent);
		free(obj_info);
		destroy_ecore_evas(handle);
		return WIDGET_ERROR_IO_ERROR;
	}

	handle->parent = edje;

	elm_object_signal_callback_add(edje, "*", "*", script_signal_cb, handle);
	evas_object_event_callback_add(edje, EVAS_CALLBACK_DEL, edje_del_cb, handle);
	evas_object_size_hint_weight_set(edje, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_fill_set(edje, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_resize(edje, w, h);
	evas_object_show(edje);
	evas_object_data_set(edje, "obj_info", obj_info);

	handle->obj_list = eina_list_append(handle->obj_list, edje);
	return WIDGET_ERROR_NONE;
}

PUBLIC int script_unload(void *_handle)
{
	struct info *handle;

	/*!
	 * \TODO
	 * Destroy "Ecore_Evas *"
	 */

	handle = _handle;

	if (handle->parent) {
		DbgPrint("Delete parent box\n");
		evas_object_del(handle->parent);
	}

	(void)destroy_ecore_evas(handle);
	return WIDGET_ERROR_NONE;
}

static void access_cb(keynode_t *node, void *user_data)
{
	int state;

	if (!node) {
		if (vconf_get_bool(VCONFKEY_SETAPPL_ACCESSIBILITY_TTS, &state) != 0) {
			ErrPrint("Idle lock state is not valid\n");
			state = 0; /* DISABLED */
		}
	} else {
		state = vconf_keynode_get_bool(node);
	}

	DbgPrint("ELM CONFIG ACCESS: %d\n", state);
	elm_config_access_set(state);
	s_info.access_on = state;
}

static void update_font_cb(void *data)
{
	elm_config_font_overlay_set(TEXT_CLASS, s_info.font_name, DEFAULT_FONT_SIZE);
	DbgPrint("Update text class %s (%s, %d)\n", TEXT_CLASS, s_info.font_name, DEFAULT_FONT_SIZE);
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

		/*!
		 * Get the first font name using system_settings API.
		 */
		font_name = NULL;
		ret = system_settings_get_value_string(SYSTEM_SETTINGS_KEY_FONT_TYPE, &font_name);
		if (ret != SYSTEM_SETTINGS_ERROR_NONE || !font_name) {
			ErrPrint("System setting get: %d, font_name[%p]\n", ret, font_name);
			return;
		}
	}

	s_info.font_name = font_name;
	DbgPrint("Font name is changed to %s(%p)\n", s_info.font_name, s_info.font_name);

	/*!
	 * \NOTE
	 * Try to update all widgets
	 */
	update_font_cb(NULL);
}

static inline int convert_font_size(int size)
{
	switch (size) {
	case SYSTEM_SETTINGS_FONT_SIZE_SMALL:
		size = -80;
		break;
	case SYSTEM_SETTINGS_FONT_SIZE_NORMAL:
		size = -100;
		break;
	case SYSTEM_SETTINGS_FONT_SIZE_LARGE:
		size = -150;
		break;
	case SYSTEM_SETTINGS_FONT_SIZE_HUGE:
		size = -190;
		break;
	case SYSTEM_SETTINGS_FONT_SIZE_GIANT:
		size = -250;
		break;
	default:
		size = -100;
		break;
	}

	DbgPrint("Return size: %d\n", size);
	return size;
}

static void font_size_cb(system_settings_key_e key, void *user_data)
{
	int size;

	if (system_settings_get_value_int(SYSTEM_SETTINGS_KEY_FONT_SIZE, &size) != SYSTEM_SETTINGS_ERROR_NONE) {
		return;
	}

	size = convert_font_size(size);

	if (size == s_info.font_size) {
		DbgPrint("Font size is not changed\n");
		return;
	}

	s_info.font_size = size;
	DbgPrint("Font size is changed to %d, but don't update the font info\n", size);
}

PUBLIC int script_init(double scale, int premultiplied)
{
	int ret;
	char *argv[] = {
		"widget.edje",
		NULL,
	};

	s_info.premultiplied = premultiplied;

	/* ecore is already initialized */
	elm_init(1, argv);
	elm_config_scale_set(scale);
	DbgPrint("Scale is updated: %lf\n", scale);

	ret = vconf_notify_key_changed(VCONFKEY_SETAPPL_ACCESSIBILITY_TTS, access_cb, NULL);
	if (ret < 0) {
		DbgPrint("TTS changed: %d\n", ret);
	}

	ret = vconf_notify_key_changed("db/setting/accessibility/font_name", font_changed_cb, NULL);
	DbgPrint("System font is changed: %d\n", ret);

	ret = system_settings_set_changed_cb(SYSTEM_SETTINGS_KEY_FONT_SIZE, font_size_cb, NULL);
	DbgPrint("System font size is changed: %d\n", ret);

	access_cb(NULL, NULL);
	font_changed_cb(NULL, NULL);
	font_size_cb(SYSTEM_SETTINGS_KEY_FONT_SIZE, NULL);

	s_info.alloc_canvas_with_stride = dlsym(RTLD_DEFAULT, "ecore_evas_buffer_allocfunc_with_stride_new");
	if (!s_info.alloc_canvas_with_stride) {
		DbgPrint("Fallback to allocfunc_new\n");
	}

	s_info.alloc_canvas = dlsym(RTLD_DEFAULT, "ecore_evas_buffer_allocfunc_new");
	if (!s_info.alloc_canvas) {
		ErrPrint("No way to allocate canvas\n");
	}

	return WIDGET_ERROR_NONE;
}

PUBLIC int script_fini(void)
{
	int ret;
	Eina_List *l;
	Eina_List *n;
	struct info *handle;

	EINA_LIST_FOREACH_SAFE(s_info.handle_list, l, n, handle) {
		script_destroy(handle);
	}

	ret = system_settings_unset_changed_cb(SYSTEM_SETTINGS_KEY_FONT_SIZE);
	if (ret < 0) {
		DbgPrint("Unset font size change event callback: %d\n", ret);
	}

	ret = vconf_ignore_key_changed("db/setting/accessibility/font_name", font_changed_cb);
	if (ret < 0) {
		DbgPrint("Unset font name change event callback: %d\n", ret);
	}

	ret = vconf_ignore_key_changed(VCONFKEY_SETAPPL_ACCESSIBILITY_TTS, access_cb);
	if (ret < 0) {
		DbgPrint("Unset tts: %d\n", ret);
	}

	elm_shutdown();

	free(s_info.font_name);
	s_info.font_name = NULL;
	return WIDGET_ERROR_NONE;
}

/* End of a file */
