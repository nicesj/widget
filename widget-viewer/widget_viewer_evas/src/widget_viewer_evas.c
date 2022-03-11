/*
 * Samsung API
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Flora License, Version 1.1 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://floralicense.org/license/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <libintl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <Elementary.h>
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Evas.h>
#include <Edje.h>

#include <pkgmgr-info.h>
#include <system_info.h>

#include <widget_errno.h>
#include <widget_viewer.h>
#include <widget_viewer_internal.h>
#include <widget_conf.h>
#include <privilege_checker.h>

#include "util.h"

#if defined(LOG_TAG)
#undef LOG_TAG
#endif
#define LOG_TAG "WIDGET_EVAS"
#include <dlog.h>

#include "widget_viewer_evas.h"
#include "widget_viewer_evas_internal.h"

#if !defined(WIDGET_COUNT_OF_SIZE_TYPE)
	#define WIDGET_COUNT_OF_SIZE_TYPE 13
#endif

#if !defined(SECURE_LOGD)
#define SECURE_LOGD LOGD
#endif

#if !defined(SECURE_LOGW)
#define SECURE_LOGW LOGW
#endif

#if !defined(SECURE_LOGE)
#define SECURE_LOGE LOGE
#endif

#if !defined(S_)
#define S_(str) dgettext("sys_string", str)
#endif

#if !defined(T_)
#define T_(str) dgettext(PKGNAME, str)
#endif

#if !defined(N_)
#define N_(str) (str)
#endif

#if !defined(_)
#define _(str) gettext(str)
#endif

#if !defined(DbgPrint)
#define DbgPrint(format, arg...)	SECURE_LOGD(format, ##arg)
#endif

#if !defined(ErrPrint)
#define ErrPrint(format, arg...)	SECURE_LOGE(format, ##arg)
#endif

#if !defined(WarnPrint)
#define WarnPrint(format, arg...)	SECURE_LOGW(format, ##arg)
#endif

#if !defined(WIDGET_VIEWER_EVAS_RESOURCE_EDJ)
#define WIDGET_VIEWER_EVAS_RESOURCE_EDJ "/usr/share/widget_viewer_evas/res/edje/widget_viewer_evas.edj"
#endif

#if !defined(WIDGET_VIEWER_EVAS_UNKNOWN)
#define WIDGET_VIEWER_EVAS_UNKNOWN "/usr/share/widget_viewer_evas/res/image/unknown.png"
#endif

#if !defined(WIDGET_VIEWER_EVAS_RESOURCE_GBAR)
#define WIDGET_VIEWER_EVAS_RESOURCE_GBAR "gbar"
#endif

#if !defined(WIDGET_VIEWER_EVAS_RESOURCE_LB)
#define WIDGET_VIEWER_EVAS_RESOURCE_LB "widget"
#endif

#if !defined(WIDGET_VIEWER_EVAS_RESOURCE_IMG)
#define WIDGET_VIEWER_EVAS_RESOURCE_IMG "widget,image"
#endif

#if !defined(WIDGET_VIEWER_EVAS_RESOURCE_OVERLAY_LOADING)
#define WIDGET_VIEWER_EVAS_RESOURCE_OVERLAY_LOADING "overlay"
#endif

#define DEFAULT_OVERLAY_COUNTER 2
#define DEFAULT_OVERLAY_WAIT_TIME 1.0f
#define RESEVED_AREA_FOR_GBAR_EFFECT 100

#define WIDGET_CLASS_NAME "widget"

#define WIDGET_KEEP_BUFFER    -2

#define DEFAULT_CLUSTER "user,created"
#define DEFAULT_CATEGORY "default"

#define WIDGET_VIEWER_PRIVILEGE "http://tizen.org/privilege/widget.viewer"

/**
 * @note
 * Supported touch devices are limited to 32.
 * Because of count of bits of integer type. (32 bits)
 */
#define MAX_DEVICE 32

#define IS_PRESSED(info, device)	(((device) < MAX_DEVICE) ? (((info)->pressed & (0x01 << (device))) == (0x01 << (device))) : 0)

/**
 * @note
 * Short-Circuit
 */
#define SET_PRESSED(info, device)	((void)(((device) < MAX_DEVICE) && (((info)->pressed |= (0x01 << (device))))))
#define SET_RELEASED(info, device)	((void)(((device) < MAX_DEVICE) && (((info)->pressed &= (~(0x01 << (device)))))))

/*!
 * \note
 * Enable this to apply shadow effect to image object (for text widget)
 * #define SUPPORT_IMAGE_EFFECT
 */

int errno;

/*!
 * \note
 * Detect click event if the pointer does moved in this region (x , y < 5 pixels)
 */
#define CLICK_REGION WIDGET_CONF_CLICK_REGION

#if defined(__LP64__) || defined(_LP64)
#define PTR_TYPE(a) ((long long)(a))
#else
#define PTR_TYPE(a) ((int)(a))
#endif

static struct {
	Evas_Smart_Class sc;
	Evas_Smart *smart;
	Eina_List *list;
	struct {
		Eina_List *delete_list;
		Eina_List *create_list;
	} raw_event;
	int screen_width;
	int screen_height;

	union _conf {
		struct _field {
			unsigned int user_view_port:1;
			unsigned int force_to_buffer:1;
			unsigned int support_gbar:1;
			unsigned int manual_pause_resume:1;
			unsigned int use_fixed_size:1;
			unsigned int easy_mode:1;
			unsigned int is_scroll_x:1;
			unsigned int is_scroll_y:1;
			unsigned int auto_feed:1;
			unsigned int delayed_resume:1;
			unsigned int sensitive_move:1;
			unsigned int render_animator:1;
			unsigned int auto_render_selector:1;
			unsigned int skip_acquire:1;
			unsigned int disable_internal_scroller: 1;

			unsigned int reserved:17;
		} field;
		unsigned int mask;
	} conf;

	Evas_Object *win;
	Ecore_Animator *renderer;
	Eina_List *widget_dirty_objects;
	Eina_List *gbar_dirty_objects;
	Eina_List *subscribed_category_list;
	Eina_List *subscribed_group_list;
	Eina_List *updated_pixmap_list;
	Eina_List *updated_pixmap_lock_list;

	int initialized;
} s_info = {
	.sc = EVAS_SMART_CLASS_INIT_NAME_VERSION(WIDGET_CLASS_NAME),
	.smart = NULL,
	.list = NULL,
	.raw_event = {
		.delete_list = NULL,
		.create_list = NULL,
	},
	.conf = {
		.mask = 0,
	},
	.screen_width = 720,
	.screen_height = 1280,
	.win = NULL,
	.renderer = NULL,
	.widget_dirty_objects = NULL,
	.gbar_dirty_objects = NULL,
	.subscribed_category_list = NULL,
	.subscribed_group_list = NULL,
	.updated_pixmap_list = NULL,
	.updated_pixmap_lock_list = NULL,
	.initialized = 0,
};

struct subscribe_group {
	char *cluster;
	char *sub_cluster;
};

struct subscribe_category {
	char *category;
};

struct access_ret_cb_data {
	Evas_Object *obj;
	void (*ret_cb)(Evas_Object *, int, void *);
	void *data;
};

struct acquire_data {
	struct widget_data *data;
	Evas_Object *content;
	int w;
	int h;
};

enum CANCEL_CLICK {
	CANCEL_DISABLED = 0x0,
	CANCEL_USER = 0x01,
	CANCEL_PROCESSED = 0x02
};

struct preview_info {
	widget_size_type_e type;
	char *preview;
};

struct widget_data {
	enum {
		WIDGET_DATA_CREATED = 0x00beef00,
		WIDGET_DATA_DELETED = 0x0d0e0a0d,
	} state;
	struct widget *handle;
	Evas *e;
	Evas_Object *stage;	/*!< Do not resize this directly, it should be resized via XX_update_geometry */
	Evas_Object *parent;

	Evas_Object *widget_layout;	/*!< Layout of widget content part */
	Evas_Object *gbar_layout;	/*!< Layout of GBAR content part */

	Evas_Object *widget;	/*!< Container object */

	struct view_port {
		int x;
		int y;
		int w;
		int h;
	} view_port;

	char *widget_id;
	char *content;
	char *cluster;
	char *category;
	double period;

	void *widget_fb;
	void *gbar_fb;

	unsigned int gbar_pixmap;
	unsigned int widget_pixmap;

	unsigned int *gbar_extra;
	unsigned int *widget_extra;
	int gbar_extra_cnt;
	int widget_extra_cnt;

	int widget_latest_idx; /* -1 = primary buffer, 0 ~ = extra buffer */
	int gbar_latest_idx; /* -1 = primary buffer, 0 ~ = extra buffer */

	struct _down {
		int x;
		int y;

		struct _geo {
			int x;
			int y;
		} geo;

		int is_gbar;
	} down;

	int x[MAX_DEVICE];
	int y[MAX_DEVICE];

	int widget_width;
	int widget_height;
	int fixed_width;
	int fixed_height;
	widget_size_type_e size_type;
	unsigned int pressed;                     /**< Mouse is pressed */

	union {
		struct {
			unsigned int touch_effect:1;                /**< Requires to play touch effect */
			unsigned int mouse_event:1;                 /**< Requires to feed mouse event */
			unsigned int scroll_x:1;                    /**< */
			unsigned int scroll_y:1;
			unsigned int faulted:1;
			unsigned int flick_down:1;
			unsigned int gbar_created:1;

			unsigned int created:1;
			unsigned int deleted:1;
			unsigned int widget_pixmap_acquire_requested:1;
			unsigned int gbar_pixmap_acquire_requested:1;
			unsigned int cancel_scroll_x:1;
			unsigned int cancel_scroll_y:1;
			unsigned int cancel_click:2;
			unsigned int disable_preview:1;
			unsigned int disable_loading:1;
			unsigned int disable_text:1;
			unsigned int widget_overlay_loaded:1;
			unsigned int gbar_overlay_loaded:1;

			unsigned int freeze_visibility:1;

			unsigned int widget_dirty:1;
			unsigned int gbar_dirty:1;

			unsigned int send_delete:1;
			unsigned int permanent_delete:1;

			unsigned int delayed_resume:2;
			unsigned int initial_resumed:1;

			unsigned int disabled_overlay_animation:1;
			unsigned int extra_info_updated:1;

			unsigned int hide_overlay_manually:1;

			/**
			 * @note
			 * Do not raise up the overlay for faulted.
			 */
			unsigned int hide_faulted_overlay: 1;
		} field;	/* Do we really have the performance loss because of bit fields? */

		unsigned int flags;
	} is;

	int refcnt;
	int overlay_update_counter;
	Ecore_Timer *overlay_timer;
	widget_visibility_status_e freezed_visibility;

	Eina_List *gbar_script_object_list;
	Eina_List *widget_script_object_list;

	Ecore_Timer *delayed_resume_timer;

	Eina_List *preview_list;
};

struct script_object {
	char *id;
	Evas_Object *obj;
	Evas_Object *parent;    /* Swallowee object : Before delete 'obj', it should be unswallowed from this object */
};

enum EFFECT_MASK {
	EFFECT_NONE = 0x0,
	EFFECT_WIDTH = 0x01,
	EFFECT_HEIGHT = 0x02,
	EFFECT_BOTH = 0x03,
	EFFECT_MOVE = 0x10
};

struct animation_data {
	Ecore_Timer *timer;
	Evas_Object *obj;

	unsigned int effect_mask;
	int w;
	int h;
	int x;
	int y;
};

struct raw_event_cbdata {
	void (*cb)(struct widget_evas_raw_event_info *info, void *data);
	void *data;
};

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

static int widget_fault_handler(enum widget_fault_type fault, const char *pkgname, const char *filename, const char *funcname, void *data);
static int widget_event_handler(struct widget *handle, enum widget_event_type event, void *data);

static int widget_system_created(struct widget *handle, struct widget_data *data);
static void __widget_created_cb(struct widget *handle, int ret, void *cbdata);
static void __widget_overlay_loading(struct widget_data *data);
static void __widget_overlay_faulted(struct widget_data *data);
static void __widget_overlay_disable(struct widget_data *data, int no_timer, int ignore_update_count);

static void gbar_overlay_loading(struct widget_data *data);
static void gbar_overlay_disable(struct widget_data *data);

static void update_widget_geometry(struct acquire_data *acquire_data);
static void update_gbar_geometry(struct acquire_data *acquire_data);
static void update_stage_geometry(struct acquire_data *acquire_data);
static void animator_del_cb(void *cbdata, Evas *e, Evas_Object *obj, void *event_info);

static void remove_widget_dirty_object_list(struct widget_data *data);
static void remove_gbar_dirty_object_list(struct widget_data *data);
static void append_widget_dirty_object_list(struct widget_data *data, int idx);
static void append_gbar_dirty_object_list(struct widget_data *data, int idx);
static void __widget_event_widget_updated(struct widget_data *data);
static void __widget_event_gbar_updated(struct widget_data *data);

static inline bool is_widget_feature_enabled(void)
{
	static bool feature = false;
	static bool retrieved = false;

	if (retrieved == false) {
		int ret;

		ret = system_info_get_platform_bool("http://tizen.org/feature/shell.appwidget", &feature);
		if (ret != SYSTEM_INFO_ERROR_NONE) {
			ErrPrint("system_info: %d\n", ret);
		} else {
			retrieved = true;
		}
	}

	return feature;
}

static char *get_preview_image(struct widget_data *data, widget_size_type_e type)
{
	Eina_List *l;
	struct preview_info *info;
	char *preview;

	EINA_LIST_FOREACH(data->preview_list, l, info) {
		if (info->type == type) {
			preview = strdup(info->preview);
			if (!preview) {
				ErrPrint("strdup: %d\n", errno);
			}
			return preview;
		}
	}

	return widget_service_get_preview_image_path(data->widget_id, type);
}

static struct widget_data *get_smart_data(Evas_Object *widget)
{
	if (widget && evas_object_smart_type_check(widget, WIDGET_CLASS_NAME)) {
		struct widget_data *data;

		data = evas_object_smart_data_get(widget);
		if (data) {
			if (data->state == WIDGET_DATA_CREATED) {
				return data;
			}

			ErrPrint("smart data is not valid\n");
		} else {
			ErrPrint("smart data is not exists\n");
		}
	}

	return NULL;
}

static struct widget_data *get_smart_data_from_handle(widget_h handle)
{
	Evas_Object *widget;

	widget = widget_viewer_get_data(handle);
	if (!widget) {
		return NULL;
	}

	return get_smart_data(widget);
}

static Evas_Object *find_script_object(struct widget_data *data, int gbar, const char *id)
{
	Eina_List *script_object_list;
	struct script_object *so;
	Eina_List *l;

	if (gbar) {
		if (!data->gbar_layout) {
			return NULL;
		}

		script_object_list = data->gbar_script_object_list;
	} else {
		if (!data->widget_layout) {
			return NULL;
		}

		script_object_list = data->widget_script_object_list;
	}

	if (!id) {
		so = eina_list_nth(script_object_list, 0);
		return so ? so->obj : NULL;
	}

	EINA_LIST_FOREACH(script_object_list, l, so) {
		if (so->id && !strcmp(so->id, id)) {
			return so->obj;
		}
	}

	return NULL;
}

static void gbar_script_del_cb(void *cbdata, Evas *e, Evas_Object *obj, void *event_info)
{
	struct widget_data *data = cbdata;
	Eina_List *l;
	Eina_List *n;
	struct script_object *so;

	EINA_LIST_FOREACH_SAFE(data->gbar_script_object_list, l, n, so) {
		if (so->obj == obj) {
			data->gbar_script_object_list = eina_list_remove(data->gbar_script_object_list, so);
			free(so->id);
			free(so);
			break;
		}
	}
}

static void __widget_script_del_cb(void *cbdata, Evas *e, Evas_Object *obj, void *event_info)
{
	struct widget_data *data = cbdata;
	Eina_List *l;
	Eina_List *n;
	struct script_object *so;

	EINA_LIST_FOREACH_SAFE(data->widget_script_object_list, l, n, so) {
		if (so->obj == obj) {
			data->widget_script_object_list = eina_list_remove(data->widget_script_object_list, so);
			free(so->id);
			free(so);
			break;
		}
	}
}

static void script_signal_forwarder(void *cbdata, Evas_Object *obj, const char *signal_name, const char *source)
{
	struct widget_data *data = cbdata;
	struct widget_text_signal event_info = {
		.signal_name = signal_name,
		.source = source,
		.geometry = {
			.sx = 0.0f,
			.sy = 0.0f,
			.ex = 1.0f,
			.ey = 1.0f,
		}
	};

	widget_viewer_emit_text_signal(data->handle, &event_info, NULL, NULL);
}

static int append_script_object(struct widget_data *data, int gbar, const char *id, Evas_Object *parent, Evas_Object *child)
{
	struct script_object *so;

	so = malloc(sizeof(*so));
	if (!so) {
		ErrPrint("malloc: %d\n", errno);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	if (id) {
		so->id = strdup(id);
		if (!so->id) {
			ErrPrint("strdup: %d\n", errno);
			free(so);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}
	} else {
		so->id = NULL;
	}

	so->obj = child;
	so->parent = parent;

	if (gbar) {
		data->gbar_script_object_list = eina_list_append(data->gbar_script_object_list, so);
		evas_object_event_callback_add(child, EVAS_CALLBACK_DEL, gbar_script_del_cb, data);
	} else {
		data->widget_script_object_list = eina_list_append(data->widget_script_object_list, so);
		evas_object_event_callback_add(child, EVAS_CALLBACK_DEL, __widget_script_del_cb, data);
	}

	elm_object_signal_callback_add(child, "*", "*", script_signal_forwarder, data);
	return WIDGET_ERROR_NONE;
}

static inline void reset_scroller(struct widget_data *data)
{
	Evas_Object *scroller;
	int height = 0;
	int width = 0;

	if (!data->widget_layout) {
		return;
	}

	scroller = elm_object_part_content_get(data->widget_layout, "scroller");
	if (!scroller) {
		return;
	}

	if (s_info.conf.field.is_scroll_x) {
		height = data->widget_height >> 1;
	}

	if (s_info.conf.field.is_scroll_y) {
		width = data->widget_width >> 1;
	}

	elm_scroller_region_show(scroller, width, height, data->widget_width, data->widget_height);
}

static int invoke_raw_event_callback(enum widget_evas_raw_event_type type, const char *pkgname, Evas_Object *widget, int error)
{
	struct widget_evas_raw_event_info info;
	struct raw_event_cbdata *cbdata;
	Eina_List *l;
	Eina_List *n;
	int cnt = 0;

	info.pkgname = pkgname;
	info.widget = widget;
	info.error = error;
	info.type = type;

	switch (type) {
	case WIDGET_VIEWER_EVAS_RAW_DELETE:
		EINA_LIST_FOREACH_SAFE(s_info.raw_event.delete_list, l, n, cbdata) {
			if (cbdata->cb) {
				cbdata->cb(&info, cbdata->data);
				cnt++;
			}
		}
		break;
	case WIDGET_VIEWER_EVAS_RAW_CREATE:
		EINA_LIST_FOREACH_SAFE(s_info.raw_event.create_list, l, n, cbdata) {
			if (cbdata->cb) {
				cbdata->cb(&info, cbdata->data);
				cnt++;
			}
		}
		break;
	default:
		break;
	}

	return cnt;
}

static widget_size_type_e find_size_type(struct widget_data *data, int w, int h)
{
	int cnt = WIDGET_COUNT_OF_SIZE_TYPE;
	int i;
	int *_w = NULL;
	int *_h = NULL;
	widget_size_type_e type = WIDGET_SIZE_TYPE_UNKNOWN;
	int find;
	int ret_type = WIDGET_SIZE_TYPE_UNKNOWN;
	int delta;

	if (widget_service_get_supported_sizes(data->widget_id, &cnt, &_w, &_h) < 0) {
		ErrPrint("No available sizes: %s\n", data->widget_id);
		return WIDGET_SIZE_TYPE_UNKNOWN;
	}

	find = 0x7FFFFFFF;
	for (i = 0; i < cnt; i++) {
		widget_service_get_size_type(_w[i], _h[i], &type);

		if (!s_info.conf.field.easy_mode) {
			switch (type) {
			case WIDGET_SIZE_TYPE_EASY_1x1:
			case WIDGET_SIZE_TYPE_EASY_3x1:
			case WIDGET_SIZE_TYPE_EASY_3x3:
				continue;
			default:
				break;
			}
		} else {
			switch (type) {
			case WIDGET_SIZE_TYPE_EASY_1x1:
			case WIDGET_SIZE_TYPE_EASY_3x1:
			case WIDGET_SIZE_TYPE_EASY_3x3:
				break;
			default:
				continue;
			}
		}

		delta = abs(_w[i] - w) + abs(_h[i] - h);
		if (delta < find) {
			find = delta;
			ret_type = type;
		}
	}

	free(_w);
	free(_h);

	return ret_type;
}

static Eina_Bool effect_animator_cb(void *_data)
{
	struct animation_data *data = _data;
	int w;
	int h;
	int x;
	int y;
	int move_x = 0;
	int move_y = 0;

	evas_object_geometry_get(data->obj, &x, &y, &w, &h);
	if (data->w == w && data->h == h) {
		evas_object_event_callback_del(data->obj, EVAS_CALLBACK_DEL, animator_del_cb);
		evas_object_data_del(data->obj, "animation");
		free(data);
		return ECORE_CALLBACK_CANCEL;
	}

	if (data->effect_mask & EFFECT_WIDTH) {
		if (w < data->w) {
			if (data->w - w > RESEVED_AREA_FOR_GBAR_EFFECT) {
				w += 20;
				move_x = 20;
			} else if (data->w - w > 10) {
				w += 8;
				move_x = 8;
			} else {
				w++;
				move_x = 1;
			}
		} else if (w > data->w) {
			if (w - data->w > RESEVED_AREA_FOR_GBAR_EFFECT) {
				w -= 20;
				move_x = -20;
			} else if (w - data->w > 10) {
				w -= 8;
				move_x = -8;
			} else {
				w--;
				move_x = -1;
			}
		}
	} else {
		w = data->w;
	}

	if (data->effect_mask & EFFECT_HEIGHT) {
		if (h < data->h) {
			if (data->h - h > RESEVED_AREA_FOR_GBAR_EFFECT) {
				h += 20;
				move_y = 20;
			} else if (data->h - h > 10) {
				h += 8;
				move_y = 8;
			} else {
				h++;
				move_y = 1;
			}
		} else if (h > data->h) {
			if (h - data->h > RESEVED_AREA_FOR_GBAR_EFFECT) {
				h -= 20;
				move_y = -20;
			} else if (h - data->h > 10) {
				h -= 8;
				move_y = -8;
			} else {
				h--;
				move_y = -1;
			}
		}
	} else {
		h = data->h;
	}

	if (data->effect_mask & EFFECT_MOVE) {
		if (move_x) {
			x -= move_x;
		}
		if (move_y) {
			y -= move_y;
		}
		evas_object_move(data->obj, x, y);
	}

	evas_object_resize(data->obj, w, h);
	return ECORE_CALLBACK_RENEW;
}

static void animator_del_cb(void *cbdata, Evas *e, Evas_Object *obj, void *event_info)
{
	struct animation_data *data;

	data = evas_object_data_del(obj, "animation");
	if (data) {
		ecore_timer_del(data->timer);
		free(data);
	}
}

static void effect_size_get(Evas_Object *obj, int *w, int *h)
{
	struct animation_data *data;

	data = evas_object_data_get(obj, "animation");
	if (data) {
		*w = data->w;
		*h = data->h;
	} else {
		evas_object_geometry_get(obj, NULL, NULL, w, h);
	}
}

static void effect_resize(Evas_Object *obj, int w, int h, unsigned int effect_mask)
{
	struct animation_data *data;
	int ow;
	int oh;
	int ox;
	int oy;

	evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);

	data = evas_object_data_get(obj, "animation");
	if (data) {
		if (ow == w && oh == h) {
			evas_object_event_callback_del(obj, EVAS_CALLBACK_DEL, animator_del_cb);
			evas_object_data_del(obj, "animation");
			free(data);
			return;
		}
		/*!
		 * \note
		 * Update to new size
		 */
		data->w = w;
		data->h = h;
		return;
	} else if (ow == w && oh == h) {
		return;
	}

	data = malloc(sizeof(*data));
	if (!data) {
		ErrPrint("Heap: %d\n", errno);
		return;
	}

	data->obj = obj;
	data->w = w;
	data->h = h;
	data->x = ox;
	data->y = oy;
	data->effect_mask = effect_mask;

	data->timer = ecore_timer_add(1.0f/60.0f, effect_animator_cb, data);
	if (!data->timer) {
		free(data);
		return;
	}

	evas_object_data_set(obj, "animation", data);
	evas_object_event_callback_add(obj, EVAS_CALLBACK_DEL, animator_del_cb, data);
}

struct widget_data *widget_ref(struct widget_data *data)
{
	data->refcnt++;
	return data;
}

static void dump_handle_list(void)
{
	Eina_List *l = NULL;
	Eina_List *n;
	Evas_Object *widget;
	struct widget_data *data;

	DbgPrint("============== DUMP ===============");
	EINA_LIST_FOREACH_SAFE(s_info.list, l, n, widget) {
		data = evas_object_smart_data_get(widget);
		if (!data) {
			continue;
		}

		DbgPrint("data[%p] %s (%s)\n", data, data->widget_id, data->is.field.faulted ? "faulted" : "loaded");
	}
	DbgPrint("===================================");
}

struct widget_data *widget_unref(struct widget_data *data)
{
	Eina_List *l;
	Eina_List *n;
	struct preview_info *info;

	data->refcnt--;
	DbgPrint("refcnt: %d (%s)\n", data->refcnt, data->widget_id);
	if (data->refcnt != 0) {
		return data;
	}

	DbgPrint("Destroy widget data %p(%s)\n", data, data->widget_id);
	free(data->content);
	free(data->widget_id);
	free(data->cluster);
	free(data->category);

	EINA_LIST_FOREACH_SAFE(data->preview_list, l, n, info) {
		free(info->preview);
		free(info);
	}

	if (data->overlay_timer) {
		ecore_timer_del(data->overlay_timer);
		data->overlay_timer = NULL;
	}

	if (data->widget_layout) {
		Evas_Object *content;

		content = elm_object_part_content_unset(data->widget_layout, "widget,content");
		if (content) {
			Evas_Object *front;

			front = elm_object_part_content_unset(content, "front,content");
			if (front) {
				DbgPrint("Front image object is deleted\n");
				evas_object_del(front);
			}

			DbgPrint("Content object deleted\n");
			evas_object_del(content);
		}

		content = elm_object_part_content_unset(data->widget_layout, "overlay,content");
		if (content) {
			DbgPrint("Overlay is deleted\n");
			evas_object_del(content);
		}


		DbgPrint("Remove WIDGET Layout\n");
		evas_object_del(data->widget_layout);
	} else {
		DbgPrint("Layout is already deleted\n");
	}

	if (data->gbar_layout) {
		Evas_Object *content;
		content = elm_object_part_content_get(data->gbar_layout, "gbar,content");
		if (content) {
			Evas_Object *overlay;
			overlay = elm_object_part_content_unset(content, "overlay,content");
			if (overlay) {
				DbgPrint("Overlay is deleted\n");
				evas_object_del(overlay);
			}
		}
		DbgPrint("Remove GBAR Layout\n");
		evas_object_del(data->gbar_layout);
	} else {
		DbgPrint("GBar layout is already deleted\n");
	}

	if (data->stage) {
		DbgPrint("Remove Stage\n");
		evas_object_del(data->stage);
	} else {
		DbgPrint("Stage is already deleted\n");
	}

	if (data->widget_fb) {
		widget_viewer_release_buffer(data->widget_fb);
		data->widget_fb = NULL;
	}

	if (data->gbar_fb) {
		widget_viewer_release_buffer(data->gbar_fb);
		data->gbar_fb = NULL;
	}

	data->state = WIDGET_DATA_DELETED;
	free(data);
	dump_handle_list();
	return NULL;
}

static void gbar_multi_down_cb(void *cbdata, Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Multi_Down *down = event_info;
	struct widget_data *data = cbdata;
	struct widget_mouse_event_info minfo;
	Evas_Coord x, y, w, h;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	SET_PRESSED(data, down->device);

	evas_object_geometry_get(obj, &x, &y, &w, &h);

	if (data->size_type != WIDGET_SIZE_TYPE_UNKNOWN && !s_info.conf.field.use_fixed_size) {
		minfo.ratio_w = (double)data->fixed_width / (double)w;
		minfo.ratio_h = (double)data->fixed_height / (double)h;
	} else {
		minfo.ratio_w = 1.0f;
		minfo.ratio_h = 1.0f;
	}
	minfo.device = down->device;

	if (s_info.conf.field.auto_feed) {
		minfo.x = (double)x;
		minfo.y = (double)y;
		widget_viewer_feed_mouse_event(data->handle, WIDGET_GBAR_MOUSE_SET, &minfo);
	} else {
		minfo.x = (double)(down->canvas.x - x);
		minfo.y = (double)(down->canvas.y - y);

		widget_viewer_feed_mouse_event(data->handle, WIDGET_GBAR_MOUSE_ENTER, &minfo);
		widget_viewer_feed_mouse_event(data->handle, WIDGET_GBAR_MOUSE_DOWN, &minfo);
	}

	if (data->is.field.cancel_click == CANCEL_USER) {
		minfo.x = (double)(down->canvas.x - x);
		minfo.y = (double)(down->canvas.y - y);

		minfo.device = 0;
		widget_viewer_feed_mouse_event(data->handle, WIDGET_GBAR_MOUSE_ON_HOLD, &minfo);
		data->is.field.cancel_click = CANCEL_PROCESSED;
	}
}

static void gbar_down_cb(void *cbdata, Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Mouse_Down *down = event_info;
	struct widget_data *data = cbdata;
	struct widget_mouse_event_info minfo;
	Evas_Coord x, y, w, h;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	evas_object_geometry_get(obj, &data->down.geo.x, &data->down.geo.y, &w, &h);
	x = data->down.geo.x;
	y = data->down.geo.y;
	data->down.is_gbar = 1;

	data->x[0] = data->down.x = down->canvas.x;
	data->y[0] = data->down.y = down->canvas.y;
	SET_PRESSED(data, 0);
	if (s_info.conf.field.auto_render_selector) {
		DbgPrint("Change to direct render\n");
		s_info.conf.field.render_animator = 0;
	}

	/**
	 * In case of the GBar,
	 * There is no pre-defined size set.
	 * Every time its size will be notified to the provider app.
	 * So we don't need to handling the scale info of it.
	 */
	minfo.ratio_w = 1.0f;
	minfo.ratio_h = 1.0f;
	minfo.device = 0;

	if (s_info.conf.field.auto_feed) {
		minfo.x = (double)x;
		minfo.y = (double)y;
		widget_viewer_feed_mouse_event(data->handle, WIDGET_GBAR_MOUSE_SET, &minfo);
	} else {
		minfo.x = (double)(down->canvas.x - x);
		minfo.y = (double)(down->canvas.y - y);
		widget_viewer_feed_mouse_event(data->handle, WIDGET_GBAR_MOUSE_ENTER, &minfo);
		widget_viewer_feed_mouse_event(data->handle, WIDGET_GBAR_MOUSE_DOWN, &minfo);
	}
}

static void gbar_multi_move_cb(void *cbdata, Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Multi_Move *move = event_info;
	struct widget_data *data = cbdata;
	Evas_Coord x, y, w, h;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	if (!IS_PRESSED(data, move->device)) {
		return;
	}

	evas_object_geometry_get(obj, &x, &y, &w, &h);

	/**
	 * @note
	 * In this case, the ON_HOLD event is already sent.
	 * We don't need to check the cancel_state.
	 */
	if (!s_info.conf.field.auto_feed) {
		struct widget_mouse_event_info minfo;

		minfo.x = (double)(move->cur.canvas.x - x);
		minfo.y = (double)(move->cur.canvas.y - y);

		minfo.ratio_w = 1.0f;
		minfo.ratio_h = 1.0f;
		minfo.device = move->device;

		widget_viewer_feed_mouse_event(data->handle, WIDGET_GBAR_MOUSE_MOVE, &minfo);

		if (s_info.conf.field.auto_render_selector) {
			DbgPrint("Change to direct render\n");
			s_info.conf.field.render_animator = 0;
		}
	}
}

static void gbar_move_cb(void *cbdata, Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Mouse_Move *move = event_info;
	struct widget_data *data = cbdata;
	Evas_Coord x, y, w, h;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	if (!IS_PRESSED(data,0)) {
		return;
	}

	evas_object_geometry_get(obj, &x, &y, &w, &h);

	data->x[0] = move->cur.canvas.x;
	data->y[0] = move->cur.canvas.y;

	if (data->is.field.cancel_click == CANCEL_USER || !s_info.conf.field.auto_feed) {
		struct widget_mouse_event_info minfo;

		minfo.x = (double)(move->cur.canvas.x - x);
		minfo.y = (double)(move->cur.canvas.y - y);

		minfo.ratio_w = 1.0f;
		minfo.ratio_h = 1.0f;
		minfo.device = 0;

		if (!s_info.conf.field.auto_feed) {
			widget_viewer_feed_mouse_event(data->handle, WIDGET_GBAR_MOUSE_MOVE, &minfo);
		}

		if (data->is.field.cancel_click == CANCEL_USER) {
			widget_viewer_feed_mouse_event(data->handle, WIDGET_GBAR_MOUSE_ON_HOLD, &minfo);
			data->is.field.cancel_click = CANCEL_PROCESSED;
		}

		if (s_info.conf.field.auto_render_selector) {
			DbgPrint("Change to direct render\n");
			s_info.conf.field.render_animator = 0;
		}
	}
}

static void __widget_pixmap_del_cb(void *cbdata, Evas *e, Evas_Object *obj, void *event_info)
{
	struct widget_data *data = cbdata;

	if (data->widget_pixmap) {
		if (!s_info.conf.field.skip_acquire) {
			widget_viewer_release_resource_id(data->handle, 0, data->widget_pixmap);
		}
		data->widget_pixmap = 0;
	}

	if (data->widget_extra) {
		int idx;

		for (idx = 0; idx < widget_viewer_get_option(WIDGET_OPTION_EXTRA_BUFFER_CNT); idx++) {
			if (data->widget_extra[idx] != 0u) {
				if (!s_info.conf.field.skip_acquire) {
					if (widget_viewer_release_resource_id(data->handle, 0, data->widget_extra[idx]) < 0) {
						ErrPrint("Failed to release %u\n", data->widget_extra[idx]);
					}
				}
				data->widget_extra[idx] = 0u;
			}
		}

		free(data->widget_extra);
		data->widget_extra = NULL;
	}
}

static void gbar_pixmap_del_cb(void *cbdata, Evas *e, Evas_Object *obj, void *event_info)
{
	struct widget_data *data = cbdata;

	if (data->gbar_pixmap) {
		if (!s_info.conf.field.skip_acquire) {
			widget_viewer_release_resource_id(data->handle, 1, data->gbar_pixmap);
		}
		data->gbar_pixmap = 0;
	}

	if (data->gbar_extra) {
		int idx;

		for (idx = 0; idx < widget_viewer_get_option(WIDGET_OPTION_EXTRA_BUFFER_CNT); idx++) {
			if (data->gbar_extra[idx] != 0u) {
				if (!s_info.conf.field.skip_acquire) {
					if (widget_viewer_release_resource_id(data->handle, 0, data->gbar_extra[idx]) < 0) {
						ErrPrint("Failed to release %u\n", data->gbar_extra[idx]);
					}
				}
				data->gbar_extra[idx] = 0u;
			}
		}

		free(data->gbar_extra);
		data->gbar_extra = NULL;
	}
}

static void gbar_multi_up_cb(void *cbdata, Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Multi_Up *up = event_info;
	struct widget_data *data = cbdata;
	Evas_Coord x, y, w, h;
	struct widget_mouse_event_info minfo;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	if (!IS_PRESSED(data, up->device)) {
		return;
	}

	SET_RELEASED(data, up->device);

	evas_object_geometry_get(obj, &x, &y, &w, &h);

	minfo.ratio_w = 1.0f;
	minfo.ratio_h = 1.0f;
	minfo.device = up->device;

	if (s_info.conf.field.auto_feed) {
		/**
		 * @note
		 * UNSET will subtract object.x and object.y by master
		 * so we just send original touch position based on screen
		 */
		minfo.x = (double)up->canvas.x;
		minfo.y = (double)up->canvas.y;

		widget_viewer_feed_mouse_event(data->handle, WIDGET_GBAR_MOUSE_UNSET, &minfo);
	} else {
		minfo.x = (double)(up->canvas.x - x);
		minfo.y = (double)(up->canvas.y - y);

		widget_viewer_feed_mouse_event(data->handle, WIDGET_GBAR_MOUSE_UP, &minfo);
		widget_viewer_feed_mouse_event(data->handle, WIDGET_GBAR_MOUSE_LEAVE, &minfo);
	}
}

static void gbar_up_cb(void *cbdata, Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Mouse_Up *up = event_info;
	struct widget_data *data = cbdata;
	Evas_Coord x, y, w, h;
	struct widget_mouse_event_info minfo;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	if (!IS_PRESSED(data, 0)) {
		return;
	}

	SET_RELEASED(data, 0);

	evas_object_geometry_get(obj, &x, &y, &w, &h);

	minfo.ratio_w = 1.0f;
	minfo.ratio_h = 1.0f;
	minfo.device = 0;

	/**
	 * @note
	 * If the object is moved, we should send ON_HOLD event.
	 */
	if (data->down.geo.x != x || data->down.geo.y != y || data->is.field.cancel_click == CANCEL_USER) {
		if (data->is.field.cancel_click != CANCEL_PROCESSED) {
			minfo.x = (double)(up->canvas.x - x);
			minfo.y = (double)(up->canvas.y - y);
			widget_viewer_feed_mouse_event(data->handle, WIDGET_GBAR_MOUSE_ON_HOLD, &minfo);
			data->is.field.cancel_click = CANCEL_PROCESSED;
		}
	}

	if (s_info.conf.field.auto_feed) {
		/**
		 * @note
		 * UNSET will subtract object.x and object.y by master
		 * so we just send original touch position based on screen
		 */
		minfo.x = (double)up->canvas.x;
		minfo.y = (double)up->canvas.y;

		widget_viewer_feed_mouse_event(data->handle, WIDGET_GBAR_MOUSE_UNSET, &minfo);
	} else {
		minfo.x = (double)(up->canvas.x - x);
		minfo.y = (double)(up->canvas.y - y);

		widget_viewer_feed_mouse_event(data->handle, WIDGET_GBAR_MOUSE_UP, &minfo);
		widget_viewer_feed_mouse_event(data->handle, WIDGET_GBAR_MOUSE_LEAVE, &minfo);
	}

	data->is.field.cancel_click = CANCEL_DISABLED;
}

static void smart_callback_call(struct widget_data *data, const char *signal, void *cbdata)
{
	if (data->is.field.deleted || !data->widget) {
		DbgPrint("widget is deleted, ignore smart callback call\n");
		return;
	}

	evas_object_smart_callback_call(data->widget, signal, cbdata);
}

static void __widget_multi_down_cb(void *cbdata, Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Multi_Down *down = event_info;
	struct widget_data *data = cbdata;
	Evas_Coord x, y, w, h;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	data->x[down->device] = down->canvas.x;
	data->y[down->device] = down->canvas.y;

	evas_object_geometry_get(obj, &x, &y, &w, &h);

	/**
	 * @note
	 * If the multi-touch started,
	 * GBar will not be created.
	 */
	if (s_info.conf.field.support_gbar && data->is.field.flick_down) {
		struct widget_evas_event_info info;

		DbgPrint("Flick down is canceled\n");
		data->is.field.flick_down = 0;
		info.widget_app_id = data->widget_id;
		info.event = WIDGET_EVENT_GBAR_CREATED;
		info.error = WIDGET_ERROR_CANCELED;

		smart_callback_call(data, WIDGET_SMART_SIGNAL_FLICKDOWN_CANCELLED, &info);
	}

	if (s_info.conf.field.auto_render_selector) {
		DbgPrint("Change to direct render\n");
		s_info.conf.field.render_animator = 0;
	}

	SET_PRESSED(data, down->device);

	if (data->handle && !data->is.field.faulted) {
		struct widget_mouse_event_info minfo;

		if (!s_info.conf.field.use_fixed_size) {
			minfo.ratio_w = (double)data->fixed_width / (double)w;
			minfo.ratio_h = (double)data->fixed_height / (double)h;
		} else {
			minfo.ratio_w = 1.0f;
			minfo.ratio_h = 1.0f;
		}
		minfo.device = down->device;

		if (s_info.conf.field.auto_feed && data->is.field.mouse_event) {
			minfo.x = (double)data->down.geo.x;
			minfo.y = (double)data->down.geo.y;
			DbgPrint("[%d] %lfx%lf (%lfx%lf), %dx%d %dx%d\n", minfo.device, minfo.x, minfo.y, minfo.ratio_w, minfo.ratio_h, data->fixed_width, data->fixed_height, w, h);
			widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_SET, &minfo);
		} else {
			minfo.x = (double)(data->x[down->device] - x);
			minfo.y = (double)(data->y[down->device] - y);

			DbgPrint("[%d] %lfx%lf (%lfx%lf)\n", minfo.device, minfo.x, minfo.y, minfo.ratio_w, minfo.ratio_h);

			widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_ENTER, &minfo);
			widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_DOWN, &minfo);
			widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_MOVE, &minfo);
		}

		/**
		 * @note
		 * Send the ON_HOLD event if the multi-touch events are triggered.
		 */
		if (data->is.field.cancel_click != CANCEL_PROCESSED) {
			DbgPrint("ON_HOLD send\n");
			minfo.x = (double)(data->x[0] - x);
			minfo.y = (double)(data->y[0] - y);
			minfo.device = 0;
			widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_ON_HOLD, &minfo);
			data->is.field.cancel_click = CANCEL_PROCESSED;
		}
	}
}

static void __widget_down_cb(void *cbdata, Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Mouse_Down *down = event_info;
	struct widget_data *data = cbdata;
	Evas_Coord x, y, w, h;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	if (s_info.conf.field.support_gbar && !data->is.field.gbar_created) {
		data->is.field.flick_down = 1;
	}

	data->down.x = data->x[0] = down->canvas.x;
	data->down.y = data->y[0] = down->canvas.y;
	data->down.is_gbar = 0;
	data->is.field.scroll_x = 0;
	data->is.field.scroll_y = 0;
	data->is.field.cancel_scroll_x = 0;
	data->is.field.cancel_scroll_y = 0;

	if (s_info.conf.field.auto_render_selector) {
		DbgPrint("Change to direct render\n");
		s_info.conf.field.render_animator = 0;
	}

	evas_object_geometry_get(obj, &data->down.geo.x, &data->down.geo.y, &w, &h);
	x = data->down.geo.x;
	y = data->down.geo.y;

	if (s_info.conf.field.sensitive_move && (data->down.geo.x != data->view_port.x || data->down.geo.y != data->view_port.y)) {
		SET_RELEASED(data, 0);
		if (s_info.conf.field.auto_render_selector) {
			DbgPrint("Change to render animator\n");
			s_info.conf.field.render_animator = 1;
		}
		return;
	}

	SET_PRESSED(data, 0);

	if (data->handle && !data->is.field.faulted) {
		struct widget_mouse_event_info minfo;

		if (!s_info.conf.field.use_fixed_size) {
			minfo.ratio_w = (double)data->fixed_width / (double)w;
			minfo.ratio_h = (double)data->fixed_height / (double)h;
		} else {
			minfo.ratio_w = 1.0f;
			minfo.ratio_h = 1.0f;
		}
		minfo.device = 0;

		if (s_info.conf.field.auto_feed && data->is.field.mouse_event) {
			minfo.x = (double)x;
			minfo.y = (double)y;
			DbgPrint("%lfx%lf (%lfx%lf), %dx%d %dx%d\n", minfo.x, minfo.y, minfo.ratio_w, minfo.ratio_h, data->fixed_width, data->fixed_height, w, h);
			widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_SET, &minfo);
		} else {
			minfo.x = (double)(data->x[0] - x);
			minfo.y = (double)(data->y[0] - y);

			DbgPrint("%lfx%lf (%lfx%lf)\n", minfo.x, minfo.y, minfo.ratio_w, minfo.ratio_h);

			widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_ENTER, &minfo);
			widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_DOWN, &minfo);
			widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_MOVE, &minfo);
		}
	}
}

static void __widget_destroy_gbar_cb(struct widget *handle, int ret, void *cbdata)
{
	struct widget_data *data = cbdata;
	Evas_Object *gbar_content;
	struct widget_evas_event_info info;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	data->is.field.gbar_created = 0;

	info.error = ret;
	info.event = WIDGET_EVENT_GBAR_DESTROYED;
	info.widget_app_id = data->widget_id;
	smart_callback_call(data, WIDGET_SMART_SIGNAL_GBAR_DESTROYED, &info);

	DbgPrint("ret: %d\n", ret);
	gbar_content = elm_object_part_content_unset(data->gbar_layout, "gbar,content");
	if (gbar_content) {
		Evas_Native_Surface *surface;
		unsigned int pixmap;
		widget_type_e widget_type;

		widget_viewer_get_type(data->handle, 1, &widget_type);

		switch (widget_type) {
		case WIDGET_CONTENT_TYPE_RESOURCE_ID:
			if (!s_info.conf.field.force_to_buffer) {
				surface = evas_object_image_native_surface_get(gbar_content);
				if (!surface) {
					ErrPrint("surface is NULL\n");
					evas_object_del(gbar_content);
					break;
				}

				pixmap = util_get_resource_id_of_native_surface(surface);
				evas_object_del(gbar_content);

				if (!s_info.conf.field.skip_acquire) {
					widget_viewer_release_resource_id(data->handle, 1, (int)pixmap);
				}

				if (pixmap == data->gbar_pixmap) {
					data->gbar_pixmap = 0;
				}
				break;
			}
		case WIDGET_CONTENT_TYPE_BUFFER:
			if (data->gbar_fb) {
				widget_viewer_release_buffer(data->gbar_fb);
				data->gbar_fb = NULL;
			}
			evas_object_del(gbar_content);
			break;
		case WIDGET_CONTENT_TYPE_TEXT:
			break;
		case WIDGET_CONTENT_TYPE_UIFW:
			break;
		case WIDGET_CONTENT_TYPE_INVALID:
		default:
			break;
		}
	}

	if (data->gbar_layout) {
		evas_object_del(data->gbar_layout);
		data->gbar_layout = NULL;
	}

	remove_gbar_dirty_object_list(data);
	widget_unref(data);
}

static void gbar_animation_done_cb(void *cbdata, Evas_Object *obj, const char *signal_name, const char *source)
{
	Evas_Object *rect;
	struct widget_data *data = cbdata;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	rect = elm_object_part_content_unset(obj, "overlay,content");
	if (rect) {
		evas_object_del(rect);
	}
}

static void gbar_create_buffer_object(struct widget_data *data)
{
	Evas_Object *gbar_content;

	gbar_content = elm_object_part_content_get(data->gbar_layout, "gbar,content");
	if (!gbar_content) {
		gbar_content = evas_object_image_filled_add(data->e);
		if (!gbar_content) {
			ErrPrint("Failed to create an image object\n");
		} else {
			evas_object_image_colorspace_set(gbar_content, EVAS_COLORSPACE_ARGB8888);
			evas_object_image_alpha_set(gbar_content, EINA_TRUE);

			elm_object_part_content_set(data->gbar_layout, "gbar,content", gbar_content);
		}
	}
}

static int do_text_update_color(Evas_Object *layout, const char *part, const char *data)
{
	int ret;
	int r[3], g[3], b[3], a[3];
	ret = sscanf(data, "%d %d %d %d %d %d %d %d %d %d %d %d",
			r, g, b, a,			/* OBJECT */
			r + 1, g + 1, b + 1, a + 1,	/* OUTLINE */
			r + 2, g + 2, b + 2, a + 2);	/* SHADOW */
	if (ret != 12) {
		DbgPrint("part[%s] rgba[%s]\n", part, data);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	ret = edje_object_color_class_set(elm_layout_edje_get(layout), part,
			r[0], g[0], b[0], a[0], /* OBJECT */
			r[1], g[1], b[1], a[1], /* OUTLINE */
			r[2], g[2], b[2], a[2]); /* SHADOW */

	return WIDGET_ERROR_NONE;
}

static void update_focus_chain(Evas_Object *parent, Evas_Object *ao)
{
	const Eina_List *list;

	list = elm_object_focus_custom_chain_get(parent);
	if (!eina_list_data_find(list, ao)) {
		DbgPrint("Append again to the focus chain\n");
		elm_object_focus_custom_chain_append(parent, ao, NULL);
	}
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

static int do_text_update_access(Evas_Object *parent, Evas_Object *layout, const char *part, const char *text, const char *option)
{
	Evas_Object *edje_part;

	elm_object_part_text_set(layout, part, text ? text : "");

	edje_part = (Evas_Object *)edje_object_part_object_get(elm_layout_edje_get(layout), part);
	if (edje_part) {
		Evas_Object *ao;
		char *utf8;

		ao = evas_object_data_get(edje_part, "ao");
		if (!ao) {
			ao = elm_access_object_register(edje_part, parent);
			if (!ao) {
				ErrPrint("Unable to register an access object(%s)\n", part);
				return WIDGET_ERROR_NONE;
			}

			evas_object_data_set(edje_part, "ao", ao);
			elm_access_activate_cb_set(ao, activate_cb, NULL);
			elm_object_focus_custom_chain_append(parent, ao, NULL);

			DbgPrint("[%s] Register access info: (%s) to, %p\n", part, text, parent);
		}

		if (!text || !strlen(text)) {
			/*!
			 * \note
			 * Delete callback will be called
			 */
			DbgPrint("[%s] Remove access object(%p)\n", part, ao);
			elm_access_object_unregister(ao);

			return WIDGET_ERROR_NONE;
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

			return WIDGET_ERROR_NONE;
		}

		elm_access_info_set(ao, ELM_ACCESS_INFO, utf8);
		free(utf8);

		update_focus_chain(parent, ao);
	} else {
		ErrPrint("Unable to get text part[%s]\n", part);
	}

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

static inline void apply_shadow_effect(struct image_option *img_opt, Evas_Object *img)
{
#if defined(SUPPORT_IMAGE_EFFECT)
	ea_effect_h *ea_effect;

	if (!img_opt->shadow.enabled) {
		return;
	}

	ea_effect = ea_image_effect_create();
	if (!ea_effect) {
		return;
	}

	/* -90, 2, 4, 0x99000000 */
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

static int do_text_update_text(Evas_Object *parent, Evas_Object *layout, const char *part, const char *text)
{
	Evas_Object *edje_part;

	DbgPrint("Part[%s], Text[%s]\n", part, text);
	elm_object_part_text_set(layout, part, text ? text : "");

	edje_part = (Evas_Object *)edje_object_part_object_get(elm_layout_edje_get(layout), part);
	if (edje_part) {
		Evas_Object *ao;
		char *utf8;

		ao = evas_object_data_get(edje_part, "ao");
		if (!ao) {
			ao = elm_access_object_register(edje_part, parent);
			if (!ao) {
				ErrPrint("Unable to register an access object(%s)\n", part);
				return WIDGET_ERROR_NONE;
			}

			evas_object_data_set(edje_part, "ao", ao);
			elm_access_activate_cb_set(ao, activate_cb, NULL);
			elm_object_focus_custom_chain_append(parent, ao, NULL);

			DbgPrint("[%s] Register access info: (%s) to, %p\n", part, text, parent);
		}

		if (!text || !strlen(text)) {
			/*!
			 * \note
			 * Delete callback will be called
			 */
			DbgPrint("[%s] Remove access object(%p)\n", part, ao);
			elm_access_object_unregister(ao);

			return WIDGET_ERROR_NONE;
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

			return WIDGET_ERROR_NONE;
		}

		elm_access_info_set(ao, ELM_ACCESS_INFO, utf8);
		free(utf8);

		update_focus_chain(parent, ao);
	} else {
		ErrPrint("Unable to get text part[%s]\n", part);
	}

	return WIDGET_ERROR_NONE;
}

static int do_text_update_image(Evas_Object *layout, const char *part, const char *data, const char *option)
{
	Evas_Object *content;
	Evas_Load_Error err;
	Evas_Coord w;
	Evas_Coord h;
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

	content = elm_object_part_content_unset(layout, part);
	if (content) {
		evas_object_del(content);
	}

	if (!data || !strlen(data) || access(data, R_OK) != 0) {
		ErrPrint("Skip image: %s, deleted\n", part);
		return WIDGET_ERROR_NONE;
	}

	content = evas_object_image_add(evas_object_evas_get(layout));
	if (!content) {
		ErrPrint("Failed to add an image object\n");
		return WIDGET_ERROR_FAULT;
	}

	evas_object_image_preload(content, EINA_FALSE);
	parse_image_option(option, &img_opt);
	evas_object_image_load_orientation_set(content, img_opt.orient);

	evas_object_image_file_set(content, data, NULL);
	err = evas_object_image_load_error_get(content);
	if (err != EVAS_LOAD_ERROR_NONE) {
		ErrPrint("Load error: %s\n", evas_load_error_str(err));
		evas_object_del(content);
		return WIDGET_ERROR_IO_ERROR;
	}

	apply_shadow_effect(&img_opt, content);

	evas_object_image_size_get(content, &w, &h);
	if (img_opt.aspect) {
		if (img_opt.fill == FILL_OVER_SIZE) {
			Evas_Coord part_w;
			Evas_Coord part_h;

			if (img_opt.width >= 0 && img_opt.height >= 0) {
				part_w = img_opt.width;
				part_h = img_opt.height;
			} else {
				part_w = 0;
				part_h = 0;
				edje_object_part_geometry_get(elm_layout_edje_get(layout), part, NULL, NULL, &part_w, &part_h);
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
				evas_object_del(content);
				return WIDGET_ERROR_INVALID_PARAMETER;
			}

			content = crop_image(content, data, part_w, part_h, w, h, &img_opt);
		} else if (img_opt.fill == FILL_IN_SIZE) {
			Evas_Coord part_w;
			Evas_Coord part_h;

			if (img_opt.width >= 0 && img_opt.height >= 0) {
				part_w = img_opt.width * elm_config_scale_get();
				part_h = img_opt.height * elm_config_scale_get();
			} else {
				part_w = 0;
				part_h = 0;
				edje_object_part_geometry_get(elm_layout_edje_get(layout), part, NULL, NULL, &part_w, &part_h);
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
				evas_object_del(content);
				return WIDGET_ERROR_INVALID_PARAMETER;
			}

			content = crop_image(content, data, part_w, part_h, w, h, &img_opt);
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
				edje_object_part_geometry_get(elm_layout_edje_get(layout), part, NULL, NULL, &part_w, &part_h);
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
				evas_object_del(content);
				return WIDGET_ERROR_INVALID_PARAMETER;
			}

			content = crop_image(content, data, part_w, part_h, w, h, &img_opt);
		} else {
			evas_object_image_fill_set(content, 0, 0, w, h);
			evas_object_size_hint_fill_set(content, EVAS_HINT_FILL, EVAS_HINT_FILL);
			evas_object_size_hint_aspect_set(content, EVAS_ASPECT_CONTROL_BOTH, w, h);
		}

		apply_shadow_effect(&img_opt, content);
	} else {
		if (img_opt.width >= 0 && img_opt.height >= 0) {
			w = img_opt.width;
			h = img_opt.height;
			DbgPrint("Using given image size: %dx%d\n", w, h);
		}

		evas_object_image_fill_set(content, 0, 0, w, h);
		evas_object_size_hint_fill_set(content, EVAS_HINT_FILL, EVAS_HINT_FILL);
		evas_object_size_hint_weight_set(content, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
		evas_object_image_filled_set(content, EINA_TRUE);
	}


	/*!
	 * \note
	 * object will be shown by below statement automatically
	 */
	DbgPrint("%s part swallow image %p (%dx%d)\n", part, content, w, h);
	elm_object_part_content_set(layout, part, content);

	/*!
	 * \note
	 * This object is not registered as an access object.
	 * So the developer should add it to access list manually, using DESC_ACCESS block.
	 */
	return WIDGET_ERROR_NONE;
}

static int do_text_update_script(struct widget_data *data, int gbar, Evas_Object *layout, const char *id, const char *part, const char *file, const char *group)
{
	Evas_Object *content;
	int ret;

	content = elm_object_part_content_unset(layout, part);
	if (content) {
		evas_object_del(content);
	}

	if (!file || !strlen(file) || access(file, R_OK) != 0) {
		ErrPrint("path: %s (%d), Delete old object\n", file, errno);
		return WIDGET_ERROR_NONE;
	}

	content = elm_layout_add(layout);
	if (!content) {
		return WIDGET_ERROR_FAULT;
	}

	if (elm_layout_file_set(content, file, group) == EINA_FALSE) {
		evas_object_del(content);
		return WIDGET_ERROR_FAULT;
	}

	ret = append_script_object(data, gbar, id, layout, content);
	if (ret != WIDGET_ERROR_NONE) {
		evas_object_del(content);
		return ret;
	}

	elm_object_part_content_set(layout, part, content);
	/**
	 * @note
	 * Do we need to send all script event to the provider?
	 *
	 * elm_object_signal_callback_add(obj, "*", "*", script_signal_cb, handle);
	 */
	return WIDGET_ERROR_NONE;
}

static int do_text_operate_access(Evas_Object *layout, const char *part, const char *operation, const char *option)
{
	Elm_Access_Action_Info action_info;
	int ret;

	memset(&action_info, 0, sizeof(action_info));

	/* OPERATION is defined in libwidget package */
	if (!strcasecmp(operation, "set,hl")) {
		if (part) {
			Evas_Object *content;
			Evas_Coord x;
			Evas_Coord y;
			Evas_Coord w;
			Evas_Coord h;

			content = elm_object_part_content_get(layout, part);
			if (!content) {
				ErrPrint("Invalid part: %s\n", part);
				return WIDGET_ERROR_NONE;
			}

			evas_object_geometry_get(content, &x, &y, &w, &h);

			action_info.x = x + w / 2;
			action_info.y = x + h / 2;
		} else if (option && sscanf(option, "%dx%d", &action_info.x, &action_info.y) == 2) {
		} else {
			ErrPrint("Insufficient info for HL\n");
			return WIDGET_ERROR_NONE;
		}

		DbgPrint("TXxTY: %dx%d\n", action_info.x, action_info.y);
		ret = elm_access_action(layout, ELM_ACCESS_ACTION_HIGHLIGHT, &action_info);
		if (ret == EINA_FALSE) {
			ErrPrint("Action error\n");
		}
	} else if (!strcasecmp(operation, "unset,hl")) {
		ret = elm_access_action(layout, ELM_ACCESS_ACTION_UNHIGHLIGHT, &action_info);
		if (ret == EINA_FALSE) {
			ErrPrint("Action error\n");
		}
	} else if (!strcasecmp(operation, "next,hl")) {
		action_info.highlight_cycle = (!!option) && (!!strcasecmp(option, "no,cycle"));

		ret = elm_access_action(layout, ELM_ACCESS_ACTION_HIGHLIGHT_NEXT, &action_info);
		if (ret == EINA_FALSE) {
			ErrPrint("Action error\n");
		}
	} else if (!strcasecmp(operation, "prev,hl")) {
		action_info.highlight_cycle = EINA_TRUE;
		ret = elm_access_action(layout, ELM_ACCESS_ACTION_HIGHLIGHT_PREV, &action_info);
		if (ret == EINA_FALSE) {
			ErrPrint("Action error\n");
		}
	} else if (!strcasecmp(operation, "reset,focus")) {
		DbgPrint("Reset Focus\n");
		elm_object_focus_custom_chain_set(layout, NULL);
	}
	return WIDGET_ERROR_NONE;
}

static int gbar_text_update_begin(widget_h handle)
{
	struct widget_data *data;
	data = get_smart_data_from_handle(handle);
	if (!data) {
		return WIDGET_ERROR_FAULT;
	}

	DbgPrint("Begin text update: [%s]\n", data->widget_id);

	return WIDGET_ERROR_NONE;
}

static int gbar_text_update_end(widget_h handle)
{
	struct widget_data *data;
	data = get_smart_data_from_handle(handle);
	if (!data) {
		return WIDGET_ERROR_FAULT;
	}

	DbgPrint("End text update: [%s]\n", data->widget_id);

	return WIDGET_ERROR_NONE;
}

static int gbar_text_update_text(widget_h handle, const char *id, const char *part, const char *data)
{
	struct widget_data *widget_data;
	Evas_Object *layout;

	widget_data = get_smart_data_from_handle(handle);
	if (!widget_data) {
		return WIDGET_ERROR_FAULT;
	}

	layout = find_script_object(widget_data, 1, id);
	if (!layout) {
		ErrPrint("Target[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	return do_text_update_text(widget_data->parent, layout, part, data);
}

static int gbar_text_update_image(widget_h handle, const char *id, const char *part, const char *data, const char *option)
{
	struct widget_data *widget_data;
	Evas_Object *layout;

	widget_data = get_smart_data_from_handle(handle);
	if (!widget_data) {
		return WIDGET_ERROR_FAULT;
	}

	layout = find_script_object(widget_data, 1, id);
	if (!layout) {
		ErrPrint("Target[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	return do_text_update_image(layout, part, data, option);
}

static int gbar_text_update_script(widget_h handle, const char *id, const char *new_id, const char *part, const char *file, const char *group)
{
	struct widget_data *data;
	Evas_Object *layout;

	data = get_smart_data_from_handle(handle);
	if (!data) {
		return WIDGET_ERROR_FAULT;
	}

	layout = find_script_object(data, 1, id);
	if (!layout) {
		ErrPrint("Target[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	return do_text_update_script(data, 1, layout, new_id, part, file, group);
}

static int gbar_text_update_signal(widget_h handle, const char *id, const char *signal_name, const char *signal)
{
	struct widget_data *data;
	Evas_Object *layout;

	data = get_smart_data_from_handle(handle);
	if (!data) {
		return WIDGET_ERROR_FAULT;
	}

	layout = find_script_object(data, 1, id);
	if (!layout) {
		ErrPrint("Target[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	elm_object_signal_emit(layout, signal, signal_name);
	return WIDGET_ERROR_NONE;
}

static int gbar_text_update_drag(widget_h handle, const char *id, const char *part, double dx, double dy)
{
	struct widget_data *data;
	Evas_Object *layout;

	data = get_smart_data_from_handle(handle);
	if (!data) {
		return WIDGET_ERROR_FAULT;
	}

	layout = find_script_object(data, 1, id);
	if (!layout) {
		ErrPrint("Target[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	edje_object_part_drag_value_set(elm_layout_edje_get(layout), part, dx, dy);
	return WIDGET_ERROR_NONE;
}

static int gbar_text_update_info_size(widget_h handle, const char *id, int w, int h)
{
	struct widget_data *data;
	Evas_Object *layout;

	data = get_smart_data_from_handle(handle);
	if (!data) {
		return WIDGET_ERROR_FAULT;
	}

	layout = find_script_object(data, 1, id);
	if (!layout) {
		ErrPrint("Target[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	DbgPrint("Resize to %dx%d\n", w, h);
	evas_object_resize(layout, w, h);

	return WIDGET_ERROR_NONE;
}

static int gbar_text_update_info_category(widget_h handle, const char *id, const char *category)
{
	struct widget_data *data;
	Evas_Object *layout;

	data = get_smart_data_from_handle(handle);
	if (!data) {
		return WIDGET_ERROR_FAULT;
	}

	layout = find_script_object(data, 1, id);
	if (!layout) {
		ErrPrint("Target[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	DbgPrint("Update category: %s\n", category);

	return WIDGET_ERROR_NONE;
}

static int gbar_text_update_access(widget_h handle, const char *id, const char *part, const char *text, const char *option)
{
	struct widget_data *data;
	Evas_Object *layout;

	data = get_smart_data_from_handle(handle);
	if (!data) {
		return WIDGET_ERROR_FAULT;
	}

	layout = find_script_object(data, 1, id);
	if (!layout) {
		ErrPrint("Target[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	return do_text_update_access(data->parent, layout, part, text, option);
}

static int gbar_text_operate_access(widget_h handle, const char *id, const char *part, const char *operation, const char *option)
{
	struct widget_data *data;
	Evas_Object *layout;

	data = get_smart_data_from_handle(handle);
	if (!data) {
		return WIDGET_ERROR_FAULT;
	}

	layout = find_script_object(data, 1, id);
	if (!layout) {
		ErrPrint("Target[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	return do_text_operate_access(layout, part, operation, option);
}

static int gbar_text_update_color(widget_h handle, const char *id, const char *part, const char *data)
{
	struct widget_data *widget_data;
	Evas_Object *layout;

	widget_data = get_smart_data_from_handle(handle);
	if (!widget_data) {
		return WIDGET_ERROR_FAULT;
	}

	layout = find_script_object(widget_data, 1, id);
	if (!layout) {
		ErrPrint("Target[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	return do_text_update_color(layout, part, data);
}

static void gbar_create_text_object(struct widget_data *data)
{
	Evas_Object *gbar_content;

	gbar_content = elm_object_part_content_get(data->gbar_layout, "gbar,content");
	if (!gbar_content) {
		char *script_file;
		char *script_group;
		struct widget_script_operators operator = {
			.update_begin = gbar_text_update_begin,
			.update_end = gbar_text_update_end,

			.update_text = gbar_text_update_text,
			.update_image = gbar_text_update_image,
			.update_script = gbar_text_update_script,
			.update_signal = gbar_text_update_signal,
			.update_drag = gbar_text_update_drag,
			.update_info_size = gbar_text_update_info_size,
			.update_info_category = gbar_text_update_info_category,
			.update_access = gbar_text_update_access,
			.operate_access = gbar_text_operate_access,
			.update_color = gbar_text_update_color,
		};
		int ret;

		gbar_content = elm_layout_add(data->gbar_layout);
		if (!gbar_content) {
			ErrPrint("Failed to create a layout object\n");
			return;
		}

		script_file = widget_service_get_gbar_script_path(data->widget_id);
		script_group = widget_service_get_gbar_script_group(data->widget_id);
		if (!script_file || !script_group) {
			ErrPrint("Invalid script info ([%s] - [%s])\n", script_file, script_group);
			free(script_file);
			free(script_group);
			evas_object_del(gbar_content);
			return;
		}

		if (access(script_file, R_OK) != 0) {
			ErrPrint("Unable to access [%s] - %d\n", script_file, errno);
			free(script_file);
			free(script_group);
			evas_object_del(gbar_content);
			return;
		}

		ret = elm_layout_file_set(gbar_content, script_file, script_group);
		DbgPrint("Load (%s)(%s)\n", script_file, script_group);
		free(script_file);
		free(script_group);
		if (ret == EINA_FALSE) {
			ErrPrint("Failed to load an edje file\n");
			evas_object_del(gbar_content);
			return;
		}

		if (widget_viewer_set_text_handler(data->handle, 1, &operator) != WIDGET_ERROR_NONE) {
			ErrPrint("Failed to set text handler for [%s]\n", data->widget_id);
			evas_object_del(gbar_content);
			return;
		}

		ret = append_script_object(data, 1, NULL, NULL, gbar_content);
		if (ret != WIDGET_ERROR_NONE) {
			ErrPrint("Failed to append this to script object list\n");
			evas_object_del(gbar_content);
			return;
		}

		elm_object_part_content_set(data->gbar_layout, "gbar,content", gbar_content);
	}
}

static void gbar_create_pixmap_object(struct widget_data *data)
{
	Evas_Object *gbar_content;

	gbar_content = elm_object_part_content_get(data->gbar_layout, "gbar,content");
	if (!gbar_content) {
		gbar_content = evas_object_image_filled_add(data->e);
		if (!gbar_content) {
			ErrPrint("Failed to create an image object\n");
			return;
		}

		evas_object_image_colorspace_set(gbar_content, EVAS_COLORSPACE_ARGB8888);
		evas_object_image_alpha_set(gbar_content, EINA_TRUE);
		elm_object_part_content_set(data->gbar_layout, "gbar,content", gbar_content);

		evas_object_event_callback_add(gbar_content, EVAS_CALLBACK_MOUSE_DOWN, gbar_down_cb, data);
		evas_object_event_callback_add(gbar_content, EVAS_CALLBACK_MOUSE_MOVE, gbar_move_cb, data);
		evas_object_event_callback_add(gbar_content, EVAS_CALLBACK_MOUSE_UP, gbar_up_cb, data);

		evas_object_event_callback_add(gbar_content, EVAS_CALLBACK_MULTI_DOWN, gbar_multi_down_cb, data);
		evas_object_event_callback_add(gbar_content, EVAS_CALLBACK_MULTI_MOVE, gbar_multi_move_cb, data);
		evas_object_event_callback_add(gbar_content, EVAS_CALLBACK_MULTI_UP, gbar_multi_up_cb, data);

		evas_object_event_callback_add(gbar_content, EVAS_CALLBACK_DEL, gbar_pixmap_del_cb, data);
	}
}

static void gbar_layout_del_cb(void *_data, Evas *e, Evas_Object *obj, void *event_info)
{
	struct widget_data *data = _data;
	DbgPrint("GBar layout is deleted(%p - %p)\n", data->gbar_layout, obj);
	/**
	 * @note
	 * We should not access the data->gbar_layout in this function.
	 * We can only believe the "obj" now..
	 */
	evas_object_smart_member_del(obj);
	evas_object_clip_unset(obj);
	data->gbar_layout = NULL;
}

static void __widget_create_gbar_cb(struct widget *handle, int ret, void *cbdata)
{
	struct widget_data *data = cbdata;
	struct widget_evas_event_info info;
	widget_type_e widget_type;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	if (ret != WIDGET_ERROR_NONE) {
		DbgPrint("Create GBAR: 0x%X\n", ret);
		info.error = ret;
		info.event = WIDGET_EVENT_GBAR_CREATED;
		info.widget_app_id = data->widget_id;
		smart_callback_call(data, WIDGET_SMART_SIGNAL_GBAR_ABORTED, &info);
		widget_unref(data);
		return;
	}

	if (data->is.field.deleted) {
		/**
		 * Evas Object is deleted.
		 * Do not proceed this process anymore and destroy GBAR too
		 */
		if (widget_viewer_destroy_glance_bar(data->handle, NULL, NULL) != WIDGET_ERROR_NONE) {
			DbgPrint("widget_viewer_destroy_glance_bar failed\n");
		}
		return;
	}

	DbgPrint("GBAR is created\n");

	if (!data->gbar_layout) {
		data->gbar_layout = elm_layout_add(data->parent);
		if (!data->gbar_layout) {
			ErrPrint("Failed to add an edje\n");
			info.error = WIDGET_ERROR_FAULT;
			info.event = WIDGET_EVENT_GBAR_CREATED;
			info.widget_app_id = data->widget_id;
			smart_callback_call(data, WIDGET_SMART_SIGNAL_GBAR_ABORTED, &info);

			ret = widget_viewer_destroy_glance_bar(data->handle, __widget_destroy_gbar_cb, widget_ref(data));
			if (ret < 0) {
				/*!
				 * \note
				 *       PREVENT detect this. but it is FALSE POSITIVE
				 *
				 * widget_ref will increase the refcnt of data.
				 * and it is called when calling the widget_destroy_glance_bar function (via the last param)
				 * So this function call will not release the data.
				 */
				widget_unref(data);
			}

			widget_unref(data);
			return;
		}
		evas_object_event_callback_add(data->gbar_layout, EVAS_CALLBACK_DEL, gbar_layout_del_cb, data);

		if (elm_layout_file_set(data->gbar_layout, WIDGET_VIEWER_EVAS_RESOURCE_EDJ, WIDGET_VIEWER_EVAS_RESOURCE_GBAR) == EINA_FALSE) {
			ErrPrint("Failed to load edje object: %s(%s)\n", WIDGET_VIEWER_EVAS_RESOURCE_EDJ, WIDGET_VIEWER_EVAS_RESOURCE_GBAR);
			evas_object_del(data->gbar_layout);
			data->gbar_layout = NULL;

			info.error = WIDGET_ERROR_IO_ERROR;
			info.event = WIDGET_EVENT_GBAR_CREATED;
			info.widget_app_id = data->widget_id;
			smart_callback_call(data, WIDGET_SMART_SIGNAL_GBAR_ABORTED, &info);

			ret = widget_viewer_destroy_glance_bar(data->handle, __widget_destroy_gbar_cb, widget_ref(data));
			if (ret < 0) {
				/*!
				 * \note
				 *       PREVENT detect this. but it is FALSE POSITIVE
				 *
				 * widget_ref will increase the refcnt of data.
				 * and it is called when calling the widget_destroy_glance_bar function (via the last param)
				 * So this function call will not release the data.
				 */
				widget_unref(data);
			}

			widget_unref(data);
			return;
		}

		evas_object_smart_member_add(data->gbar_layout, data->widget);
		evas_object_clip_set(data->gbar_layout, data->stage);
		elm_object_signal_callback_add(data->gbar_layout, "finished", "animation", gbar_animation_done_cb, data);
		evas_object_show(data->gbar_layout);
	}
	gbar_overlay_loading(data);

	widget_viewer_get_type(data->handle, 1, &widget_type);

	switch (widget_type) {
	case WIDGET_CONTENT_TYPE_RESOURCE_ID:
		if (!s_info.conf.field.force_to_buffer) {
			gbar_create_pixmap_object(data);
			break;
		}
	case WIDGET_CONTENT_TYPE_BUFFER:
		gbar_create_buffer_object(data);
		break;
	case WIDGET_CONTENT_TYPE_TEXT:
		gbar_create_text_object(data);
		break;
	case WIDGET_CONTENT_TYPE_UIFW:
		ErrPrint("Not implemented - TYPE_UIFW for GBAR\n");
	default:
		info.error = WIDGET_ERROR_INVALID_PARAMETER;
		info.event = WIDGET_EVENT_GBAR_CREATED;
		info.widget_app_id = data->widget_id;
		ret = widget_viewer_destroy_glance_bar(data->handle, __widget_destroy_gbar_cb, widget_ref(data));
		if (ret < 0) {
			/*!
			 * \note
			 *       PREVENT detect this. but it is FALSE POSITIVE
			 *
			 * widget_ref will increase the refcnt of data.
			 * and it is called when calling the widget_destroy_glance_bar function (via the last param)
			 * So this function call will not release the data.
			 */
			data = widget_unref(data);
		}
		ErrPrint("Failed to create a GBAR, unknown type\n");
		if (data) {
			smart_callback_call(data, WIDGET_SMART_SIGNAL_GBAR_ABORTED, &info);
			data = widget_unref(data);
		}
		return;
	}

	data->is.field.gbar_created = 1;
	info.error = WIDGET_ERROR_NONE;
	info.event = WIDGET_EVENT_GBAR_CREATED;
	info.widget_app_id = data->widget_id;
	smart_callback_call(data, WIDGET_SMART_SIGNAL_GBAR_CREATED, &info);
	widget_unref(data);
}

static void update_scroll_flag(struct widget_data *data, int x, int y)
{
	if (s_info.conf.field.is_scroll_x && !s_info.conf.field.is_scroll_y) {
		if (!data->is.field.scroll_x && abs(y - data->down.y) > CLICK_REGION && (abs(x - data->down.x) <= CLICK_REGION)) {
			data->is.field.cancel_scroll_x = 1;
		}
	}

	if (s_info.conf.field.is_scroll_y && !s_info.conf.field.is_scroll_x) {
		if (!data->is.field.scroll_y && abs(x - data->down.x) > CLICK_REGION && (abs(y - data->down.y) <= CLICK_REGION)) {
			data->is.field.cancel_scroll_y = 1;
		}
	}

	data->is.field.scroll_x = s_info.conf.field.is_scroll_x && (!!(data->is.field.scroll_x || (abs(x - data->down.x) > CLICK_REGION)));
	data->is.field.scroll_y = s_info.conf.field.is_scroll_y && (!!(data->is.field.scroll_y || (abs(y - data->down.y) > CLICK_REGION)));
	data->is.field.scroll_x = !data->is.field.cancel_scroll_x && data->is.field.scroll_x;
	data->is.field.scroll_y = !data->is.field.cancel_scroll_y && data->is.field.scroll_y;
}

static void __widget_multi_up_cb(void *cbdata, Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Multi_Up *up = event_info;
	struct widget_data *data = cbdata;
	Evas_Coord x, y, w, h;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	if (s_info.conf.field.auto_render_selector) {
		DbgPrint("Change to render animator\n");
		s_info.conf.field.render_animator = 1;
	}

	if (!IS_PRESSED(data, up->device)) {
		return;
	}

	SET_RELEASED(data, up->device);

	if (data->handle && !data->is.field.faulted) {
		struct widget_mouse_event_info minfo;

		data->x[up->device] = up->canvas.x;
		data->y[up->device] = up->canvas.y;

		evas_object_geometry_get(data->widget, &x, &y, &w, &h);

		if (data->size_type != WIDGET_SIZE_TYPE_UNKNOWN && !s_info.conf.field.use_fixed_size) {
			minfo.ratio_w = (double)data->fixed_width / (double)w;
			minfo.ratio_h = (double)data->fixed_height / (double)h;
			DbgPrint("Width: %d/%d - Height: %d/%d\n", data->fixed_width, w, data->fixed_height, h);
		} else {
			minfo.ratio_w = 1.0f;
			minfo.ratio_h = 1.0f;
		}
		DbgPrint("Ratio: %lfx%lf\n", minfo.ratio_w, minfo.ratio_h);
		minfo.device = up->device;

		if (s_info.conf.field.auto_feed && data->is.field.mouse_event) {
			/**
			 * @note
			 * UNSET will subtract object.x and object.y by master
			 * so we just send original touch position based on screen
			 */
			minfo.x = (double)data->x[up->device];
			minfo.y = (double)data->y[up->device];
			DbgPrint("X,Y = %lfx%lf\n", minfo.x, minfo.y);

			widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_UNSET, &minfo);
		} else {
			/* We have to keep the first position of touch down */
			minfo.x = (double)(data->x[up->device] - x);
			minfo.y = (double)(data->y[up->device] - y);

			DbgPrint("%lfx%lf (%lfx%lf)\n", minfo.x, minfo.y, minfo.ratio_w, minfo.ratio_h);
			widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_UP, &minfo);
			widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_LEAVE, &minfo);
		}
	}
}

static void __widget_up_cb(void *cbdata, Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Mouse_Up *up = event_info;
	struct widget_evas_event_info info;
	struct widget_data *data = cbdata;
	Evas_Coord x, y, w, h;
	int ret = 0;
	widget_size_type_e size_type;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	if (!IS_PRESSED(data, 0)) {
		return;
	}

	update_scroll_flag(data, up->canvas.x, up->canvas.y);

	data->x[0] = up->canvas.x;
	data->y[0] = up->canvas.y;
	SET_RELEASED(data, 0);

	if (s_info.conf.field.auto_render_selector) {
		DbgPrint("Change to render animator\n");
		s_info.conf.field.render_animator = 1;
	}

	info.widget_app_id = data->widget_id;
	info.event = WIDGET_EVENT_GBAR_CREATED;

	if (s_info.conf.field.support_gbar && data->is.field.flick_down && (data->y[0] - data->down.y) < CLICK_REGION) {
		DbgPrint("Flick down is canceled\n");
		data->is.field.flick_down = 0;
		info.error = WIDGET_ERROR_CANCELED;
		smart_callback_call(data, WIDGET_SMART_SIGNAL_FLICKDOWN_CANCELLED, &info);
	}

	evas_object_geometry_get(obj, &x, &y, &w, &h);

	if (data->is.field.flick_down) {
		data->is.field.flick_down = 0;
		if (!data->handle || data->is.field.faulted || !widget_viewer_has_glance_bar(data->handle)) {
			if (!data->is.field.gbar_created && s_info.conf.field.support_gbar) {
				elm_object_signal_emit(data->widget_layout, "tilt", "content");
			}
			DbgPrint("Flick down is canceled\n");
			info.error = WIDGET_ERROR_CANCELED;
			smart_callback_call(data, WIDGET_SMART_SIGNAL_FLICKDOWN_CANCELLED, &info);
		} else if (s_info.conf.field.support_gbar && !data->is.field.gbar_created) {
			double rx;
			double ry;
			int gbar_w;
			int gbar_h;

			if (widget_viewer_get_glance_bar_size(data->handle, &gbar_w, &gbar_h) != WIDGET_ERROR_NONE) {
				gbar_w = 0;
				gbar_h = 0;
			}

			elm_object_signal_emit(data->widget_layout, "move,down", "content");

			rx = ((double)x + (double)w / 2.0f) / (double)s_info.screen_width;
			DbgPrint("x[%d], w[%d], rx[%lf]\n", x, w, rx);
			/* 0.0    0.125    0.25    0.375   0.5   0.625   0.75    0.875   1.0 */
			widget_viewer_get_size_type(data->handle, &size_type);
			switch (size_type) {
			case WIDGET_SIZE_TYPE_1x1:
				if (rx < 0.25f) {
					rx = 0.125f;
				} else if (rx < 0.5f) {
					rx = 0.375f;
				} else if (rx < 0.75f) {
					rx = 0.625f;
				} else if (rx < 1.0f) {
					rx = 0.875f;
				}
				break;
			case WIDGET_SIZE_TYPE_2x1:
			case WIDGET_SIZE_TYPE_2x2:
				if (rx < 0.5f) {
					rx = 0.25f;
				} else if (rx < 0.625f) {
					rx = 0.5f;
				} else {
					rx = 0.75f;
				}
				break;
			default:
				rx = 0.5f;
				break;
			}

			if (y + h + gbar_h <= s_info.screen_height) {
				ry = 0.0f;
			} else {
				ry = 1.0f;
			}

			DbgPrint("converted rx[%lf], ry[%lf]\n", rx, ry);

			ret = widget_viewer_create_glance_bar(data->handle, rx, ry, __widget_create_gbar_cb, widget_ref(data));
			if (ret < 0) {
				widget_unref(data);
				DbgPrint("Flick down is canceled\n");
				info.error = WIDGET_ERROR_CANCELED;
				smart_callback_call(data, WIDGET_SMART_SIGNAL_FLICKDOWN_CANCELLED, &info);
			}
			DbgPrint("Create GBAR: %d (%lfx%lf)\n", ret, rx, ry);
		}
	}

	if (data->handle && !data->is.field.faulted) {
		struct widget_mouse_event_info minfo;

		if (data->size_type != WIDGET_SIZE_TYPE_UNKNOWN && !s_info.conf.field.use_fixed_size) {
			minfo.ratio_w = (double)data->fixed_width / (double)w;
			minfo.ratio_h = (double)data->fixed_height / (double)h;
		} else {
			minfo.ratio_w = 1.0f;
			minfo.ratio_h = 1.0f;
		}
		minfo.device = 0;

		reset_scroller(data);

		if (data->is.field.cancel_click != CANCEL_PROCESSED) {
			if (data->down.geo.x != x || data->down.geo.y != y ||
				data->is.field.scroll_x || data->is.field.scroll_y ||
				data->is.field.cancel_click == CANCEL_USER ||
				(!data->is.field.mouse_event && (abs(data->x[0] - data->down.x) > CLICK_REGION || abs(data->y[0] - data->down.y) > CLICK_REGION)) ||
				(up->event_flags & EVAS_EVENT_FLAG_ON_HOLD) == EVAS_EVENT_FLAG_ON_HOLD)
			{
				minfo.x = (double)(data->x[0] - x);
				minfo.y = (double)(data->y[0] - y);
				widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_ON_HOLD, &minfo);
				data->is.field.cancel_click = CANCEL_PROCESSED;
			}
		}

		if (s_info.conf.field.auto_feed && data->is.field.mouse_event) {
			/**
			 * @note
			 * UNSET will subtract object.x and object.y by master
			 * so we just send original touch position based on screen
			 */
			minfo.x = (double)data->x[0];
			minfo.y = (double)data->y[0];
			DbgPrint("X,Y = %lfx%lf\n", minfo.x, minfo.y);
			widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_UNSET, &minfo);
		} else {
			minfo.x = (double)(data->x[0] - x);
			minfo.y = (double)(data->y[0] - y);

			DbgPrint("%lfx%lf (%lfx%lf)\n", minfo.x, minfo.y, minfo.ratio_w, minfo.ratio_h);
			widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_UP, &minfo);
			widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_LEAVE, &minfo);
		}

		if (!data->is.field.flick_down) {
			ret = WIDGET_ERROR_INVALID_PARAMETER;
			if (data->is.field.gbar_created) {
				ret = widget_viewer_destroy_glance_bar(data->handle, __widget_destroy_gbar_cb, widget_ref(data));
				if (ret < 0) {
					data = widget_unref(data);
				}
			} else if (data->is.field.cancel_click == CANCEL_DISABLED) {
				ret = widget_viewer_send_click_event(data->handle, WIDGET_VIEWER_CLICK_BUTTON_LEFT, minfo.x, minfo.y);
			}
		}

		if (data) {
			DbgPrint("Up: %lfx%lf [x:%d/%d/%d] [y:%d/%d/%d], ret: 0x%X, cancel: 0x%x\n",
					minfo.x, minfo.y,
					data->is.field.scroll_x, s_info.conf.field.is_scroll_x, data->is.field.cancel_scroll_x,
					data->is.field.scroll_y, s_info.conf.field.is_scroll_y, data->is.field.cancel_scroll_y,
					ret, data->is.field.cancel_click);
			data->is.field.cancel_click = CANCEL_DISABLED;
		}
	}
}

static void __widget_multi_move_cb(void *cbdata, Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Multi_Move *move = event_info;
	struct widget_data *data = cbdata;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	if (!IS_PRESSED(data, move->device)) {
		return;
	}

	data->x[move->device] = move->cur.canvas.x;
	data->y[move->device] = move->cur.canvas.y;

	if (data->handle && !data->is.field.faulted) {
		if (!s_info.conf.field.auto_feed && data->is.field.mouse_event) {
			struct widget_mouse_event_info minfo;
			Evas_Coord x, y, w, h;

			evas_object_geometry_get(obj, &x, &y, &w, &h);

			minfo.x = (double)(data->x[move->device] - x);
			minfo.y = (double)(data->y[move->device] - y);

			if (data->size_type != WIDGET_SIZE_TYPE_UNKNOWN && !s_info.conf.field.use_fixed_size) {
				minfo.ratio_w = (double)data->fixed_width / (double)w;
				minfo.ratio_h = (double)data->fixed_height / (double)h;
			} else {
				minfo.ratio_w = 1.0f;
				minfo.ratio_h = 1.0f;
			}
			minfo.device = move->device;

			widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_MOVE, &minfo);
		}
	} else {
		if (s_info.conf.field.auto_render_selector) {
			s_info.conf.field.render_animator = 1;
		}
	}
}

static void __widget_move_cb(void *cbdata, Evas *e, Evas_Object *obj, void *event_info)
{
	Evas_Event_Mouse_Move *move = event_info;
	struct widget_data *data = cbdata;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	if (!IS_PRESSED(data, 0)) {
		return;
	}

	if (s_info.conf.field.support_gbar && data->is.field.flick_down && data->y[0] - move->cur.canvas.y > 0) {
		struct widget_evas_event_info info;

		DbgPrint("Flick down is canceled\n");
		data->is.field.flick_down = 0;
		info.widget_app_id = data->widget_id;
		info.event = WIDGET_EVENT_GBAR_CREATED;
		info.error = WIDGET_ERROR_CANCELED;

		smart_callback_call(data, WIDGET_SMART_SIGNAL_FLICKDOWN_CANCELLED, &info);
	}

	update_scroll_flag(data, move->cur.canvas.x, move->cur.canvas.y);

	data->x[0] = move->cur.canvas.x;
	data->y[0] = move->cur.canvas.y;

	if (data->handle && !data->is.field.faulted) {
		Evas_Coord x, y, w, h;
		struct widget_mouse_event_info minfo;
		int need_move_event = 0;

		evas_object_geometry_get(obj, &x, &y, &w, &h);

		if (data->size_type != WIDGET_SIZE_TYPE_UNKNOWN && !s_info.conf.field.use_fixed_size) {
			minfo.ratio_w = (double)data->fixed_width / (double)w;
			minfo.ratio_h = (double)data->fixed_height / (double)h;
		} else {
			minfo.ratio_w = 1.0f;
			minfo.ratio_h = 1.0f;
		}
		minfo.device = 0;

		if (data->is.field.cancel_click == CANCEL_USER) {

			minfo.x = (double)(data->x[0] - x);
			minfo.y = (double)(data->y[0] - y);
			widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_ON_HOLD, &minfo);

			/*
			 * Reset the clicked event
			 */
			data->is.field.cancel_click = CANCEL_PROCESSED;

			if (s_info.conf.field.auto_render_selector) {
				DbgPrint("Change to render animator\n");
				s_info.conf.field.render_animator = 1;
			}

			/**
			 * @note
			 * After ON_HOLD, we should send one MOVE event.
			 */
			need_move_event = 1;
		}

		if (!s_info.conf.field.auto_feed && (need_move_event || data->is.field.mouse_event)) {
			minfo.x = (double)(data->x[0] - x);
			minfo.y = (double)(data->y[0] - y);

			widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_MOVE, &minfo);
		}

		if (s_info.conf.field.support_gbar && data->is.field.flick_down && abs(data->y[0] - data->down.y) > CLICK_REGION) {
			struct widget_evas_event_info info;
			data->is.field.flick_down = 0;
			info.widget_app_id = data->widget_id;
			info.event = WIDGET_EVENT_GBAR_CREATED;
			info.error = WIDGET_ERROR_CANCELED;
			smart_callback_call(data, WIDGET_SMART_SIGNAL_FLICKDOWN_CANCELLED, &info);
			DbgPrint("Flick down is canceled\n");
		}

	} else {
		if (s_info.conf.field.auto_render_selector) {
			s_info.conf.field.render_animator = 1;
		}
	}
}

static char *get_package_icon(struct widget_data *data)
{
	char *icon;
	char *uiapp;

	if (data->size_type == WIDGET_SIZE_TYPE_UNKNOWN) {
		icon = widget_service_get_icon(data->widget_id, NULL);
	} else {
		icon = get_preview_image(data, data->size_type);
	}

	if (icon && access(icon, R_OK) == 0) {
		return icon;
	}

	if (icon) {
		ErrPrint("Failed to access an icon file: [%s]\n", icon);
		free(icon);
		icon = NULL;
	}

	uiapp = widget_service_get_main_app_id(data->widget_id);
	if (uiapp) {
		int ret;
		pkgmgrinfo_appinfo_h appinfo_h;

		ret = pkgmgrinfo_appinfo_get_appinfo(uiapp, &appinfo_h);
		free(uiapp);

		if (ret < 0) {
			/**
			 * 'Icon' will be remained as NULL
			 */
		} else {
			char *uiapp_icon = NULL;

			ret = pkgmgrinfo_appinfo_get_icon(appinfo_h, &uiapp_icon);
			if (ret == PMINFO_R_OK && uiapp_icon && access(uiapp_icon, R_OK) == 0) {
				DbgPrint("UI-App icon: [%s]\n", uiapp_icon);
				icon = strdup(uiapp_icon);
				if (!icon) {
					ErrPrint("Heap: %d\n", strerror(errno));
					/**
					 * @note
					 * 'Icon' will be specified to "unknown" icon file (Default)
					 */
				}
			} else {
				ErrPrint("[%s] - %d\n", uiapp_icon, errno);
			}

			pkgmgrinfo_appinfo_destroy_appinfo(appinfo_h);
		}
	}

	if (!icon) {
		icon = strdup(WIDGET_VIEWER_EVAS_UNKNOWN);
		if (!icon) {
			ErrPrint("strdup: %d\n", errno);
		}
	}

	return icon;
}

static void activate_ret_cb(struct widget *handle, int ret, void *cbdata)
{
	struct widget_data *data = cbdata;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	__widget_overlay_disable(data, 1, 1);

	DbgPrint("Activated (%s): %d\n", data->widget_id, ret);
	if (!data->is.field.deleted && (ret == WIDGET_ERROR_NONE || ret == WIDGET_ERROR_INVALID_PARAMETER)) {
		widget_size_type_e type;
		Evas_Coord w, h;
		struct acquire_data acquire_data = {
			.data = data,
		};

		evas_object_geometry_get(data->widget_layout, NULL, NULL, &w, &h);

		type = find_size_type(data, w, h);
		if (type == WIDGET_SIZE_TYPE_UNKNOWN) {
			ErrPrint("Failed to find a proper size type: %dx%d\n", w, h);
			type = WIDGET_SIZE_TYPE_1x1;
		}

		data->is.field.faulted = 0;
		data->is.field.created = 0;
		data->is.field.send_delete = 1;
		update_widget_geometry(&acquire_data);
		data->handle = widget_viewer_add_widget(data->widget_id, data->content,
				data->cluster, data->category,
				data->period, type,
				__widget_created_cb, widget_ref(data));
		if (!data->handle) {
			ErrPrint("Failed to send add request\n");
			widget_unref(data);
			return;
		}

		DbgPrint("Added Handle: (%p) %p\n", data->handle, data);
		widget_viewer_set_data(data->handle, data->widget);
		__widget_overlay_loading(data);
	}

	data->is.field.deleted = 0;
	widget_unref(data);
}

static void __widget_animation_done_cb(void *cbdata, Evas_Object *obj, const char *signal_name, const char *source)
{
	struct widget_data *data = cbdata;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	if (widget_viewer_has_glance_bar(data->handle)) {
	} else {
		DbgPrint("Animation finished\n");
	}
}

static void __widget_turn_done_cb(void *cbdata, Evas_Object *obj, const char *signal_name, const char *source)
{
	struct widget_data *data = cbdata;
	Evas_Object *overlay;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	overlay = elm_object_part_content_unset(data->widget_layout, "overlay,content");
	if (overlay) {
		evas_object_del(overlay);
		data->is.field.widget_overlay_loaded = 0;
	}
}

static void __widget_overlay_clicked_cb(void *cbdata, Evas_Object *obj, const char *signal_name, const char *source)
{
	struct widget_data *data = cbdata;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	DbgPrint("Overlay is clicked: (%s) (%s)\n", signal_name, source);
	if (!data->is.field.faulted) {
		/*!
		 * \todo
		 * Reload
		 */
		DbgPrint("Package [%s] is not faulted one\n", data->widget_id);
	} else {
		DbgPrint("Activate: [%s]\n", data->widget_id);
		if (widget_viewer_activate_faulted_widget(data->widget_id, activate_ret_cb, widget_ref(data)) < 0) {
			ErrPrint("Failed to activate %s\n", data->widget_id);
			widget_unref(data);
		}
	}
}

static void widget_layout_del_cb(void *_data, Evas *e, Evas_Object *obj, void *event_info)
{
	struct widget_data *data = _data;
	DbgPrint("Layout is deleted (%p - %p)\n", data->widget_layout, obj);
	evas_object_smart_member_del(obj);
	evas_object_clip_unset(obj);
	data->widget_layout = NULL;
}

static void stage_del_cb(void *_data, Evas *e, Evas_Object *obj, void *event_info)
{
	struct widget_data *data = _data;

	DbgPrint("Stage is deleted (%p - %p)\n", data->stage, obj);
	if (data->widget_layout) {
		evas_object_clip_unset(data->widget_layout);
	}

	evas_object_smart_member_del(obj);
	data->stage = NULL;
}

static void __widget_data_setup(struct widget_data *data)
{
	data->e = evas_object_evas_get(data->widget);
	if (!data->e) {
		ErrPrint("Failed to get Evas object\n");
		data->state = WIDGET_DATA_DELETED;
		free(data);
		return;
	}

	data->stage = evas_object_rectangle_add(data->e);
	if (!data->stage) {
		ErrPrint("Failed to add stage object\n");
		data->state = WIDGET_DATA_DELETED;
		free(data);
		return;
	}

	evas_object_color_set(data->stage, 255, 255, 255, 255);

	data->widget_layout = elm_layout_add(data->parent);
	if (!data->widget_layout) {
		ErrPrint("Failed to add edje object\n");
		evas_object_del(data->stage);
		data->state = WIDGET_DATA_DELETED;
		free(data);
		return;
	}

	if (elm_layout_file_set(data->widget_layout, WIDGET_VIEWER_EVAS_RESOURCE_EDJ, WIDGET_VIEWER_EVAS_RESOURCE_LB) == EINA_FALSE) {
		ErrPrint("Failed to load edje object: %s(%s)\n", WIDGET_VIEWER_EVAS_RESOURCE_EDJ, WIDGET_VIEWER_EVAS_RESOURCE_LB);
		evas_object_del(data->widget_layout);
		evas_object_del(data->stage);
		data->state = WIDGET_DATA_DELETED;
		free(data);
		return;
	}

	if (!s_info.conf.field.disable_internal_scroller && (s_info.conf.field.is_scroll_x || s_info.conf.field.is_scroll_y)) {
		Evas_Object *scroller;
		scroller = elm_scroller_add(data->parent);
		if (scroller) {
			Evas_Object *box;

			elm_scroller_bounce_set(scroller, EINA_FALSE, EINA_FALSE);
			elm_scroller_policy_set(scroller, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_OFF);
			elm_scroller_single_direction_set(scroller, ELM_SCROLLER_SINGLE_DIRECTION_HARD);
			/* elm_object_scroll_lock_x_set(scroller, EINA_TRUE); */

			box = evas_object_rectangle_add(data->e);
			if (box) {
				int height = 0;
				int width = 0;

				if (s_info.conf.field.is_scroll_x) {
					height = s_info.screen_height << 1;
				}

				if (s_info.conf.field.is_scroll_y) {
					width = s_info.screen_width << 1;
				}

				evas_object_color_set(box, 0, 0, 0, 0);
				evas_object_resize(box, width, height);
				evas_object_size_hint_min_set(box, width, height);
				evas_object_show(box);
			}

			elm_object_content_set(scroller, box);
			elm_object_part_content_set(data->widget_layout, "scroller", scroller);
		} else {
			ErrPrint("Failed to create a scroller\n");
		}
	}

	evas_object_show(data->widget_layout);

	elm_object_signal_callback_add(data->widget_layout, "mouse,clicked,1", "overlay,content", __widget_overlay_clicked_cb, data);
	elm_object_signal_callback_add(data->widget_layout, "done", "turn", __widget_turn_done_cb, data);
	elm_object_signal_callback_add(data->widget_layout, "finished", "animation", __widget_animation_done_cb, data);

	evas_object_event_callback_add(data->widget_layout, EVAS_CALLBACK_MOUSE_DOWN, __widget_down_cb, data);
	evas_object_event_callback_add(data->widget_layout, EVAS_CALLBACK_MOUSE_MOVE, __widget_move_cb, data);
	evas_object_event_callback_add(data->widget_layout, EVAS_CALLBACK_MOUSE_UP, __widget_up_cb, data);

	evas_object_event_callback_add(data->widget_layout, EVAS_CALLBACK_MULTI_DOWN, __widget_multi_down_cb, data);
	evas_object_event_callback_add(data->widget_layout, EVAS_CALLBACK_MULTI_MOVE, __widget_multi_move_cb, data);
	evas_object_event_callback_add(data->widget_layout, EVAS_CALLBACK_MULTI_UP, __widget_multi_up_cb, data);

	evas_object_smart_member_add(data->stage, data->widget);
	evas_object_smart_member_add(data->widget_layout, data->widget);
	evas_object_clip_set(data->widget_layout, data->stage);

	evas_object_event_callback_add(data->widget_layout, EVAS_CALLBACK_DEL, widget_layout_del_cb, data);
	evas_object_event_callback_add(data->stage, EVAS_CALLBACK_DEL, stage_del_cb, data);
}

static Eina_Bool renderer_cb(void *_data)
{
	struct widget_data *data;

	EINA_LIST_FREE(s_info.widget_dirty_objects, data) {
		__widget_event_widget_updated(data);
	}

	EINA_LIST_FREE(s_info.gbar_dirty_objects, data) {
		__widget_event_gbar_updated(data);
	}

	s_info.renderer = NULL;
	return ECORE_CALLBACK_CANCEL;
}

static void remove_widget_dirty_object_list(struct widget_data *data)
{
	s_info.widget_dirty_objects = eina_list_remove(s_info.widget_dirty_objects, data);
}

static void append_widget_dirty_object_list(struct widget_data *data, int idx)
{
	data->is.field.widget_dirty = 1;

	if (idx != WIDGET_KEEP_BUFFER) {
		data->widget_latest_idx = idx;
	}

	if (widget_viewer_get_visibility(data->handle) != WIDGET_SHOW) {
		DbgPrint("Box[%s] is not visible\n", data->widget_id);
		return;
	}

	if (s_info.conf.field.render_animator) {
		if (eina_list_data_find(s_info.widget_dirty_objects, data)) {
			return;
		}

		if (!s_info.renderer) {
			s_info.renderer = ecore_animator_add(renderer_cb, NULL);
			if (!s_info.renderer) {
				ErrPrint("Failed to create a renderer\n");
			}
		}

		s_info.widget_dirty_objects = eina_list_append(s_info.widget_dirty_objects, data);
	} else {
		if (s_info.renderer) {
			ecore_animator_del(s_info.renderer);
			s_info.renderer = NULL;
		}

		/* Need a choice
		 * Do we have to discard these all changes? or just flush them?
		 struct widget_data *item;
		 EINA_LIST_FREE(s_info.widget_dirty_objects, item) {
			__widget_event_widget_updated(item);
		 }
		 */
		eina_list_free(s_info.widget_dirty_objects);
		s_info.widget_dirty_objects = NULL;
		__widget_event_widget_updated(data);
	}
}

static void remove_gbar_dirty_object_list(struct widget_data *data)
{
	s_info.gbar_dirty_objects = eina_list_remove(s_info.gbar_dirty_objects, data);
}

static void append_gbar_dirty_object_list(struct widget_data *data, int idx)
{
	data->is.field.gbar_dirty = 1;

	if (idx != WIDGET_KEEP_BUFFER) {
		data->gbar_latest_idx = idx;
	}

	if (widget_viewer_get_visibility(data->handle) != WIDGET_SHOW) {
		return;
	}

	if (s_info.conf.field.render_animator) {
		if (eina_list_data_find(s_info.gbar_dirty_objects, data)) {
			return;
		}

		if (!s_info.renderer) {
			s_info.renderer = ecore_animator_add(renderer_cb, NULL);
			if (!s_info.renderer) {
				ErrPrint("Failed to create a renderer\n");
			}
		}

		s_info.gbar_dirty_objects = eina_list_append(s_info.gbar_dirty_objects, data);
	} else {
		if (s_info.renderer) {
			ecore_animator_del(s_info.renderer);
			s_info.renderer = NULL;
		}

		/* Need a choice
		 * Do we have to discard these all changes? or just flush them?
		 struct widget_data *item;
		 EINA_LIST_FREE(s_info.gbar_dirty_objects, item) {
		 __widget_event_gbar_updated(item);
		 }
		 */
		eina_list_free(s_info.gbar_dirty_objects);
		s_info.gbar_dirty_objects = NULL;
		__widget_event_gbar_updated(data);
	}
}

static void __widget_add(Evas_Object *widget)
{
	struct widget_data *data;

	data = calloc(1, sizeof(*data));
	if (!data) {
		ErrPrint("Heap: %d\n", errno);
		return;
	}

	data->state = WIDGET_DATA_CREATED;
	data->widget = widget;
	data->is.field.permanent_delete = 0;
	data->widget_latest_idx = WIDGET_PRIMARY_BUFFER;
	data->gbar_latest_idx = WIDGET_PRIMARY_BUFFER;
	evas_object_smart_data_set(data->widget, data);
	widget_ref(data);

	s_info.list = eina_list_append(s_info.list, widget);
	return;
}

static Evas_Object *create_image_object(struct widget_data *data)
{
	Evas_Object *img;

	img = evas_object_image_filled_add(data->e);
	if (!img) {
		ErrPrint("Failed to create an image object\n");
	} else {
		evas_object_image_colorspace_set(img, EVAS_COLORSPACE_ARGB8888);
		evas_object_image_alpha_set(img, EINA_TRUE);
	}

	return img;
}

static void replace_widget_pixmap_with_image(struct widget_data *data)
{
	Evas_Object *img;
	Evas_Object *content;

	content = elm_object_part_content_unset(data->widget_layout, "widget,content");
	if (!content) {
		ErrPrint("Failed to get content object\n");
		return;
	}

	img = create_image_object(data);
	if (img) {
		Evas_Coord w;
		Evas_Coord h;
		void *image_data;

		evas_object_image_size_get(content, &w, &h);
		evas_object_image_size_set(img, w, h);

		image_data = evas_object_image_data_get(content, 0);
		if (image_data) {
			evas_object_image_data_copy_set(img, image_data);
		}

		evas_object_image_fill_set(img, 0, 0, w, h);
		evas_object_image_pixels_dirty_set(img, EINA_TRUE);
		evas_object_image_data_update_add(img, 0, 0, w, h);

		elm_object_part_content_set(data->widget_layout, "widget,content", img);
	} else {
		ErrPrint("Failed to create an image object\n");
	}

	evas_object_del(content);
}

static void replace_gbar_pixmap_with_image(struct widget_data *data)
{
	Evas_Object *img;
	Evas_Object *gbar_content;

	gbar_content = elm_object_part_content_unset(data->widget_layout, "gbar,content");
	if (!gbar_content) {
		ErrPrint("Failed to get content object\n");
		return;
	}

	img = create_image_object(data);
	if (img) {
		Evas_Coord w;
		Evas_Coord h;
		void *content;

		evas_object_image_size_get(gbar_content, &w, &h);
		evas_object_image_size_set(img, w, h);

		content = evas_object_image_data_get(gbar_content, 0);
		if (content) {
			evas_object_image_data_copy_set(img, content);
		}

		evas_object_image_fill_set(img, 0, 0, w, h);
		evas_object_image_pixels_dirty_set(img, EINA_TRUE);
		evas_object_image_data_update_add(img, 0, 0, w, h);

		elm_object_part_content_set(data->widget_layout, "gbar,content", img);
	} else {
		ErrPrint("Failed to create an image object\n");
	}

	evas_object_del(gbar_content);
}

static void __widget_destroy_widget_cb(widget_h handle, int ret, void *_data)
{
	struct widget_data *data = _data;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	if (data->widget_pixmap) {
		replace_widget_pixmap_with_image(data);
	}

	if (data->gbar_pixmap) {
		replace_gbar_pixmap_with_image(data);
	}

	data->is.field.send_delete = 0;
	DbgPrint("Invoke raw delete %s\n", data->widget_id);
	(void)invoke_raw_event_callback(WIDGET_VIEWER_EVAS_RAW_DELETE, data->widget_id, NULL, ret);
	remove_widget_dirty_object_list(data);
	remove_gbar_dirty_object_list(data); /* for the safety */
	widget_unref(data);
}

static void __widget_del(Evas_Object *widget)
{
	struct widget_data *data = NULL;

	data = evas_object_smart_data_get(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return;
	}

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	if (data->is.field.deleted == 1) {
		DbgPrint("Already deleted: %s\n", data->widget_id);
		return;
	}

	data->is.field.deleted = 1;

	if (data->delayed_resume_timer) {
		ecore_timer_del(data->delayed_resume_timer);
		data->delayed_resume_timer = NULL;
	}

	s_info.list = eina_list_remove(s_info.list, widget);

	if (data->handle) {
		widget_viewer_set_data(data->handle, NULL);

		if (data->is.field.send_delete) {
			widget_delete_type_e delete_type;

			if (data->is.field.permanent_delete) {
				delete_type = WIDGET_DELETE_PERMANENTLY;
			} else {
				delete_type = WIDGET_DELETE_TEMPORARY;
			}
			DbgPrint("Send delete request (0x%X) - %s\n", delete_type, data->widget_id);

			if (data->is.field.created) {
				if (widget_viewer_delete_widget(data->handle, delete_type, __widget_destroy_widget_cb, widget_ref(data)) < 0) {
					widget_unref(data);
					data = NULL;
				}
			} else {
				DbgPrint("Not created yet. this will be canceld by created callback, ignore delete callback\n");
				if (widget_viewer_delete_widget(data->handle, delete_type, NULL, NULL) < 0) {
					DbgPrint("Unref %p\n", data);
				}
			}
		} else {
			DbgPrint("Skip delete request\n");
		}
	} else {
		DbgPrint("Handle is not created: %s\n", data->widget_id);
		if (data->is.field.faulted && data->widget_id) {
			/* If a package is faulted. we have to activate it before delete object from here */
			if (widget_viewer_activate_faulted_widget(data->widget_id, NULL, NULL) < 0) {
				ErrPrint("Activate: %s\n", data->widget_id);
			}
		}
	}

	/**
	 * From now, the widget object is not valid
	 */
	if (data) {
		data->widget = NULL;
		widget_unref(data);
	}
}

static Eina_Bool delayed_resume_timer_cb(void *_data)
{
	struct widget_data *data = _data;

	(void)widget_viewer_set_visibility(data->handle, WIDGET_SHOW);

	if (data->is.field.widget_dirty) {
		/**
		 * If the object has dirty flag, pumping it up again
		 * To updates its content
		 */
		append_widget_dirty_object_list(data, WIDGET_KEEP_BUFFER);
	}

	data->delayed_resume_timer = NULL;
	return ECORE_CALLBACK_CANCEL;
}

static void update_visibility(struct widget_data *data)
{
	int is_visible = 0;
	int delay;

	if (!data->handle || !data->is.field.created) {
		return;
	}

	if (data->is.field.freeze_visibility) {
		DbgPrint("Freezed visibility: %X (%s)\n", data->freezed_visibility, widget_viewer_get_pkgname(data->handle));

		if (data->freezed_visibility == WIDGET_VISIBILITY_STATUS_SHOW_FIXED) {
			if (data->delayed_resume_timer) {
				(void)ecore_timer_del(data->delayed_resume_timer);
				data->delayed_resume_timer = NULL;
			}
			data->is.field.initial_resumed = 1;
			(void)widget_viewer_set_visibility(data->handle, WIDGET_SHOW);
		} else if (data->freezed_visibility == WIDGET_VISIBILITY_STATUS_HIDE_FIXED) {
			if (data->delayed_resume_timer) {
				(void)ecore_timer_del(data->delayed_resume_timer);
				data->delayed_resume_timer = NULL;
			}
			(void)widget_viewer_set_visibility(data->handle, WIDGET_HIDE_WITH_PAUSE);
		}
		return;
	}

	is_visible = evas_object_visible_get(data->stage);

	if (is_visible) {
		Evas_Coord x, y, w, h;

		evas_object_geometry_get(data->widget_layout, &x, &y, &w, &h);

		if (!s_info.conf.field.user_view_port) {
			Ecore_Evas *ee;

			ee = ecore_evas_ecore_evas_get(data->e);
			if (ee) {
				ecore_evas_geometry_get(ee, &data->view_port.x, &data->view_port.y, &data->view_port.w, &data->view_port.h);
			} else {
				data->view_port.x = 0;
				data->view_port.y = 0;
				util_screen_size_get(&data->view_port.w, &data->view_port.h);
				ErrPrint("Failed to get view port info (Fallback: %dx%d - %dx%d\n",
						data->view_port.x, data->view_port.y, data->view_port.w, data->view_port.h);
			}
		}

		if (x + w <= data->view_port.x || x >= data->view_port.x + data->view_port.w || y + h <= data->view_port.y || y >= data->view_port.y + data->view_port.h) {
			is_visible = 0;
		} else {
			is_visible = 1;
		}
	}

	if (data->is.field.delayed_resume == 0) { /* Follow the global configuration */
		delay = s_info.conf.field.delayed_resume;
	} else { /* Ignore the global configuration */
		/* 1 : Disable Delayed Pause Resume */
		/* 2 : Enable Delayed Pause Resume */
		delay = (data->is.field.delayed_resume == 2);
	}

	if (delay) {
		if (is_visible) {
			if (data->delayed_resume_timer) {
				DbgPrint("Reset timer\n");
				ecore_timer_reset(data->delayed_resume_timer);
			} else if (WIDGET_CONF_VISIBILITY_CHANGE_DELAY > 0.0f) {
				if (data->is.field.initial_resumed == 0) {
					/**
					 * @note
					 * If a widget is not resumed before,
					 * it should be resumed immediately for displaying its contents ASAP.
					 */
					DbgPrint("First immeidiate resume\n");
					data->is.field.initial_resumed = 1;
					delayed_resume_timer_cb(data);
				} else {
					DbgPrint("Delayed resume (%lf)\n", WIDGET_CONF_VISIBILITY_CHANGE_DELAY);
					data->delayed_resume_timer = ecore_timer_add(WIDGET_CONF_VISIBILITY_CHANGE_DELAY, delayed_resume_timer_cb, data);
					if (!data->delayed_resume_timer) {
						ErrPrint("Failed to add a timer\n");
						delayed_resume_timer_cb(data);
					}
				}
			} else {
				delayed_resume_timer_cb(data);
			}
		} else {
			if (data->delayed_resume_timer) {
				ecore_timer_del(data->delayed_resume_timer);
				data->delayed_resume_timer = NULL;
			}

			widget_viewer_set_visibility(data->handle, WIDGET_HIDE_WITH_PAUSE);
		}
	} else {
		/**
		 * @note
		 * In this case, if there is any registered timer,
		 * this function should clear it.
		 * Timer means that the delayed_resume mode is changed.
		 */
		if (data->delayed_resume_timer) {
			ecore_timer_del(data->delayed_resume_timer);
			data->delayed_resume_timer = NULL;
			DbgPrint("Clear timer of delayed resuming\n");
		}

		if (is_visible) {
			delayed_resume_timer_cb(data);
		} else {
			widget_viewer_set_visibility(data->handle, WIDGET_HIDE_WITH_PAUSE);
		}
	}
}

static int do_force_mouse_up(struct widget_data *data)
{
	struct widget_mouse_event_info minfo;
	Evas_Coord x, y, w, h;
	struct widget_evas_event_info info;
	int i;

	if (s_info.conf.field.auto_render_selector && s_info.conf.field.render_animator == 0) {
		DbgPrint("Change to render animator\n");
		s_info.conf.field.render_animator = 1;
	}

	if (!data->pressed) {
		/**
		 * @note
		 * No buttons are pressed or there is no touch.
		 */
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	/**
	 * @todo
	 * What happens if this is called for GBar?
	 */
	evas_object_geometry_get(data->widget, &x, &y, &w, &h);

	/**
	 * @note
	 * If the GBar is pressed, we don't need to calculate the ratio.
	 */
	if (!data->down.is_gbar && (data->size_type != WIDGET_SIZE_TYPE_UNKNOWN && !s_info.conf.field.use_fixed_size)) {
		minfo.ratio_w = (double)data->fixed_width / (double)w;
		minfo.ratio_h = (double)data->fixed_height / (double)h;
		DbgPrint("Width: %d/%d - Height: %d/%d\n", data->fixed_width, w, data->fixed_height, h);
	} else {
		minfo.ratio_w = 1.0f;
		minfo.ratio_h = 1.0f;
	}
	DbgPrint("Ratio: %lfx%lf\n", minfo.ratio_w, minfo.ratio_h);

	DbgPrint("%x\n", data->is.field.cancel_click);
	if (data->is.field.cancel_click != CANCEL_PROCESSED) {
		DbgPrint("ON_HOLD send\n");
		minfo.device = 0;
		minfo.x = (double)(data->x[0] - x);
		minfo.y = (double)(data->y[0] - y);

		widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_ON_HOLD, &minfo);
		data->is.field.cancel_click = CANCEL_PROCESSED;
	}

	reset_scroller(data);

	if (s_info.conf.field.auto_feed && data->is.field.mouse_event) {
		for (i = 0; i < MAX_DEVICE; i++) {
			if (IS_PRESSED(data, i)) {
				/**
				 * @note
				 * UNSET will subtract object.x and object.y by master
				 * so we just send original touch position based on screen
				 */
				minfo.x = (double)data->x[i];
				minfo.y = (double)data->y[i];
				DbgPrint("X,Y = %lfx%lf\n", minfo.x, minfo.y);
				minfo.device = i;

				widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_UNSET, &minfo);
			}
		}
	} else {
		DbgPrint("%lfx%lf (%lfx%lf)\n", minfo.x, minfo.y, minfo.ratio_w, minfo.ratio_h);
		for (i = 0; i < MAX_DEVICE; i++) {
			if (IS_PRESSED(data, i)) {
				minfo.x = (double)(data->x[i] - x);
				minfo.y = (double)(data->y[i] - y);
				minfo.device = i;

				widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_UP, &minfo);
				widget_viewer_feed_mouse_event(data->handle, WIDGET_MOUSE_LEAVE, &minfo);
			}
		}
	}

	data->pressed = 0;
	data->is.field.cancel_click = CANCEL_DISABLED;

	if (s_info.conf.field.support_gbar && data->is.field.flick_down) {
		data->is.field.flick_down = 0;
		info.widget_app_id = data->widget_id;
		info.event = WIDGET_EVENT_GBAR_CREATED;
		info.error = WIDGET_ERROR_CANCELED;
		smart_callback_call(data, WIDGET_SMART_SIGNAL_FLICKDOWN_CANCELLED, &info);
		DbgPrint("Flick down is canceled\n");
	}

	return WIDGET_ERROR_NONE;
}

static void __widget_move(Evas_Object *widget, Evas_Coord x, Evas_Coord y)
{
	struct widget_data *data;
	Evas_Coord w, h;

	data = evas_object_smart_data_get(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return;
	}

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	if (data->gbar_layout) {
		Evas_Coord gbar_x, gbar_y, gbar_h;
		Evas_Coord prev_x, prev_y;
		Evas_Coord widget_w, widget_h;
		double rx;
		double ry;

		evas_object_geometry_get(data->widget_layout, &prev_x, &prev_y, &widget_w, &widget_h);
		evas_object_geometry_get(data->gbar_layout, &gbar_x, &gbar_y, NULL, &gbar_h);

		gbar_x += (x - prev_x);
		gbar_y += (y - prev_y);

		evas_object_move(data->gbar_layout, gbar_x, gbar_y);

		rx = ((double)x + (double)widget_w / 2.0f) / s_info.screen_width;
		switch (find_size_type(data, widget_w, widget_h)) {
		case WIDGET_SIZE_TYPE_1x1:
			if (rx < 0.25f) {
				rx = 0.125f;
			} else if (rx < 0.5f) {
				rx = 0.375f;
			} else if (rx < 0.75f) {
				rx = 0.625f;
			} else if (rx < 1.0f) {
				rx = 0.875f;
			}
			break;
		case WIDGET_SIZE_TYPE_2x1:
		case WIDGET_SIZE_TYPE_2x2:
			if (rx < 0.5f) {
				rx = 0.25f;
			} else if (rx < 0.75f) {
				rx = 0.5f;
			} else {
				rx = 0.75f;
			}
			break;
		default:
			rx = 0.5f;
			break;
		}

		if (prev_y + widget_h + gbar_h > s_info.screen_height) {
			ry = 1.0f;
		} else {
			ry = 0.0f;
		}

		if (data->is.field.gbar_created) {
			widget_viewer_move_glance_bar(data->handle, rx, ry);
		}
	}

	evas_object_move(data->stage, x, y);
	evas_object_move(data->widget_layout, x, y);
	evas_object_geometry_get(data->widget_layout, NULL, NULL, &w, &h);

	if (!s_info.conf.field.manual_pause_resume) {
		update_visibility(data);
	}

	if (s_info.conf.field.sensitive_move) {
		do_force_mouse_up(data);
	}
}

static int widget_create_plug_object(struct widget_data *data)
{
	struct acquire_data acquire_data = {
		.w = 0,
		.h = 0,
		.content = NULL,
		.data = data,
	};

	DbgPrint("Plug type created\n");

	acquire_data.content = elm_object_part_content_unset(data->widget_layout, "widget,content");
	if (acquire_data.content) {
		DbgPrint("widget Content is already prepared: %s\n", widget_viewer_get_filename(data->handle));
		evas_object_del(acquire_data.content);
	}

	acquire_data.content = elm_plug_add(s_info.win);
	if (!acquire_data.content) {
		ErrPrint("Failed to add a plug object\n");
		return WIDGET_ERROR_FAULT;
	}

	DbgPrint("Try to connect to %s\n", widget_viewer_get_filename(data->handle));
	if (!elm_plug_connect(acquire_data.content, widget_viewer_get_filename(data->handle), 0, EINA_TRUE)) {
		ErrPrint("Cannot connect plug[%s]", widget_viewer_get_filename(data->handle));
		evas_object_del(acquire_data.content);
		return WIDGET_ERROR_FAULT;
	}

	elm_object_part_content_set(data->widget_layout, "widget,content", acquire_data.content);

	acquire_data.w = data->widget_width;
	acquire_data.h = data->widget_height;
	update_widget_geometry(&acquire_data);
	return WIDGET_ERROR_NONE;
}

static int widget_create_image_object(struct widget_data *data)
{
	Evas_Object *front_image;
	struct acquire_data acquire_data = {
		.w = 0,
		.h = 0,
		.content = NULL,
		.data = data,
	};

	DbgPrint("Image type created\n");

	acquire_data.content = elm_object_part_content_get(data->widget_layout, "widget,content");
	if (!acquire_data.content) {
		acquire_data.content = elm_layout_add(data->parent);
		if (!acquire_data.content) {
			ErrPrint("Failed to create an edje object\n");
			return WIDGET_ERROR_FAULT;
		}

		if (elm_layout_file_set(acquire_data.content, WIDGET_VIEWER_EVAS_RESOURCE_EDJ, WIDGET_VIEWER_EVAS_RESOURCE_IMG) == EINA_FALSE) {
			ErrPrint("Failed to load edje object: %s(%s)\n", WIDGET_VIEWER_EVAS_RESOURCE_EDJ, WIDGET_VIEWER_EVAS_RESOURCE_IMG);
			evas_object_del(acquire_data.content);
			return WIDGET_ERROR_IO_ERROR;
		}

		front_image = elm_image_add(acquire_data.content);
		if (!front_image) {
			ErrPrint("Failed to add front_image object\n");
			evas_object_del(acquire_data.content);
			return WIDGET_ERROR_FAULT;
		}

		DbgPrint("Default size %dx%d\n", data->widget_width, data->widget_height);

		elm_object_part_content_set(acquire_data.content, "front,content", front_image);
		elm_object_part_content_set(data->widget_layout, "widget,content", acquire_data.content);
	} else {
		front_image = elm_object_part_content_get(acquire_data.content, "front,content");
		if (!front_image) {
			ErrPrint("Unable to get front,content object\n");
			front_image = elm_image_add(acquire_data.content);
			if (!front_image) {
				ErrPrint("Failed to add front_image object\n");
				return WIDGET_ERROR_FAULT;
			}

			elm_object_part_content_set(acquire_data.content, "front,content", front_image);
		}
	}

	/*
	   evas_object_geometry_get(data->widget, NULL, NULL, &acquire_data.w, &acquire_data.h);
	   DbgPrint("Default size %dx%d\n", acquire_data.w, acquire_data.h);
	   DbgPrint("Image size: %dx%d\n", acquire_data.w, acquire_data.h);
	 */
	acquire_data.w = data->widget_width;
	acquire_data.h = data->widget_height;
	update_widget_geometry(&acquire_data);
	return WIDGET_ERROR_NONE;
}

static int widget_create_buffer_object(struct widget_data *data)
{
	Evas_Object *content;

	content = elm_object_part_content_get(data->widget_layout, "widget,content");
	if (!content) {
		content = evas_object_image_filled_add(data->e);
		if (!content) {
			ErrPrint("Failed to create an image object\n");
			return WIDGET_ERROR_FAULT;
		}

		evas_object_image_colorspace_set(content, EVAS_COLORSPACE_ARGB8888);
		evas_object_image_alpha_set(content, EINA_TRUE);
		elm_object_part_content_set(data->widget_layout, "widget,content", content);
	}

	return WIDGET_ERROR_NONE;
}

static int widget_text_update_begin(widget_h handle)
{
	struct widget_data *data;
	data = get_smart_data_from_handle(handle);
	if (!data) {
		return WIDGET_ERROR_FAULT;
	}

	DbgPrint("Begin text update: [%s]\n", data->widget_id);

	return WIDGET_ERROR_NONE;
}

static int widget_text_update_end(widget_h handle)
{
	struct widget_data *data;
	data = get_smart_data_from_handle(handle);
	if (!data) {
		return WIDGET_ERROR_FAULT;
	}

	DbgPrint("End text update: [%s]\n", data->widget_id);

	return WIDGET_ERROR_NONE;
}

static int widget_text_update_text(widget_h handle, const char *id, const char *part, const char *data)
{
	struct widget_data *widget_data;
	Evas_Object *layout;

	widget_data = get_smart_data_from_handle(handle);
	if (!widget_data) {
		return WIDGET_ERROR_FAULT;
	}

	layout = find_script_object(widget_data, 0, id);
	if (!layout) {
		ErrPrint("Target[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	return do_text_update_text(widget_data->parent, layout, part, data);
}

static int widget_text_update_image(widget_h handle, const char *id, const char *part, const char *data, const char *option)
{
	struct widget_data *widget_data;
	Evas_Object *layout;

	widget_data = get_smart_data_from_handle(handle);
	if (!widget_data) {
		return WIDGET_ERROR_FAULT;
	}

	layout = find_script_object(widget_data, 0, id);
	if (!layout) {
		ErrPrint("Target[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	return do_text_update_image(layout, part, data, option);
}

static int widget_text_update_script(widget_h handle, const char *id, const char *new_id, const char *part, const char *file, const char *group)
{
	struct widget_data *data;
	Evas_Object *layout;

	data = get_smart_data_from_handle(handle);
	if (!data) {
		return WIDGET_ERROR_FAULT;
	}

	layout = find_script_object(data, 0, id);
	if (!layout) {
		ErrPrint("Target[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	return do_text_update_script(data, 0, layout, new_id, part, file, group);
}

static int widget_text_update_signal(widget_h handle, const char *id, const char *signal_name, const char *signal)
{
	struct widget_data *data;
	Evas_Object *layout;

	data = get_smart_data_from_handle(handle);
	if (!data) {
		return WIDGET_ERROR_FAULT;
	}

	layout = find_script_object(data, 0, id);
	if (!layout) {
		ErrPrint("Target[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	elm_object_signal_emit(layout, signal, signal_name);
	return WIDGET_ERROR_NONE;
}

static int widget_text_update_drag(widget_h handle, const char *id, const char *part, double dx, double dy)
{
	struct widget_data *data;
	Evas_Object *layout;

	data = get_smart_data_from_handle(handle);
	if (!data) {
		return WIDGET_ERROR_FAULT;
	}

	layout = find_script_object(data, 0, id);
	if (!layout) {
		ErrPrint("Target[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	edje_object_part_drag_value_set(elm_layout_edje_get(layout), part, dx, dy);
	return WIDGET_ERROR_NONE;
}

static int widget_text_update_info_size(widget_h handle, const char *id, int w, int h)
{
	struct widget_data *data;
	Evas_Object *layout;

	data = get_smart_data_from_handle(handle);
	if (!data) {
		return WIDGET_ERROR_FAULT;
	}

	layout = find_script_object(data, 0, id);
	if (!layout) {
		ErrPrint("Target[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	DbgPrint("Resize to %dx%d\n", w, h);
	evas_object_resize(layout, w, h);

	return WIDGET_ERROR_NONE;
}

static int widget_text_update_info_category(widget_h handle, const char *id, const char *category)
{
	struct widget_data *data;
	Evas_Object *layout;

	data = get_smart_data_from_handle(handle);
	if (!data) {
		return WIDGET_ERROR_FAULT;
	}

	layout = find_script_object(data, 0, id);
	if (!layout) {
		ErrPrint("Target[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	DbgPrint("Update category: %s\n", category);

	return WIDGET_ERROR_NONE;
}

static int widget_text_update_access(widget_h handle, const char *id, const char *part, const char *text, const char *option)
{
	struct widget_data *data;
	Evas_Object *layout;

	data = get_smart_data_from_handle(handle);
	if (!data) {
		return WIDGET_ERROR_FAULT;
	}

	layout = find_script_object(data, 0, id);
	if (!layout) {
		ErrPrint("Target[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	return do_text_update_access(data->parent, layout, part, text, option);
}

static int widget_text_operate_access(widget_h handle, const char *id, const char *part, const char *operation, const char *option)
{
	struct widget_data *data;
	Evas_Object *layout;

	data = get_smart_data_from_handle(handle);
	if (!data) {
		return WIDGET_ERROR_FAULT;
	}

	layout = find_script_object(data, 0, id);
	if (!layout) {
		ErrPrint("Target[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	return do_text_operate_access(layout, part, operation, option);
}

static int widget_text_update_color(widget_h handle, const char *id, const char *part, const char *data)
{
	struct widget_data *widget_data;
	Evas_Object *layout;

	widget_data = get_smart_data_from_handle(handle);
	if (!widget_data) {
		return WIDGET_ERROR_FAULT;
	}

	layout = find_script_object(widget_data, 0, id);
	if (!layout) {
		ErrPrint("Target[%s] is not exists\n", id);
		return WIDGET_ERROR_NOT_EXIST;
	}

	return do_text_update_color(layout, part, data);
}
static int widget_create_text_object(struct widget_data *data)
{
	Evas_Object *content;

	content = elm_object_part_content_get(data->widget_layout, "widget,content");
	if (!content) {
		int ret;
		char *script_file;
		char *script_group;
		struct widget_script_operators operator = {
			.update_begin = widget_text_update_begin,
			.update_end = widget_text_update_end,

			.update_text = widget_text_update_text,
			.update_image = widget_text_update_image,
			.update_script = widget_text_update_script,
			.update_signal = widget_text_update_signal,
			.update_drag = widget_text_update_drag,
			.update_info_size = widget_text_update_info_size,
			.update_info_category = widget_text_update_info_category,
			.update_access = widget_text_update_access,
			.operate_access = widget_text_operate_access,
			.update_color = widget_text_update_color,
		};

		content = elm_layout_add(data->widget_layout);
		if (!content) {
			ErrPrint("Failed to create a layout object\n");
			return WIDGET_ERROR_FAULT;
		}

		script_file = widget_service_get_widget_script_path(data->widget_id);
		script_group = widget_service_get_widget_script_group(data->widget_id);
		if (!script_file || !script_group) {
			ErrPrint("Invalid script info ([%s] - [%s])\n", script_file, script_group);
			free(script_file);
			free(script_group);
			evas_object_del(content);
			return WIDGET_ERROR_FAULT;
		}

		if (access(script_file, R_OK) != 0) {
			ErrPrint("Unable to access [%s] - %d\n", script_file, errno);
			free(script_file);
			free(script_group);
			evas_object_del(content);
			return WIDGET_ERROR_FAULT;
		}

		ret = elm_layout_file_set(content, script_file, script_group);
		DbgPrint("Load edje file ([%s] - [%s])\n", script_file, script_group);
		free(script_file);
		free(script_group);
		if (ret == EINA_FALSE) {
			ErrPrint("Failed to load EDJE\n");
			evas_object_del(content);
			return WIDGET_ERROR_FAULT;
		}

		ret = widget_viewer_set_text_handler(data->handle, 0, &operator);
		if (ret != WIDGET_ERROR_NONE) {
			ErrPrint("Failed to set text handler for [%s]\n", data->widget_id);
			evas_object_del(content);
			return ret;
		}

		ret = append_script_object(data, 0, NULL, NULL, content);
		if (ret != WIDGET_ERROR_NONE) {
			ErrPrint("Failed to append this to script object list\n");
			evas_object_del(content);
			return ret;
		}

		elm_object_part_content_set(data->widget_layout, "widget,content", content);
	}

	return WIDGET_ERROR_NONE;
}

static int widget_create_pixmap_object(struct widget_data *data)
{
	Evas_Object *content;

	content = elm_object_part_content_get(data->widget_layout, "widget,content");
	if (!content) {
		content = evas_object_image_filled_add(data->e);
		if (!content) {
			ErrPrint("Failed to create an image object\n");
			return WIDGET_ERROR_FAULT;
		}

		evas_object_image_colorspace_set(content, EVAS_COLORSPACE_ARGB8888);
		evas_object_image_alpha_set(content, EINA_TRUE);
		evas_object_event_callback_add(content, EVAS_CALLBACK_DEL, __widget_pixmap_del_cb, data);

		elm_object_part_content_set(data->widget_layout, "widget,content", content);
	}

	return WIDGET_ERROR_NONE;
}

static void __widget_resize_pixmap_object(struct widget_data *data)
{
	DbgPrint("widget resize request is succssfully sent\n");
}

static void update_widget_pixmap(Evas_Object *content, int w, int h)
{
	evas_object_image_pixels_dirty_set(content, EINA_TRUE);
	evas_object_image_data_update_add(content, 0, 0, w, h);
	evas_object_show(content);
}

static void acquire_widget_extra_resource_cb(struct widget *handle, int pixmap, void *cbdata)
{
	DbgPrint("Acquired: %u\n", (unsigned int)pixmap);
}

static void acquire_gbar_extra_resource_cb(struct widget *handle, int pixmap, void *cbdata)
{
	DbgPrint("Acquired: %u\n", (unsigned int)pixmap);
}

static void replace_pixmap(struct widget *handle, int gbar, Evas_Object *content, unsigned int pixmap)
{
	unsigned int old_pixmap;

	old_pixmap = util_replace_native_surface(handle, gbar, content, pixmap);
	if (!s_info.conf.field.skip_acquire && old_pixmap != 0u) {
		widget_viewer_release_resource_id(handle, gbar, old_pixmap);
	}
}

static void acquire_widget_pixmap_cb(struct widget *handle, int pixmap, void *cbdata)
{
	struct acquire_data *acquire_data = cbdata;
	struct widget_data *data = acquire_data->data;

	data->is.field.widget_pixmap_acquire_requested = 0;
	__widget_overlay_disable(data, 0, 0);

	if (pixmap == 0) {
		DbgPrint("Pixmap gotten (0)\n");
		if (!s_info.conf.field.skip_acquire) {
			free(acquire_data);
		}
		widget_unref(data);
		return;
	}

	evas_object_image_size_set(acquire_data->content, acquire_data->w, acquire_data->h);
	evas_object_image_fill_set(acquire_data->content, 0, 0, acquire_data->w, acquire_data->h);
	DbgPrint("fillset: %dx%d\n", acquire_data->w, acquire_data->h);

	replace_pixmap(handle, 0, acquire_data->content, (unsigned int)pixmap);

	data->widget_pixmap = pixmap;

	append_widget_dirty_object_list(data, WIDGET_KEEP_BUFFER);
	update_widget_geometry(acquire_data);

	widget_unref(data);
	if (!s_info.conf.field.skip_acquire) {
		free(acquire_data);
	}

	if (WIDGET_CONF_ENABLE_RESOURCE_LOCK) {
		if (!eina_list_data_find(s_info.updated_pixmap_list, (void *)(PTR_TYPE(pixmap)))) {
			s_info.updated_pixmap_list = eina_list_append(s_info.updated_pixmap_list, (void *)(PTR_TYPE(pixmap)));
		}
	}
}

static void __widget_update_pixmap_object(struct widget_data *data, Evas_Object *widget_content, int w, int h)
{
	int ret;
	struct acquire_data *acquire_data;

	if (data->widget_latest_idx == WIDGET_PRIMARY_BUFFER) {
		unsigned int resource_id = 0u;

		if (widget_viewer_get_resource_id(data->handle, 0, &resource_id) < 0) {
			ErrPrint("Failed to get resource_id\n");
		} else if (resource_id > 0) {
			if (data->widget_pixmap == resource_id) {
				if (data->widget_extra) {
					/* Just replace the pixmap in this case, do not release old pixmap */
					replace_pixmap(NULL, 0, widget_content, data->widget_pixmap);
				}

				update_widget_pixmap(widget_content, w, h);

				if (WIDGET_CONF_ENABLE_RESOURCE_LOCK) {
					if (!eina_list_data_find(s_info.updated_pixmap_list, (void *)(PTR_TYPE(resource_id)))) {
						s_info.updated_pixmap_list = eina_list_append(s_info.updated_pixmap_list, (void *)(PTR_TYPE(resource_id)));
					}
				}
				return;
			}

			if (s_info.conf.field.skip_acquire && resource_id != 0) {
				struct acquire_data local_acquire_data = {
					.data = widget_ref(data),
					.content = widget_content,
					.w = w,
					.h = h,
				};

				/**
				 * @note
				 * Resource lock will be acquired from below callback.
				 */
				acquire_widget_pixmap_cb(data->handle, resource_id, &local_acquire_data);
				return;
			}

			if (data->is.field.widget_pixmap_acquire_requested) {
				DbgPrint("Pixmap is not acquired (in-process)\n");
				return;
			}

			acquire_data = malloc(sizeof(*acquire_data));
			if (!acquire_data) {
				ErrPrint("malloc: %d\n", errno);
				return;
			}

			acquire_data->data = widget_ref(data);
			acquire_data->content = widget_content;
			acquire_data->w = w;
			acquire_data->h = h;

			ret = widget_viewer_acquire_resource_id(data->handle, 0, acquire_widget_pixmap_cb, acquire_data);
			if (ret != WIDGET_ERROR_NONE) {
				widget_unref(data);
				free(acquire_data);
			} else {
				data->is.field.widget_pixmap_acquire_requested = 1;
			}

			/**
			 * @note
			 * Resource lock should be acquired from acquire_widget_pixmap_cb
			 */
		}
	} else {
		if (!data->widget_extra) {
			ErrPrint("Extra buffer is not prepared yet\n");
			return;
		}

		replace_pixmap(NULL, 0, widget_content, data->widget_extra[data->widget_latest_idx]);
		update_widget_pixmap(widget_content, w, h);
		if (WIDGET_CONF_ENABLE_RESOURCE_LOCK) {
			if (!eina_list_data_find(s_info.updated_pixmap_list, (void *)(PTR_TYPE(data->widget_extra[data->widget_latest_idx])))) {
				s_info.updated_pixmap_list = eina_list_append(s_info.updated_pixmap_list, (void *)(PTR_TYPE(data->widget_extra[data->widget_latest_idx])));
			}
		}
	}

}

static int widget_system_created(struct widget *handle, struct widget_data *data)
{
	int ret;
	struct widget_evas_event_info info;
	widget_type_e widget_type;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p, %s\n", data, widget_viewer_get_pkgname(handle));
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	widget_viewer_get_size_type(handle, &data->size_type);

	if (data->size_type == WIDGET_SIZE_TYPE_UNKNOWN || widget_service_get_size(data->size_type, &data->widget_width, &data->widget_height) < 0) {
		ErrPrint("Failed to get size info: %s\n", widget_viewer_get_pkgname(handle));

	} else {
		DbgPrint("System created WIDGET size is (%d)%dx%d\n", data->size_type, data->widget_width, data->widget_height);
	}

	widget_viewer_get_type(handle, 0, &widget_type);

	switch (widget_type) {
	case WIDGET_CONTENT_TYPE_IMAGE:
		ret = widget_create_image_object(data);
		break;
	case WIDGET_CONTENT_TYPE_RESOURCE_ID:
		if (!s_info.conf.field.force_to_buffer) {
			ret = widget_create_pixmap_object(data);
			break;
		}
	case WIDGET_CONTENT_TYPE_BUFFER:
		ret = widget_create_buffer_object(data);
		break;
	case WIDGET_CONTENT_TYPE_TEXT:
		ret = widget_create_text_object(data);
		break;
	case WIDGET_CONTENT_TYPE_UIFW:
		ret = widget_create_plug_object(data);
		break;
	case WIDGET_CONTENT_TYPE_INVALID:
	default:
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		break;
	}

	if (ret == WIDGET_ERROR_NONE) {
		info.error = WIDGET_ERROR_NONE;
		info.widget_app_id = data->widget_id;
		info.event = WIDGET_EVENT_CREATED;

		data->is.field.created = 1;

		update_visibility(data);
		smart_callback_call(data, WIDGET_SMART_SIGNAL_WIDGET_CREATED, &info);

		/**
		 * In case of using the direct update path,
		 * sometimes the provider can send the updated event faster than created event.
		 * In that case, the viewer cannot recognize the updated content of a widget.
		 * So for the safety, I added this to forcely update the widget at the first time
		 * Right after creating its instance.
		 */
		append_widget_dirty_object_list(data, WIDGET_PRIMARY_BUFFER);
	}

	return ret;
}

static void __widget_created_cb(struct widget *handle, int ret, void *cbdata)
{
	struct widget_data *data = cbdata;
	struct widget_evas_event_info info;
	widget_type_e widget_type;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p (%d), %s\n", data, ret, widget_viewer_get_pkgname(handle));
		return;
	}

	if (ret != WIDGET_ERROR_NONE) {
		DbgPrint("Failed to create: %X\n", ret);
		data->handle = NULL;

		if (!data->is.field.deleted) {
			struct widget_evas_event_info fault_event;

			fault_event.error = ret;
			fault_event.widget_app_id = data->widget_id;
			fault_event.event = WIDGET_EVENT_CREATED;

			if (!data->is.field.faulted) {
				data->is.field.faulted = 1;

				DbgPrint("Display tap to load (%p) [%s]\n", data, data->widget_id);
				smart_callback_call(data, WIDGET_SMART_SIGNAL_WIDGET_CREATE_ABORTED, &fault_event);

				__widget_overlay_faulted(data);
			} else {
				smart_callback_call(data, WIDGET_SMART_SIGNAL_WIDGET_CREATE_ABORTED, &fault_event);

				/**
				 * @note
				 * While processing the faulted smart callback, the user can request hide_faulted_overlay
				 * So we should reset it from here. in other cases, the widget_overlay_faulted function deals with this.
				 */
				data->is.field.hide_faulted_overlay = 0;
			}

			ret = WIDGET_ERROR_FAULT;
		} else {
			ret = WIDGET_ERROR_CANCELED;
		}

		data->is.field.send_delete = 0;
		DbgPrint("Invoke raw delete %s\n", data->widget_id);
		(void)invoke_raw_event_callback(WIDGET_VIEWER_EVAS_RAW_DELETE, data->widget_id, data->widget, ret);
		widget_unref(data);
		return;
	}

	widget_viewer_get_type(handle, 0, &widget_type);

	switch (widget_type) {
	case WIDGET_CONTENT_TYPE_IMAGE:
		ret = widget_create_image_object(data);
		break;
	case WIDGET_CONTENT_TYPE_RESOURCE_ID:
		if (!s_info.conf.field.force_to_buffer) {
			ret = widget_create_pixmap_object(data);
			break;
		}
	case WIDGET_CONTENT_TYPE_BUFFER:
		ret = widget_create_buffer_object(data);
		break;
	case WIDGET_CONTENT_TYPE_TEXT:
		ret = widget_create_text_object(data);
		break;
	case WIDGET_CONTENT_TYPE_UIFW:
		ret = widget_create_plug_object(data);
		break;
	case WIDGET_CONTENT_TYPE_INVALID:
	default:
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		break;
	}

	if (ret == WIDGET_ERROR_NONE) {
		info.error = WIDGET_ERROR_NONE;
		info.widget_app_id = data->widget_id;
		info.event = WIDGET_EVENT_CREATED;

		data->is.field.created = 1;

		update_visibility(data);
		smart_callback_call(data, WIDGET_SMART_SIGNAL_WIDGET_CREATED, &info);
		DbgPrint("Invoke raw create %s\n", data->widget_id);
		(void)invoke_raw_event_callback(WIDGET_VIEWER_EVAS_RAW_CREATE, data->widget_id, data->widget, ret);

		/**
		 * In case of using the direct update path,
		 * sometimes the provider can send the updated event faster than created event.
		 * In that case, the viewer cannot recognize the updated content of a widget.
		 * So for the safety, I added this to forcely update the widget at the first time
		 * Right after creating its instance.
		 */
		append_widget_dirty_object_list(data, WIDGET_KEEP_BUFFER);

		if (data->is.field.extra_info_updated) {
			data->is.field.extra_info_updated = 0;

			info.widget_app_id = data->widget_id;
			info.event = WIDGET_EVENT_EXTRA_INFO_UPDATED;
			info.error = WIDGET_ERROR_NONE;
			smart_callback_call(data, WIDGET_SMART_SIGNAL_EXTRA_INFO_UPDATED, &info);
		}
	} else {
		info.error = ret;
		info.widget_app_id = data->widget_id;
		info.event = WIDGET_EVENT_CREATED;
		smart_callback_call(data, WIDGET_SMART_SIGNAL_WIDGET_CREATE_ABORTED, &info);
		data->is.field.send_delete = 0;
		DbgPrint("Invoke raw delete %s\n", data->widget_id);
		(void)invoke_raw_event_callback(WIDGET_VIEWER_EVAS_RAW_DELETE, data->widget_id, data->widget, ret);
	}

	widget_unref(data);
}

static void __widget_resize_image_object(struct widget_data *data)
{
	DbgPrint("widget resize request is succssfully sent\n");
}

static void __widget_resize_buffer_object(struct widget_data *data)
{
	DbgPrint("widget resize request is succssfully sent\n");
}

static void __widget_resize_text_object(struct widget_data *data)
{
	DbgPrint("widget resize request is succssfully sent\n");
}

static void __widget_resize_cb(struct widget *handle, int ret, void *cbdata)
{
	struct widget_data *data = cbdata;
	struct widget_evas_event_info info;
	widget_type_e widget_type;

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	if (ret != WIDGET_ERROR_NONE) {
		info.error = ret;
		info.event = WIDGET_EVENT_WIDGET_SIZE_CHANGED;
		info.widget_app_id = data->widget_id;
		smart_callback_call(data, WIDGET_SMART_SIGNAL_WIDGET_RESIZE_ABORTED, &info);
		widget_unref(data);
		return;
	}

	widget_viewer_get_type(handle, 0, &widget_type);
	switch (widget_type) {
	case WIDGET_CONTENT_TYPE_IMAGE:
		__widget_resize_image_object(data);
		break;
	case WIDGET_CONTENT_TYPE_RESOURCE_ID:
		if (!s_info.conf.field.force_to_buffer) {
			__widget_resize_pixmap_object(data);
			break;
		}
	case WIDGET_CONTENT_TYPE_BUFFER:
		__widget_resize_buffer_object(data);
		break;
	case WIDGET_CONTENT_TYPE_TEXT:
		__widget_resize_text_object(data);
		break;
	case WIDGET_CONTENT_TYPE_UIFW:
		break;
	case WIDGET_CONTENT_TYPE_INVALID:
		break;
	default:
		break;
	}

	Evas_Object *scroller;
	scroller = elm_object_part_content_get(data->widget_layout, "scroller");
	if (scroller) {
		Evas_Object *box;

		box = elm_object_content_get(scroller);
		if (box) {
			widget_size_type_e type;

			if (widget_viewer_get_size_type(handle, &type) == WIDGET_ERROR_NONE) {
				int height;
				int width;

				if (widget_service_get_size(type, &width, &height) == WIDGET_ERROR_NONE) {
					if (s_info.conf.field.is_scroll_x) {
						height <<= 1;
					}

					if (s_info.conf.field.is_scroll_y) {
						width <<= 1;
					}

					DbgPrint("Update scroller size to : %dx%d\n", width, height);
					elm_object_content_unset(scroller);
					evas_object_resize(box, width, height);
					evas_object_size_hint_min_set(box, width, height);
					elm_object_content_set(scroller, box);
				}
			} else {
				ErrPrint("Failed to get widget size: %x\n", type);
			}
		}
	}

	info.error = ret;
	info.event = WIDGET_EVENT_WIDGET_SIZE_CHANGED;
	info.widget_app_id = data->widget_id;
	smart_callback_call(data, WIDGET_SMART_SIGNAL_WIDGET_RESIZED, &info);
	widget_unref(data);
}

static void gbar_overlay_disable(struct widget_data *data)
{
	if (!data->gbar_layout) {
		return;
	}

	if (!data->is.field.gbar_overlay_loaded) {
		return;
	}

	elm_object_signal_emit(data->gbar_layout, "disable", "overlay");
	data->is.field.gbar_overlay_loaded = 0;
}

static void gbar_overlay_loading(struct widget_data *data)
{
	Evas_Object *rect;

	if (data->is.field.gbar_overlay_loaded) {
		ErrPrint("Already loaded");
		return;
	}

	rect = elm_object_part_content_unset(data->gbar_layout, "overlay,content");
	if (rect) {
		evas_object_del(rect);
	}

	rect = evas_object_rectangle_add(data->e);
	evas_object_color_set(rect, 0, 0, 0, 0);
	evas_object_show(rect);
	/*!
	 * \todo
	 * Overlay for loading a GBAR
	 */

	elm_object_part_content_set(data->gbar_layout, "overlay,content", rect);
	elm_object_signal_emit(data->gbar_layout, "enable", "overlay");

	data->is.field.gbar_overlay_loaded = 1;
}

static Evas_Object *widget_load_overlay_edje(struct widget_data *data)
{
	Evas_Object *overlay;

	overlay = elm_layout_add(data->parent);
	if (!overlay) {
		ErrPrint("Failed to create a overlay\n");
		return NULL;
	}

	if (elm_layout_file_set(overlay, WIDGET_VIEWER_EVAS_RESOURCE_EDJ, WIDGET_VIEWER_EVAS_RESOURCE_OVERLAY_LOADING) == EINA_FALSE) {
		ErrPrint("Failed to load overlay file\n");
		evas_object_del(overlay);
		return NULL;
	}

	elm_object_part_content_set(data->widget_layout, "overlay,content", overlay);
	return overlay;
}

static Eina_Bool delayed_overlay_disable_cb(void *_data)
{
	struct widget_data *data = _data;

	if (data->is.field.disabled_overlay_animation) {
		elm_object_signal_emit(data->widget_layout, "disable,without,animation", "overlay");
	} else {
		elm_object_signal_emit(data->widget_layout, "disable", "overlay");
	}

	data->is.field.widget_overlay_loaded = 0;
	data->overlay_update_counter = DEFAULT_OVERLAY_COUNTER;
	data->overlay_timer = NULL;
	return ECORE_CALLBACK_CANCEL;
}

static void __widget_overlay_disable(struct widget_data *data, int no_timer, int ignore_update_count)
{
	if (!data->widget_layout) {
		return;
	}

	if (!data->is.field.widget_overlay_loaded) {
		return;
	}

	if (ignore_update_count) {
		data->overlay_update_counter = 0;
	} else {
		if (data->is.field.hide_overlay_manually) {
			return;
		}
	}

	/**
	 * @note
	 * After the frame update counter reaches to ZERO,
	 * The overlay will be disabled immediately if the "no_timer" is true(1),
	 * or it will be disabled after a few milli-seconds later.
	 * The timer will be fired when it gets the first frame update event.
	 * And it will start estimating seconds.
	 * Even after the last frame is updated (by "overlay_update_counter"),
	 * The timer will be expired after DEFAULT_OVERLAY_WAIT_TIME secs later
	 * which is started from the first frame update event.
	 */
	data->overlay_update_counter--;
	if (data->overlay_update_counter <= 0) {
		if (no_timer) {
			if (data->overlay_timer) {
				ecore_timer_del(data->overlay_timer);
				/**
				 * @note
				 * data->overlay_timer will be reset'd to NULL by delayed_overlay_disable_cb().
				 */
			}
			delayed_overlay_disable_cb(data);
			return;
		}
	}

	if (!no_timer && !data->overlay_timer) {
		/**
		 * @note
		 * This routine will be executed when the update_counter is greater than 0,
		 * (means that the frame should be updated more as this count)
		 */
		data->overlay_timer = ecore_timer_add(DEFAULT_OVERLAY_WAIT_TIME, delayed_overlay_disable_cb, data);
		if (!data->overlay_timer) {
			ErrPrint("Failed to create a timer\n");
			delayed_overlay_disable_cb(data);
		}
	}
}

static void __widget_overlay_loading(struct widget_data *data)
{
	struct acquire_data acquire_data;
	Evas_Object *overlay;

	if (data->is.field.disable_loading == 1) {
		DbgPrint("Loading overlay is disabled");
		return;
	}

	if (data->is.field.widget_overlay_loaded == 1) {
		DbgPrint("Overlay is already loaded");
		return;
	}

	overlay = elm_object_part_content_get(data->widget_layout, "overlay,content");
	if (!overlay) {
		overlay = widget_load_overlay_edje(data);
		if (!overlay) {
			return;
		}
	}

	if (!data->is.field.disable_preview) {
		char *icon;

		icon = get_package_icon(data);
		if (icon) {
			Evas_Object *preview;

			preview = elm_object_part_content_get(overlay, "preview");
			if (!preview) {
				preview = elm_image_add(overlay);
			}

			if (preview) {
				elm_image_file_set(preview, icon, NULL);
				elm_object_part_content_set(overlay, "preview", preview);
			}

			free(icon);
		}

		DbgPrint("Set overlay loading (%p) %s\n", data, data->widget_id);
	} else {
		DbgPrint("Overlay is disabled (%s)\n", data->widget_id);
	}

	elm_object_domain_translatable_part_text_set(overlay, "text", PKGNAME, "IDS_ST_BODY_LOADING_ING");
	if (data->is.field.disable_text) {
		elm_object_signal_emit(overlay, "disable", "text");
	}

	elm_object_signal_emit(data->widget_layout, "reset", "overlay");
	if (data->is.field.disabled_overlay_animation) {
		elm_object_signal_emit(data->widget_layout, "enable,without,animation", "overlay");
	} else {
		elm_object_signal_emit(data->widget_layout, "enable", "overlay");
	}

	evas_object_geometry_get(data->widget, NULL, NULL, &acquire_data.w, &acquire_data.h);
	acquire_data.content = NULL;
	acquire_data.data = data;
	update_widget_geometry(&acquire_data);

	data->is.field.widget_overlay_loaded = 1;
	data->overlay_update_counter = DEFAULT_OVERLAY_COUNTER;
}

static void __widget_overlay_faulted(struct widget_data *data)
{
	struct acquire_data acquire_data;
	Evas_Object *overlay;
	widget_type_e widget_type;

	if (data->is.field.hide_faulted_overlay == 1) {
		DbgPrint("Faulted overlay is hidden by request (%s)\n", data->widget_id);
		data->is.field.hide_faulted_overlay = 0;
		return;
	}

	if (data->is.field.widget_overlay_loaded) {
		__widget_overlay_disable(data, 1, 1);
	}

	overlay = elm_object_part_content_get(data->widget_layout, "overlay,content");
	if (!overlay) {
		overlay = widget_load_overlay_edje(data);
		if (!overlay) {
			return;
		}
	}

	widget_viewer_get_type(data->handle, 0, &widget_type);
	if (widget_type != WIDGET_CONTENT_TYPE_IMAGE) {
		Evas_Object *preview;

		preview = elm_object_part_content_get(overlay, "preview");
		if (!preview) {
			char *icon;

			icon = get_preview_image(data, data->size_type);
			if (icon) {
				preview = elm_image_add(data->widget_layout);
				if (preview) {
					elm_image_file_set(preview, icon, NULL);
					elm_object_part_content_set(overlay, "preview", preview);
				}

				free(icon);
			}
		}
	}

	DbgPrint("Set overlay fault (%p) %s\n", data, data->widget_id);
	elm_object_domain_translatable_part_text_set(overlay, "text", PKGNAME, "IDS_HS_BODY_UNABLE_TO_LOAD_DATA_TAP_TO_RETRY");
	elm_object_signal_emit(overlay, "enable", "text");
	elm_object_signal_emit(data->widget_layout, "reset", "overlay");
	elm_object_signal_emit(data->widget_layout, "enable", "overlay");

	evas_object_geometry_get(data->widget, NULL, NULL, &acquire_data.w, &acquire_data.h);
	acquire_data.content = NULL;
	acquire_data.data = data;
	update_widget_geometry(&acquire_data);
	data->is.field.widget_overlay_loaded = 1;
}

static void __widget_resize(Evas_Object *widget, Evas_Coord w, Evas_Coord h)
{
	struct widget_data *data;
	widget_size_type_e type;
	bool need_of_touch_effect = false;
	bool need_of_mouse_event = false;

	data = evas_object_smart_data_get(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return;
	}

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	type = find_size_type(data, w, h);
	if (type == WIDGET_SIZE_TYPE_UNKNOWN) {
		ErrPrint("Invalid size: %dx%d\n", w, h);
	} else if (s_info.conf.field.use_fixed_size) {
		if (widget_service_get_size(type, &w, &h) < 0) {
			ErrPrint("Failed to get box size\n");
		}
		data->fixed_width = w;
		data->fixed_height = h;
	} else {
		if (widget_service_get_size(type, &data->fixed_width, &data->fixed_height) < 0) {
			DbgPrint("Use the given size\n");
			data->fixed_width = w;
			data->fixed_width = h;
		} else {
			ErrPrint("Failed to get box size\n");
		}
	}

	/**
	 * @note
	 * If the use_fixed_size is false, the size_type can be differ with width & height
	 * Then we should not try to resize resource_id(surface) as width & height.
	 * Just keep them their size and try to resize layout only.
	 */
	if (!widget_viewer_is_created_by_user(data->handle)) {
		/**
		 * Viewer should not be able to resize the box
		 */
		ErrPrint("System created Widget is not able to be resized (%s)\n", widget_viewer_get_pkgname(data->handle));

		/* But update its size by handle's size */
		widget_viewer_get_size_type(data->handle, &data->size_type);

		if (data->size_type == WIDGET_SIZE_TYPE_UNKNOWN || widget_service_get_size(data->size_type, &data->widget_width, &data->widget_height)) {
			ErrPrint("Unable to get default size from handle\n");
			/*
			* In this case, just depends on user's request.
			* Because, there is no other information which we can use.
			*/
			data->size_type = type;
			data->widget_width = w;
			data->widget_height = h;
		}
	} else {
		data->widget_width = w;
		data->widget_height = h;
		data->size_type = type;
	}

	DbgPrint("Request size change: %dx%d [0x%X]\n", w, h, data->size_type);

	if (data->is.field.faulted) {
		evas_object_resize(data->widget_layout, data->widget_width, data->widget_height);
		ErrPrint("Faulted widget, skip resizing (%s)\n", data->widget_id);
		return;
	}

	if (!data->handle) {
		struct acquire_data acquire_data = {
			.data = data,
		};

		DbgPrint("Create new handle: %dx%d, (%s, %s), %s/%s\n", data->widget_width, data->widget_height,
				data->widget_id, data->content,
				data->cluster, data->category);

		if (widget_viewer_activate_faulted_widget(data->widget_id, NULL, NULL) < 0) {
			ErrPrint("Activate: %s\n", data->widget_id);
		}

		data->is.field.created = 0;
		data->is.field.send_delete = 1;
		update_widget_geometry(&acquire_data);

		data->handle = widget_viewer_add_widget(data->widget_id, data->content,
				data->cluster, data->category,
				data->period, type,
				__widget_created_cb, widget_ref(data));
		if (!data->handle) {
			ErrPrint("Failed to send add request\n");
			DbgPrint("Unref %p %s\n", data, data->widget_id);
			widget_unref(data);
			return;
		}

		DbgPrint("Added handle: %p (%p)\n", data->handle, data);
		widget_viewer_set_data(data->handle, widget);
		__widget_overlay_loading(data);
		widget_service_get_need_of_touch_effect(data->widget_id, type, (bool *)&need_of_touch_effect);
		data->is.field.touch_effect = need_of_touch_effect;
		widget_service_get_need_of_mouse_event(data->widget_id, type, (bool *)&need_of_mouse_event);
		data->is.field.mouse_event = need_of_mouse_event;
	} else {

		int ret;

		if (type > 0 && type != WIDGET_SIZE_TYPE_UNKNOWN) {
			ret = widget_viewer_resize_widget(data->handle, type, __widget_resize_cb, widget_ref(data));
		} else {
			ret = WIDGET_ERROR_INVALID_PARAMETER;
			/* This will be decreased soon ... */
			widget_ref(data);
		}

		evas_object_resize(data->widget_layout, data->widget_width, data->widget_height);
		if (ret == WIDGET_ERROR_ALREADY_EXIST) {
			DbgPrint("Same size\n");
			widget_unref(data);
		} else if (ret == WIDGET_ERROR_NONE) {
			DbgPrint("Resize request is successfully sent\n");
			widget_service_get_need_of_touch_effect(data->widget_id, type, (bool *)&need_of_touch_effect);
			data->is.field.touch_effect = need_of_touch_effect;
			widget_service_get_need_of_mouse_event(data->widget_id, type, (bool *)&need_of_mouse_event);
			data->is.field.mouse_event = need_of_mouse_event;
		} else {
			widget_unref(data);
		}
	}
}

static void __widget_show(Evas_Object *widget)
{
	struct widget_data *data;

	data = evas_object_smart_data_get(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return;
	}

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	evas_object_show(data->stage);
	evas_object_show(data->widget_layout);

	if (!s_info.conf.field.manual_pause_resume) {
		update_visibility(data);
	}
}

static void __widget_hide(Evas_Object *widget)
{
	struct widget_data *data;

	data = evas_object_smart_data_get(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return;
	}

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	evas_object_hide(data->stage);
	evas_object_hide(data->widget_layout);

	if (!s_info.conf.field.manual_pause_resume) {
		update_visibility(data);
	}
}

static void __widget_color_set(Evas_Object *widget, int r, int g, int b, int a)
{
	struct widget_data *data;

	data = evas_object_smart_data_get(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return;
	}

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	evas_object_color_set(data->stage, r, g, b, a);
}

static void __widget_clip_set(Evas_Object *widget, Evas_Object *clip)
{
	struct widget_data *data;

	data = evas_object_smart_data_get(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return;
	}

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	evas_object_clip_set(data->stage, clip);
}

static void __widget_clip_unset(Evas_Object *widget)
{
	struct widget_data *data;

	data = evas_object_smart_data_get(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return;
	}

	if (data->state != WIDGET_DATA_CREATED) {
		ErrPrint("Invalid widget data: %p\n", data);
		return;
	}

	evas_object_clip_unset(data->stage);
}

/**
 * This must be called before update_gbar_geometry
 */
static void update_stage_geometry(struct acquire_data *acquire_data)
{
	Evas_Coord widget_x, widget_y, widget_w, widget_h;
	Evas_Coord stage_w, stage_h;

	evas_object_geometry_get(acquire_data->data->widget_layout, &widget_x, &widget_y, &widget_w, &widget_h);

	const int delta_y_top    = (acquire_data->h - widget_y);
	const int delta_y_bottom = (acquire_data->h - (s_info.screen_height - widget_y - widget_h));

	stage_w = widget_w > acquire_data->w ? widget_w : acquire_data->w;
	stage_h = widget_h + acquire_data->h;/* - delta_y_top; */

	if (delta_y_top >= delta_y_bottom) {
		evas_object_move(acquire_data->data->stage, 0, widget_y);
	} else {
		evas_object_move(acquire_data->data->stage, 0, widget_y - acquire_data->h);
	}

	evas_object_resize(acquire_data->data->stage, stage_w, stage_h);
}

static void update_gbar_geometry(struct acquire_data *acquire_data)
{
	Evas_Coord widget_x, widget_y, widget_w, widget_h;

	evas_object_geometry_get(acquire_data->data->widget_layout, &widget_x, &widget_y, &widget_w, &widget_h);

	/* How much of the GBAR is outside the screen */
	const int delta_y_top    = (acquire_data->h - widget_y) < 0 ? 0 : acquire_data->h - widget_y;
	const int delta_y_bottom = (acquire_data->h - (s_info.screen_height - widget_y - widget_h)) < 0 ? 0 : (acquire_data->h - (s_info.screen_height - widget_y - widget_h));

	/* If more of the GBAR is outside the top side draw at the bottom, otherwise draw at the top */
	if (delta_y_top >= delta_y_bottom) {
		evas_object_move(acquire_data->data->gbar_layout, 0, widget_y + widget_h - delta_y_bottom);
		effect_resize(acquire_data->data->gbar_layout, acquire_data->w, acquire_data->h, EFFECT_HEIGHT);
	} else {
		evas_object_move(acquire_data->data->gbar_layout, 0, widget_y + delta_y_top);
		effect_resize(acquire_data->data->gbar_layout, acquire_data->w, acquire_data->h, EFFECT_HEIGHT|EFFECT_MOVE);
	}
}

static void update_widget_geometry(struct acquire_data *acquire_data)
{
	Evas_Coord widget_x, widget_y, widget_w, widget_h;
	Evas_Coord stage_w, stage_h;
	struct widget_data *data = acquire_data->data;

	evas_object_resize(data->widget_layout, data->widget_width, data->widget_height);
	evas_object_geometry_get(data->widget_layout, &widget_x, &widget_y, &widget_w, &widget_h);

	if (data->gbar_layout) {
		Evas_Coord gbar_x, gbar_y, gbar_w, gbar_h;

		evas_object_geometry_get(data->gbar_layout, &gbar_x, &gbar_y, &gbar_w, &gbar_h);
		if (widget_y + widget_h + gbar_h > s_info.screen_height) {
			evas_object_move(data->gbar_layout, 0, widget_y - gbar_h);
			evas_object_move(data->stage, 0, widget_y - gbar_h);
		} else {
			evas_object_move(data->gbar_layout, 0, widget_y + widget_h);
			evas_object_move(data->stage, 0, widget_y);
		}

		stage_w = gbar_w > widget_w ? gbar_w : widget_w;
		stage_h = widget_h + gbar_h;
	} else {
		stage_w = widget_w;
		if (s_info.conf.field.support_gbar) {
			stage_h = widget_h + RESEVED_AREA_FOR_GBAR_EFFECT; /* Reserve 100 px for effect */
		} else {
			stage_h = widget_h;
		}

		evas_object_move(data->stage, widget_x, widget_y);
	}

	evas_object_resize(data->stage, stage_w, stage_h);
}

static void __widget_update_image_object(struct widget_data *data, Evas_Object *widget_content, int w, int h)
{
	Evas_Object *front_image;

	front_image = elm_object_part_content_get(widget_content, "front,content");
	if (front_image) {
		elm_image_file_set(front_image, widget_viewer_get_filename(data->handle), NULL);
	} else {
		ErrPrint("Image object not found\n");
	}
}

static void __widget_update_buffer_object(struct widget_data *data, Evas_Object *widget_content, int w, int h)
{
	struct acquire_data acquire_data = {
		.w = w,
		.h = h,
		.content = widget_content,
		.data = data,
	};

	if (data->widget_fb) {
		widget_viewer_release_buffer(data->widget_fb);
		data->widget_fb = NULL;
	}

	data->widget_fb = widget_viewer_acquire_buffer(data->handle, 0);
	if (!data->widget_fb) {
		ErrPrint("Failed to get fb\n");
		return;
	}

	evas_object_image_size_set(widget_content, w, h);

	if (widget_viewer_acquire_buffer_lock(data->handle, 0) < 0) {
		ErrPrint("Failed to acquire lock\n");
	}
	evas_object_image_data_copy_set(widget_content, data->widget_fb);
	if (widget_viewer_release_buffer_lock(data->handle, 0) < 0) {
		ErrPrint("Failed to release lock\n");
	}

	evas_object_image_fill_set(widget_content, 0, 0, w, h);
	evas_object_image_pixels_dirty_set(widget_content, EINA_TRUE);
	evas_object_image_data_update_add(widget_content, 0, 0, w, h);
	update_widget_geometry(&acquire_data);
}

static void __widget_update_text_object(struct widget_data *data, Evas_Object *widget_content, int w, int h)
{
	struct acquire_data acquire_data = {
		.w = w,
		.h = h,
		.content = widget_content,
		.data = data,
	};

	update_widget_geometry(&acquire_data);
}

static void __widget_event_extra_info_updated(struct widget_data *data)
{
	struct widget_evas_event_info info;
	const char *content_info;
	char *tmp;

	if (data->is.field.deleted) {
		DbgPrint("Box is %s, ignore update\n", data->is.field.deleted ? "deleted" : "faulted");
		return;
	}

	content_info = widget_viewer_get_content_string(data->handle);
	if (content_info && data->content) {
		if (!strcmp(content_info, data->content)) {
			/* Nothing chnaged */
		} else {
			tmp = strdup(content_info);
			if (!tmp) {
				ErrPrint("Heap: %d\n", errno);
				return;
			}

			free(data->content);
			data->content = tmp;
		}
	} else if (content_info) {
		tmp = strdup(content_info);
		if (!tmp) {
			ErrPrint("Heap: %d\n", errno);
			return;
		}
		data->content = tmp;
	} else if (data->content) {
		free(data->content);
		data->content = NULL;
	} else {
		/* Nothing changed */
	}

	if (data->is.field.created == 0) {
		DbgPrint("Extra info is updated[%s]\n", data->widget_id);
		data->is.field.extra_info_updated = 1;
	} else {
		info.widget_app_id = data->widget_id;
		info.event = WIDGET_EVENT_EXTRA_INFO_UPDATED;
		info.error = WIDGET_ERROR_NONE;
		smart_callback_call(data, WIDGET_SMART_SIGNAL_EXTRA_INFO_UPDATED, &info);
	}
}

/*!
 * Event handlers
 */
static void __widget_event_widget_updated(struct widget_data *data)
{
	Evas_Object *content;
	widget_size_type_e type;
	widget_type_e widget_type;
	int w, h;
	struct widget_evas_event_info info;

	data->is.field.widget_dirty = 0;

	if (data->is.field.deleted) {
		DbgPrint("Box is %s, ignore update\n", data->is.field.deleted ? "deleted" : "faulted");
		return;
	}

	content = elm_object_part_content_get(data->widget_layout, "widget,content");
	if (!content) {
		ErrPrint("Failed to get content object\n");
		return;
	}

	widget_viewer_get_size_type(data->handle, &type);
	if (type < 0 || type == WIDGET_SIZE_TYPE_UNKNOWN) {
		ErrPrint("Size is not valid %X\n", type);
		return;
	}

	/**
	 * @note
	 * We should not use this widget_width and widget_height
	 * It is set'd by user but the resource_id buffer should be resized by size_type.
	 * Even if the script or text or image can be resized to given size,.
	 * We should handles them as resource_id to make the consistent behaviour.
	 */
	widget_service_get_size(type, &w, &h);
	if (w != data->widget_width || h != data->widget_height) {
		DbgPrint("Pixel and Type are differ. use type [%dx%d] - [%dx%d]\n", w, h, data->widget_width, data->widget_height);
	}

	widget_viewer_get_type(data->handle, 0, &widget_type);

	switch (widget_type) {
	case WIDGET_CONTENT_TYPE_IMAGE:
		__widget_update_image_object(data, content, w, h);
		__widget_overlay_disable(data, 0, 0);
		break;
	case WIDGET_CONTENT_TYPE_RESOURCE_ID:
		if (!s_info.conf.field.force_to_buffer) {
			__widget_update_pixmap_object(data, content, w, h);
			break;
		}
	case WIDGET_CONTENT_TYPE_BUFFER:
		__widget_update_buffer_object(data, content, w, h);
		__widget_overlay_disable(data, 0, 0);
		break;
	case WIDGET_CONTENT_TYPE_TEXT:
		__widget_update_text_object(data, content, w, h);
		__widget_overlay_disable(data, 0, 0);
		break;
	case WIDGET_CONTENT_TYPE_UIFW:
		break;
	case WIDGET_CONTENT_TYPE_INVALID:
	default:
		break;
	}

	info.widget_app_id = data->widget_id;
	info.event = WIDGET_EVENT_WIDGET_UPDATED;
	info.error = WIDGET_ERROR_NONE;
	smart_callback_call(data, WIDGET_SMART_SIGNAL_UPDATED, &info);
}

static void gbar_update_buffer_object(struct widget_data *data, Evas_Object *gbar_content, int w, int h)
{
	struct acquire_data acquire_data = {
		.data = data,
		.content = gbar_content,
		.w = w,
		.h = h,
	};

	if (data->gbar_fb) {
		widget_viewer_release_buffer(data->gbar_fb);
		data->gbar_fb = NULL;
	} else {
		/* This is first time */
		gbar_overlay_disable(data);
	}

	data->gbar_fb = widget_viewer_acquire_buffer(data->handle, 1);
	if (!data->gbar_fb) {
		ErrPrint("Failed to get fb\n");
		return;
	}

	evas_object_image_size_set(gbar_content, w, h);

	if (widget_viewer_acquire_buffer_lock(data->handle, 1) < 0) {
		ErrPrint("Failed to acquire lock\n");
	}
	evas_object_image_data_copy_set(gbar_content, data->gbar_fb);
	if (widget_viewer_release_buffer_lock(data->handle, 1) < 0) {
		ErrPrint("Failed to release lock\n");
	}

	evas_object_image_fill_set(gbar_content, 0, 0, w, h);
	evas_object_image_pixels_dirty_set(gbar_content, EINA_TRUE);
	evas_object_image_data_update_add(gbar_content, 0, 0, w, h);

	update_stage_geometry(&acquire_data);
	update_gbar_geometry(&acquire_data);
}

static void gbar_update_text_object(struct widget_data *data, Evas_Object *gbar_content, int w, int h)
{
	struct acquire_data acquire_data = {
		.data = data,
		.content = gbar_content,
		.w = w,
		.h = h,
	};

	ErrPrint("Text type is updated\n");
	gbar_overlay_disable(data);

	update_stage_geometry(&acquire_data);
	update_gbar_geometry(&acquire_data);
}

static void update_gbar_pixmap(Evas_Object *content, int w, int h)
{
	evas_object_image_pixels_dirty_set(content, EINA_TRUE);
	evas_object_image_data_update_add(content, 0, 0, w, h);
	evas_object_show(content);
}

static void acquire_gbar_pixmap_cb(struct widget *handle, int pixmap, void *cbdata)
{
	struct acquire_data *acquire_data = cbdata;
	struct widget_data *data = acquire_data->data;

	data->is.field.gbar_pixmap_acquire_requested = 0;

	if (pixmap == 0) {
		ErrPrint("Failed to acquire pixmap\n");
		DbgPrint("Unref %p %s\n", data, data->widget_id);
		widget_unref(data);
		if (!s_info.conf.field.skip_acquire) {
			free(acquire_data);
		}
		return;
	}

	evas_object_image_size_set(acquire_data->content, acquire_data->w, acquire_data->h);
	evas_object_image_fill_set(acquire_data->content, 0, 0, acquire_data->w, acquire_data->h);

	/**
	 * 1 means, First time.
	 */
	if (util_set_native_surface(handle, 1, acquire_data->content, (unsigned int)pixmap, s_info.conf.field.skip_acquire) == 1) {
		gbar_overlay_disable(data);
	}

	data->gbar_pixmap = (unsigned int)pixmap;

	append_gbar_dirty_object_list(data, WIDGET_KEEP_BUFFER);
	update_stage_geometry(acquire_data);
	update_gbar_geometry(acquire_data);

	if (!s_info.conf.field.skip_acquire) {
		free(acquire_data);
	}
	DbgPrint("Unref %p %s\n", data, data->widget_id);
	widget_unref(data);
}

static void gbar_update_pixmap_object(struct widget_data *data, Evas_Object *gbar_content, int w, int h)
{
	struct acquire_data *acquire_data;
	int ret;
	unsigned int resource_id;

	if (data->gbar_latest_idx == WIDGET_PRIMARY_BUFFER) {
		widget_viewer_get_resource_id(data->handle, 1, &resource_id);
		if (data->gbar_pixmap == resource_id) {
			int ow;
			int oh;

			effect_size_get(gbar_content, &ow, &oh);

			if (data->gbar_extra) {
				replace_pixmap(NULL, 1, gbar_content, data->gbar_pixmap);
			}
			update_gbar_pixmap(gbar_content, w, h);

			if (ow != w || oh != h) {
				struct acquire_data adata = {
					.data = data,
					.content = gbar_content,
					.w = w,
					.h = h,
				};

				update_stage_geometry(&adata);
			}
			return;
		}

		if (s_info.conf.field.skip_acquire && resource_id != 0) {
			struct acquire_data local_acquire_data = {
				.data = widget_ref(data),
				.content = gbar_content,
				.w = w,
				.h = h,
			};

			acquire_gbar_pixmap_cb(data->handle, resource_id, &local_acquire_data);
			return;
		}

		if (data->is.field.gbar_pixmap_acquire_requested) {
			return;
		}

		acquire_data = malloc(sizeof(*acquire_data));
		if (!acquire_data) {
			ErrPrint("Heap: %d\n", errno);
			return;
		}

		acquire_data->content = gbar_content;
		acquire_data->w = w;
		acquire_data->h = h;
		acquire_data->data = widget_ref(data);

		ret = widget_viewer_acquire_resource_id(data->handle, 1, acquire_gbar_pixmap_cb, acquire_data);
		if (ret != WIDGET_ERROR_NONE) {
			ErrPrint("Failed to acquire gbar resource id\n");
			free(acquire_data);
			DbgPrint("Unref %p %s\n", data, data->widget_id);
			widget_unref(data);
		} else {
			data->is.field.gbar_pixmap_acquire_requested = 1;
		}
	} else {
		int ow;
		int oh;

		if (!data->gbar_extra) {
			DbgPrint("Extra GBar is not prepared yet\n");
			return;
		}

		effect_size_get(gbar_content, &ow, &oh);

		replace_pixmap(NULL, 1, gbar_content, data->gbar_extra[data->gbar_latest_idx]);
		update_gbar_pixmap(gbar_content, w, h);

		if (ow != w || oh != h) {
			struct acquire_data adata = {
				.data = data,
				.content = gbar_content,
				.w = w,
				.h = h,
			};

			update_stage_geometry(&adata);
		}
	}
}

static void __widget_event_gbar_updated(struct widget_data *data)
{
	Evas_Object *gbar_content;
	int w, h;
	widget_type_e widget_type;

	data->is.field.gbar_dirty = 0;

	if (data->is.field.deleted) {
		DbgPrint("Box is deleted, ignore update\n");
		return;
	}

	gbar_content = elm_object_part_content_get(data->gbar_layout, "gbar,content");
	if (!gbar_content) {
		ErrPrint("Failed to get content object\n");
		return;
	}

	if (widget_viewer_get_glance_bar_size(data->handle, &w, &h) != WIDGET_ERROR_NONE) {
		ErrPrint("Failed to get gbar_size\n");
		w = 0;
		h = 0;
	}

	widget_viewer_get_type(data->handle, 1, &widget_type);

	switch (widget_type) {
	case WIDGET_CONTENT_TYPE_RESOURCE_ID:
		if (!s_info.conf.field.force_to_buffer) {
			gbar_update_pixmap_object(data, gbar_content, w, h);
			break;
		}
	case WIDGET_CONTENT_TYPE_BUFFER:
		gbar_update_buffer_object(data, gbar_content, w, h);
		break;
	case WIDGET_CONTENT_TYPE_TEXT:
		gbar_update_text_object(data, gbar_content, w, h);
		break;
	case WIDGET_CONTENT_TYPE_UIFW:
		break;
	case WIDGET_CONTENT_TYPE_INVALID:
	default:
		ErrPrint("Invalid pd type\n");
		break;
	}
}

static void __widget_event_deleted(struct widget_data *data)
{
	struct widget_evas_event_info info;

	if (data->widget_fb) {
		widget_viewer_release_buffer(data->widget_fb);
		data->widget_fb = NULL;
	}

	if (data->gbar_fb) {
		widget_viewer_release_buffer(data->gbar_fb);
		data->gbar_fb = NULL;
	}

	if (data->widget_pixmap) {
		replace_widget_pixmap_with_image(data);
	}

	if (data->gbar_pixmap) {
		replace_gbar_pixmap_with_image(data);
	}

	DbgPrint("widget is deleted: %p (emit signal)\n", data);
	data->is.field.send_delete = 0;
	info.widget_app_id = data->widget_id;
	info.event = WIDGET_EVENT_DELETED;
	info.error = data->is.field.faulted ? WIDGET_ERROR_FAULT : WIDGET_ERROR_NONE;

	/**
	 * Even if the widget object tries to be deleted from WIDGET_DELETED event callback,
	 * widget data should not be released while processing RAW_DELETE event handling
	 */
	widget_ref(data);

	smart_callback_call(data, WIDGET_SMART_SIGNAL_WIDGET_DELETED, &info);
	DbgPrint("Invoke raw delete %s\n", data->widget_id);
	(void)invoke_raw_event_callback(WIDGET_VIEWER_EVAS_RAW_DELETE, data->widget_id, data->widget, info.error);

	remove_widget_dirty_object_list(data);
	remove_gbar_dirty_object_list(data); /* For the safety */

	data->handle = NULL;

	/**
	 * All event handler is handled correctly,
	 * Then decrease the refcnt of it.
	 */
	widget_unref(data);
}

static void __widget_event_request_close_gbar(struct widget_data *data)
{
	int ret;

	ret = widget_viewer_destroy_glance_bar(data->handle, __widget_destroy_gbar_cb, widget_ref(data));
	if (ret < 0) {
		ErrPrint("Failed to close a GBAR: %x\n", ret);
		DbgPrint("Unref %p %s\n", data, data->widget_id);
		widget_unref(data);
	}
}

static void __widget_event_group_changed(struct widget_data *data)
{
	DbgPrint("Group is changed\n");
}

static void __widget_event_pinup_changed(struct widget_data *data)
{
	DbgPrint("Pinup is changed\n");
}

static void __widget_event_period_changed(struct widget_data *data)
{
	struct widget_evas_event_info info;

	widget_viewer_get_period(data->handle, &(data->period));
	DbgPrint("Update period is changed to (%lf)\n", data->period);

	info.widget_app_id = data->widget_id;
	info.event = WIDGET_EVENT_PERIOD_CHANGED;
	info.error = WIDGET_ERROR_NONE;
	smart_callback_call(data, WIDGET_SMART_SIGNAL_PERIOD_CHANGED, &info);
}

static void __widget_event_widget_size_changed(struct widget_data *data)
{
	DbgPrint("widget LB size is changed\n");
}

static void __widget_event_gbar_size_changed(struct widget_data *data)
{
	DbgPrint("widget GBAR size is changed\n");
}

static void __widget_event_gbar_created(struct widget_data *data)
{
	DbgPrint("widget GBAR is created\n");
}

static void __widget_event_gbar_destroyed(struct widget_data *data)
{
	DbgPrint("widget GBAR is destroyed\n");
	remove_gbar_dirty_object_list(data);
}

static void __widget_event_hold_scroll(struct widget_data *data)
{
	struct widget_evas_event_info info;
	DbgPrint("widget hold scroll\n");

	info.widget_app_id = data->widget_id;
	info.event = WIDGET_EVENT_HOLD_SCROLL;
	info.error = WIDGET_ERROR_NONE;
	smart_callback_call(data, WIDGET_SMART_SIGNAL_CONTROL_SCROLLER, &info);
}

static void __widget_event_release_scroll(struct widget_data *data)
{
	struct widget_evas_event_info info;
	DbgPrint("widget release scroll\n");

	info.widget_app_id = data->widget_id;
	info.event = WIDGET_EVENT_RELEASE_SCROLL;
	info.error = WIDGET_ERROR_NONE;
	smart_callback_call(data, WIDGET_SMART_SIGNAL_CONTROL_SCROLLER, &info);
}

static void __widget_event_widget_update_begin(struct widget_data *data)
{
	DbgPrint("WIDGET Update Begin\n");
}

static void __widget_event_widget_update_end(struct widget_data *data)
{
	DbgPrint("WIDGET Update End\n");
}

static void __widget_event_gbar_update_begin(struct widget_data *data)
{
	DbgPrint("GBAR Update Begin\n");
}

static void __widget_event_gbar_update_end(struct widget_data *data)
{
	DbgPrint("GBAR Update End\n");
}

static void __widget_event_update_mode_changed(struct widget_data *data)
{
	DbgPrint("Update mode changed\n");
}

static void __widget_event_ignored(struct widget_data *data)
{
	DbgPrint("Request is ignored\n");
}

static Evas_Object *create_widget_object(struct widget *handle)
{
	struct widget_data *data;
	const char *cluster;
	const char *sub_cluster;
	Evas_Object *widget;
	double period;

	if (widget_viewer_get_group(handle, &cluster, &sub_cluster) != WIDGET_ERROR_NONE) {
		ErrPrint("Unable to get the group info\n");
	}

	/**
	 * \TODO: Create a widget evas object
	 */

	widget_viewer_get_period(handle, &period);

	widget = widget_viewer_evas_add_widget(s_info.win,
						widget_viewer_get_pkgname(handle), widget_viewer_get_content_string(handle),
						period);

	data = evas_object_smart_data_get(widget);
	if (data) {
		widget_size_type_e type;
		int w = 0;
		int h = 0;

		data->handle = handle;

		widget_viewer_get_size_type(handle, &type);
		widget_service_get_size(type, &w, &h);
		DbgPrint("Size: %dx%d\n", w, h);
		evas_object_resize(widget, w, h);
	}

	return widget;
}

static inline int handle_subscribed_group(struct widget *handle)
{
	const char *cluster = NULL;
	const char *sub_cluster = NULL;
	Eina_List *l;
	struct subscribe_group *group;

	if (widget_viewer_get_group(handle, &cluster, &sub_cluster) != WIDGET_ERROR_NONE) {
		return WIDGET_ERROR_FAULT;
	}

	if (!cluster || !sub_cluster) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	EINA_LIST_FOREACH(s_info.subscribed_group_list, l, group) {
		if (!strcasecmp(group->cluster, cluster) && !strcasecmp(group->sub_cluster, sub_cluster)) {
			int nr;
			Evas_Object *widget;
			struct widget_data *temp_widget_data = NULL;

			DbgPrint("Subscribed Group: (%s)(%s)\n", cluster, sub_cluster);

			/* Subscribed object, Create this */
			widget = create_widget_object(handle);
			if (!widget) {
				ErrPrint("Failed to create a widget object\n");
				(void)widget_viewer_delete_widget(handle, WIDGET_DELETE_PERMANENTLY, NULL, NULL);
				return WIDGET_ERROR_FAULT;
			}

			widget_viewer_set_data(handle, widget);

			/* Emit RAW_CREATE event */
			nr = invoke_raw_event_callback(WIDGET_VIEWER_EVAS_RAW_CREATE, widget_viewer_get_pkgname(handle), widget, WIDGET_ERROR_NONE);

			if ((temp_widget_data = get_smart_data(widget)) == NULL) {
				ErrPrint("Failed to get widget data\n");
				(void)widget_viewer_delete_widget(handle, WIDGET_DELETE_PERMANENTLY, NULL, NULL);
				return WIDGET_ERROR_FAULT;
			}

			if (nr <= 0 || widget_system_created(handle, temp_widget_data) != WIDGET_ERROR_NONE) {
				/*
				 * Deleting evas object will invoke delete callback.
				 * Then it will invoke the RAW_DELETE event and execute the proper procedures for deleting object
				 */
				widget_viewer_evas_set_permanent_delete(widget, EINA_TRUE);
				evas_object_del(widget);
			}

			return WIDGET_ERROR_NONE;
		}
	}

	return WIDGET_ERROR_NOT_EXIST;
}

static inline int handle_subscribed_category(struct widget *handle)
{
	char *category;
	Eina_List *l;
	struct subscribe_category *info;

	category = widget_service_get_category(widget_viewer_get_pkgname(handle));
	if (!category) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	EINA_LIST_FOREACH(s_info.subscribed_category_list, l, info) {
		if (!strcmp(category, info->category)) {
			int nr;
			Evas_Object *widget;
			struct widget_data *temp_widget_data;

			DbgPrint("Subscribed Category: (%s)(%s)\n", category, info->category);

			widget = create_widget_object(handle);
			if (!widget) {
				ErrPrint("Failed to create a widget object\n");
				(void)widget_viewer_delete_widget(handle, WIDGET_DELETE_PERMANENTLY, NULL, NULL);
				free(category);
				return WIDGET_ERROR_FAULT;
			}

			widget_viewer_set_data(handle, widget);

			/* Subscribed object, Create this */
			nr = invoke_raw_event_callback(WIDGET_VIEWER_EVAS_RAW_CREATE, widget_viewer_get_pkgname(handle), widget, WIDGET_ERROR_NONE);

			temp_widget_data = get_smart_data(widget);
			if (!temp_widget_data) {
				ErrPrint("Failed to get widget data\n");
				(void)widget_viewer_delete_widget(handle, WIDGET_DELETE_PERMANENTLY, NULL, NULL);
				evas_object_del(widget);
				free(category);
				return WIDGET_ERROR_FAULT;
			}

			if (nr <= 0 || widget_system_created(handle, temp_widget_data) != WIDGET_ERROR_NONE) {
				/* Delete widget, if no one cares it */
				DbgPrint("No one cares\n");
				widget_viewer_evas_set_permanent_delete(widget, EINA_TRUE);
				evas_object_del(widget);
			}

			free(category);
			return WIDGET_ERROR_NONE;
		}
	}

	free(category);
	return WIDGET_ERROR_NOT_EXIST;
}

static int widget_event_handler(struct widget *handle, enum widget_event_type event, void *cbdata)
{
	Evas_Object *widget;
	struct widget_data *data;
	int idx;
	unsigned int resource_id;

	widget = widget_viewer_get_data(handle);
	if (widget) {
		data = get_smart_data(widget);
	} else {
		data = NULL;
		DbgPrint("widget Object is not exists, create it?\n");
	}

	if (!widget || !data || data->is.field.deleted) {
		widget_viewer_set_data(handle, NULL);

		if (event == WIDGET_EVENT_CREATED) {
			if (handle_subscribed_group(handle) == WIDGET_ERROR_NONE) {
				return 0;
			}

			if (handle_subscribed_category(handle) == WIDGET_ERROR_NONE) {
				return 0;
			}

			DbgPrint("System created widget is not supported\n");
			(void)widget_viewer_delete_widget(handle, WIDGET_DELETE_PERMANENTLY, NULL, NULL);
		} else {
			ErrPrint("Failed to get smart data\n");
		}

		return 0;
	}

	switch ((int)event) {
	case WIDGET_EVENT_WIDGET_EXTRA_BUFFER_CREATED:
		widget_viewer_get_affected_extra_buffer(handle, 0, &idx, &resource_id);
		DbgPrint("Extra buffer created for WIDGET: %d (%u)\n", idx, resource_id);
		if (resource_id == 0u) {
			ErrPrint("Invalid resource id: %d\n", idx);
			break;
		}

		if (!s_info.conf.field.skip_acquire) {
			int status;

			status = widget_viewer_acquire_extra_resource_id(handle, 0, idx, acquire_widget_extra_resource_cb, data);
			if (status < 0) {
				ErrPrint("Failed to acquire resource: %u (0x%X)\n", resource_id, status);
				break;
			}
		}

		if (!data->widget_extra) {
			data->widget_extra = calloc(widget_viewer_get_option(WIDGET_OPTION_EXTRA_BUFFER_CNT), sizeof(*data->widget_extra));
			if (!data->widget_extra) {
				ErrPrint("calloc: %d\n", errno);
				break;
			}
		}

		data->widget_extra[idx] = resource_id;
		data->widget_extra_cnt++;
		break;
	case WIDGET_EVENT_WIDGET_EXTRA_BUFFER_DESTROYED:
		widget_viewer_get_affected_extra_buffer(handle, 0, &idx, &resource_id);
		DbgPrint("Extra buffer destroyed for WIDGET: %d (%u)\n", idx, resource_id);
		if (resource_id == 0u) {
			ErrPrint("Invalid resource id: %d\n", idx);
			break;
		}

		if (data->widget_extra[idx] != resource_id) {
			DbgPrint("Resource Id mismatched\n");
			if (data->widget_extra[idx] == 0u) {
				DbgPrint("Not acquired resourced\n");
				break;
			}
		}

		data->widget_extra[idx] = 0u;
		data->widget_extra_cnt--;
		if (!data->widget_extra_cnt) {
			DbgPrint("Release widget array\n");
			free(data->widget_extra);
			data->widget_extra = NULL;
		}

		if (!s_info.conf.field.skip_acquire) {
			if (widget_viewer_release_resource_id(handle, 0, resource_id) < 0) {
				ErrPrint("Failed to release resource: %u\n", resource_id);
			}
		}
		break;
	case WIDGET_EVENT_GBAR_EXTRA_BUFFER_CREATED:
		widget_viewer_get_affected_extra_buffer(handle, 1, &idx, &resource_id);
		DbgPrint("Extra buffer destroyed for GBAR: %d (%u)\n", idx, resource_id);
		if (resource_id == 0u) {
			ErrPrint("Invalid resource id: %d\n", idx);
			break;
		}

		if (!data->gbar_extra) {
			data->gbar_extra = calloc(widget_viewer_get_option(WIDGET_OPTION_EXTRA_BUFFER_CNT), sizeof(*data->gbar_extra));
			if (!data->gbar_extra) {
				ErrPrint("calloc: %d\n", errno);
				break;
			}
		}

		data->gbar_extra[idx] = resource_id;
		data->gbar_extra_cnt++;

		if (!s_info.conf.field.skip_acquire) {
			if (widget_viewer_acquire_extra_resource_id(handle, 1, idx, acquire_gbar_extra_resource_cb, data) < 0) {
				ErrPrint("Failed to acquire resource: %u\n", resource_id);
			}
		}
		break;
	case WIDGET_EVENT_GBAR_EXTRA_BUFFER_DESTROYED:
		widget_viewer_get_affected_extra_buffer(handle, 1, &idx, &resource_id);
		DbgPrint("Extra buffer destroyed for GBAR: %d (%u)\n", idx, resource_id);
		if (resource_id == 0u) {
			ErrPrint("Invalid resource id: %d\n", idx);
			break;
		}

		if (data->gbar_extra[idx] != resource_id) {
			DbgPrint("Resource Id mismatched\n");
		}

		data->gbar_extra[idx] = 0u;
		data->gbar_extra_cnt--;
		if (!data->gbar_extra_cnt) {
			DbgPrint("Release gbar array\n");
			free(data->gbar_extra);
			data->gbar_extra = NULL;
		}

		if (!s_info.conf.field.skip_acquire) {
			if (widget_viewer_release_resource_id(handle, 1, resource_id) < 0) {
				ErrPrint("Failed to release resource: %u\n", resource_id);
			}
		}
		break;
	case WIDGET_EVENT_WIDGET_EXTRA_UPDATED:
		widget_viewer_get_affected_extra_buffer(handle, 0, &idx, &resource_id);
		if (!data->widget_extra) {
			ErrPrint("Extra buffer is not prepared yet\n");
		} else {
			if (data->widget_extra[idx] != resource_id) {
				ErrPrint("Resource ID mismatched\n");
			}
			append_widget_dirty_object_list(data, idx);
		}
		break;
	case WIDGET_EVENT_GBAR_EXTRA_UPDATED:
		widget_viewer_get_affected_extra_buffer(handle, 1, &idx, &resource_id);
		if (!data->gbar_extra) {
			ErrPrint("Extra buffer is not prepared yet\n");
		} else {
			if (data->gbar_extra[idx] != resource_id) {
				ErrPrint("Resource ID mismatched\n");
			}
			append_gbar_dirty_object_list(data, idx);
		}
		break;
	case WIDGET_EVENT_WIDGET_UPDATED:
		append_widget_dirty_object_list(data, WIDGET_PRIMARY_BUFFER);
		break;
	case WIDGET_EVENT_GBAR_UPDATED:
		append_gbar_dirty_object_list(data, WIDGET_PRIMARY_BUFFER);
		break;
	case WIDGET_EVENT_EXTRA_INFO_UPDATED:
		__widget_event_extra_info_updated(data);
		break;

	case WIDGET_EVENT_DELETED:
		__widget_event_deleted(data);
		break;

	case WIDGET_EVENT_GROUP_CHANGED:
		__widget_event_group_changed(data);
		break;
	case WIDGET_EVENT_PINUP_CHANGED:
		__widget_event_pinup_changed(data);
		break;
	case WIDGET_EVENT_PERIOD_CHANGED:
		__widget_event_period_changed(data);
		break;

	case WIDGET_EVENT_WIDGET_SIZE_CHANGED:
		__widget_event_widget_size_changed(data);
		break;
	case WIDGET_EVENT_GBAR_SIZE_CHANGED:
		__widget_event_gbar_size_changed(data);
		break;

	case WIDGET_EVENT_GBAR_CREATED:
		__widget_event_gbar_created(data);
		break;
	case WIDGET_EVENT_GBAR_DESTROYED:
		__widget_event_gbar_destroyed(data);
		break;

	case WIDGET_EVENT_HOLD_SCROLL:
		__widget_event_hold_scroll(data);
		break;
	case WIDGET_EVENT_RELEASE_SCROLL:
		__widget_event_release_scroll(data);
		break;

	case WIDGET_EVENT_WIDGET_UPDATE_BEGIN:
		__widget_event_widget_update_begin(data);
		break;
	case WIDGET_EVENT_WIDGET_UPDATE_END:
		__widget_event_widget_update_end(data);
		break;

	case WIDGET_EVENT_GBAR_UPDATE_BEGIN:
		__widget_event_gbar_update_begin(data);
		break;
	case WIDGET_EVENT_GBAR_UPDATE_END:
		__widget_event_gbar_update_end(data);
		break;

	case WIDGET_EVENT_UPDATE_MODE_CHANGED:
		__widget_event_update_mode_changed(data);
		break;

	case WIDGET_EVENT_REQUEST_CLOSE_GBAR:
		__widget_event_request_close_gbar(data);
		break;

	case WIDGET_EVENT_IGNORED:
		__widget_event_ignored(data);
		break;
	default:
		break;
	}

	return 0;
}

static int widget_fault_handler(enum widget_fault_type fault, const char *pkgname, const char *filename, const char *funcname, void *cbdata)
{
	Eina_List *l = NULL;
	Evas_Object *widget;
	struct widget_data *data;
	struct widget_evas_event_info info;

	switch (fault) {
	case WIDGET_FAULT_DEACTIVATED:
		EINA_LIST_FOREACH(s_info.list, l, widget) {
			data = get_smart_data(widget);
			if (!data) {
				continue;
			}

			if (!strcmp(data->widget_id, pkgname)) {
				DbgPrint("Faulted: %s (%p)\n", pkgname, data);
				data->is.field.faulted = 1;

				info.error = WIDGET_ERROR_FAULT;
				info.widget_app_id = data->widget_id;
				info.event = WIDGET_FAULT_DEACTIVATED;

				smart_callback_call(data, WIDGET_SMART_SIGNAL_WIDGET_FAULTED, &info);

				__widget_overlay_faulted(data);
			}
		}
		break;
	case WIDGET_FAULT_PROVIDER_DISCONNECTED:
		EINA_LIST_FOREACH(s_info.list, l, widget) {
			data = get_smart_data(widget);
			if (!data) {
				continue;
			}

			if (!strcmp(data->widget_id, pkgname)) {
				DbgPrint("Disconnected: %s (%p)\n", pkgname, data);
				data->is.field.faulted = 1;

				info.error = WIDGET_ERROR_FAULT;
				info.widget_app_id = data->widget_id;
				info.event = WIDGET_FAULT_PROVIDER_DISCONNECTED;

				smart_callback_call(data, WIDGET_SMART_SIGNAL_PROVIDER_DISCONNECTED, &info);

				__widget_overlay_faulted(data);
			}
		}
		break;
	default:
		break;
	}
	return 0;
}

static void evas_render_post_cb(void *data, Evas *e, void *event_info)
{
	widget_resource_lock_t handle;

	EINA_LIST_FREE(s_info.updated_pixmap_lock_list, handle) {
		widget_service_release_resource_lock(handle);
		widget_service_destroy_resource_lock(handle, 0);
	}
}

static void evas_render_pre_cb(void *data, Evas *e, void *event_info)
{
	widget_resource_lock_t handle;
	void *pixmap;

	EINA_LIST_FREE(s_info.updated_pixmap_list, pixmap) {
		handle = widget_service_create_resource_lock((unsigned int)(PTR_TYPE(pixmap)), WIDGET_LOCK_READ);
		if (handle) {
			widget_service_acquire_resource_lock(handle);
			s_info.updated_pixmap_lock_list = eina_list_append(s_info.updated_pixmap_lock_list, handle);
		} else {
			ErrPrint("Failed to create a resource lock\n");
		}
	}
}

EAPI int widget_viewer_evas_init(Evas_Object *win)
{
	int ret;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (!win) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		return WIDGET_ERROR_PERMISSION_DENIED;
	}

	if (s_info.initialized) {
		ErrPrint("Already initialized\n");
		return WIDGET_ERROR_ALREADY_EXIST;
	}

	if (!bindtextdomain(PKGNAME, WIDGET_EVAS_RESOURCE_PO)) {
		ErrPrint("bindtextdomain: %d\n", errno);
	} else {
		DbgPrint("%s - %s\n", PKGNAME, WIDGET_EVAS_RESOURCE_PO);
	}

	util_screen_size_get(&s_info.screen_width, &s_info.screen_height);

	s_info.conf.field.render_animator = 0;	/* By default, use render animator for updating */

	ret = widget_viewer_init(util_display_get(), 1, 0.001f, 1);
	if (ret < 0) {
		return ret;
	}

	ret = widget_viewer_add_event_handler(widget_event_handler, NULL);
	if (ret != WIDGET_ERROR_NONE) {
		ErrPrint("Failed to set handler\n");
		widget_viewer_fini();
	} else {
		DbgPrint("Event handler registered\n");
		ret = widget_viewer_add_fault_handler(widget_fault_handler, NULL);
		if (ret != WIDGET_ERROR_NONE) {
			ErrPrint("Failed to set fault handler\n");
			widget_viewer_remove_event_handler(widget_event_handler);
			widget_viewer_fini();
		} else {
			DbgPrint("Fault handler is registered\n");
			s_info.initialized = 1;
			widget_conf_init();
			widget_conf_load();
		}
	}

	/* s_info.conf.field.force_to_buffer = force_to_buffer; */
	s_info.conf.field.force_to_buffer = 0;
	s_info.win = win;

	if (WIDGET_CONF_ENABLE_RESOURCE_LOCK) {
		Evas *e;
		e = evas_object_evas_get(win);
		if (e) {
			evas_event_callback_add(e, EVAS_CALLBACK_RENDER_PRE, evas_render_pre_cb, NULL);
			evas_event_callback_add(e, EVAS_CALLBACK_RENDER_POST, evas_render_post_cb, NULL);
		}
	}

	return ret;
}

EAPI int widget_viewer_evas_fini(void)
{
	int ret;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		return WIDGET_ERROR_PERMISSION_DENIED;
	}

	if (s_info.initialized) {
		if (WIDGET_CONF_ENABLE_RESOURCE_LOCK && s_info.win) {
			Evas *e;
			e = evas_object_evas_get(s_info.win);
			if (e) {
				evas_event_callback_del(e, EVAS_CALLBACK_RENDER_PRE, evas_render_pre_cb);
				evas_event_callback_del(e, EVAS_CALLBACK_RENDER_POST, evas_render_post_cb);
			}
		}

		widget_viewer_remove_event_handler(widget_event_handler);
		widget_viewer_remove_fault_handler(widget_fault_handler);
		widget_viewer_fini();
		widget_conf_reset();
		s_info.initialized = 0;

		ret = WIDGET_ERROR_NONE;
	} else {
		ret = WIDGET_ERROR_FAULT;
	}

	return ret;
}

EAPI int widget_viewer_evas_notify_resumed_status_of_viewer(void)
{
	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		return WIDGET_ERROR_PERMISSION_DENIED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	return widget_viewer_notify_resumed_status_of_viewer();
}

EAPI int widget_viewer_evas_notify_paused_status_of_viewer(void)
{
	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		return WIDGET_ERROR_PERMISSION_DENIED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	return widget_viewer_notify_paused_status_of_viewer();
}

EAPI int widget_viewer_evas_notify_orientation_of_viewer(int orientation)
{
	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		return WIDGET_ERROR_PERMISSION_DENIED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	return widget_viewer_notify_orientation_of_viewer(orientation);
}

EAPI Evas_Object *widget_viewer_evas_add_widget(Evas_Object *parent, const char *widget_id, const char *content_info, double period)
{
	struct widget_data *data;
	Evas_Object *widget;
	char *_widget_id;
	char *_content_info;
	char *_cluster;
	char *_category;
	char *cluster = DEFAULT_CLUSTER;
	char *category = DEFAULT_CATEGORY;

	if (!is_widget_feature_enabled()) {
		ErrPrint("feature is not supported\n");
		set_last_result(WIDGET_ERROR_NOT_SUPPORTED);
		return NULL;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		ErrPrint("Permission denied\n");
		set_last_result(WIDGET_ERROR_PERMISSION_DENIED);
		return NULL;
	}

	if (!s_info.initialized) {
		ErrPrint("Viewer is not initialized\n");
		return NULL;
	}

	if (!parent || !widget_id) {
		return NULL;
	}

	_cluster = strdup(cluster);
	if (!_cluster) {
		ErrPrint("Heap: %d\n", errno);
		return NULL;
	}

	_category = strdup(category);
	if (!_category) {
		ErrPrint("Heap: %d\n", errno);
		free(_cluster);
		return NULL;
	}

	_widget_id = strdup(widget_id);
	if (!_widget_id) {
		ErrPrint("Heap: %d\n", errno);
		free(_category);
		free(_cluster);
		return NULL;
	}

	if (content_info) {
		_content_info = strdup(content_info);
		if (!_content_info) {
			ErrPrint("Heap: %d\n", errno);
			free(_widget_id);
			free(_category);
			free(_cluster);
			return NULL;
		}
	} else {
		_content_info = NULL;
	}

	if (!s_info.smart) {
		s_info.sc.add = __widget_add;
		s_info.sc.del = __widget_del;
		s_info.sc.move = __widget_move;
		s_info.sc.resize = __widget_resize;
		s_info.sc.show = __widget_show;
		s_info.sc.hide = __widget_hide;
		s_info.sc.color_set = __widget_color_set;
		s_info.sc.clip_set = __widget_clip_set;
		s_info.sc.clip_unset = __widget_clip_unset;

		s_info.smart = evas_smart_class_new(&s_info.sc);
	}

	widget = evas_object_smart_add(evas_object_evas_get(parent), s_info.smart);

	data = evas_object_smart_data_get(widget);
	if (data) {
		data->parent = parent;
		data->widget_id = _widget_id;
		data->content = _content_info;
		data->cluster = _cluster;
		data->category = _category;
		data->is.field.mouse_event = 0;
		data->period = period;

		__widget_data_setup(data);
	} else {
		ErrPrint("Failed to get smart data\n");
		free(_widget_id);
		free(_content_info);
		free(_cluster);
		free(_category);
	}

	return widget;
}

EAPI int widget_viewer_evas_set_view_port(Evas_Object *widget, int x, int y, int w, int h)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	data = get_smart_data(widget);
	if (!data) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	data->view_port.x = x;
	data->view_port.y = y;
	data->view_port.w = w;
	data->view_port.h = h;
	s_info.conf.field.user_view_port = 1;
	return WIDGET_ERROR_NONE;
}

EAPI int widget_viewer_evas_get_view_port(Evas_Object *widget, int *x, int *y, int *w, int *h)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	data = get_smart_data(widget);
	if (!data) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (x) {
		*x = data->view_port.x;
	}

	if (y) {
		*y = data->view_port.y;
	}

	if (w) {
		*w = data->view_port.w;
	}

	if (h) {
		*h = data->view_port.h;
	}

	return WIDGET_ERROR_NONE;
}

EAPI int widget_viewer_evas_set_option(widget_evas_conf_e type, int value)
{
	int ret = WIDGET_ERROR_NONE;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		return WIDGET_ERROR_PERMISSION_DENIED;
	}

	switch ((int)type) {
	case WIDGET_VIEWER_EVAS_SENSITIVE_MOVE:
		s_info.conf.field.sensitive_move = value;
		break;
	case WIDGET_VIEWER_EVAS_EVENT_AUTO_FEED:
		s_info.conf.field.auto_feed = value;
		break;
	case WIDGET_VIEWER_EVAS_EASY_MODE:
		s_info.conf.field.easy_mode = value;
		break;
	case WIDGET_VIEWER_EVAS_USE_FIXED_SIZE:
		s_info.conf.field.use_fixed_size = value;
		break;
	case WIDGET_VIEWER_EVAS_MANUAL_PAUSE_RESUME:
		s_info.conf.field.manual_pause_resume = value;
		break;
	case WIDGET_VIEWER_EVAS_SHARED_CONTENT:
		(void)widget_viewer_set_option(WIDGET_OPTION_SHARED_CONTENT, value);
		break;
	case WIDGET_VIEWER_EVAS_SUPPORT_GBAR:
		s_info.conf.field.support_gbar = value;
		break;
	case WIDGET_VIEWER_EVAS_SCROLL_X:
		s_info.conf.field.is_scroll_x = value;
		break;
	case WIDGET_VIEWER_EVAS_SCROLL_Y:
		s_info.conf.field.is_scroll_y = value;
		break;
	case WIDGET_VIEWER_EVAS_DELAYED_RESUME:
		s_info.conf.field.delayed_resume = value;
		break;
	case WIDGET_VIEWER_EVAS_AUTO_RENDER_SELECTION:
		s_info.conf.field.auto_render_selector = value;
		break;
	case WIDGET_VIEWER_EVAS_DIRECT_UPDATE:
		(void)widget_viewer_set_option(WIDGET_OPTION_DIRECT_UPDATE, !!value);
		break;
	case WIDGET_VIEWER_EVAS_USE_RENDER_ANIMATOR:
		if (s_info.conf.field.auto_render_selector) {
			DbgPrint("Auto selector enabled, this render_animator option will be changed automatically\n");
		}

		s_info.conf.field.render_animator = !!value;
		DbgPrint("Turn %s render animator\n", s_info.conf.field.render_animator ? "on" : "off");
		break;
	case WIDGET_VIEWER_EVAS_SKIP_ACQUIRE:
		s_info.conf.field.skip_acquire = !!value;
		DbgPrint("Turn %s skip-acquire option\n", s_info.conf.field.skip_acquire ? "on" : "off");
		break;
	case WIDGET_VIEWER_EVAS_DISABLE_SCROLLER:
		s_info.conf.field.disable_internal_scroller = !!value;
		DbgPrint("Turn %s internal_scroller option\n", s_info.conf.field.disable_internal_scroller ? "off" : "on");
		break;
	default:
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		break;
	}

	return ret;
}

EAPI int widget_viewer_evas_pause_widget(Evas_Object *widget)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		return WIDGET_ERROR_PERMISSION_DENIED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	data = get_smart_data(widget);
	if (!data || !data->is.field.created || !data->handle) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (data->delayed_resume_timer) {
		(void)ecore_timer_del(data->delayed_resume_timer);
		data->delayed_resume_timer = NULL;
	}

	return widget_viewer_set_visibility(data->handle, WIDGET_HIDE_WITH_PAUSE);
}

EAPI int widget_viewer_evas_resume_widget(Evas_Object *widget)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		return WIDGET_ERROR_PERMISSION_DENIED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	data = get_smart_data(widget);
	if (!data || !data->is.field.created || !data->handle) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (data->delayed_resume_timer) {
		(void)ecore_timer_del(data->delayed_resume_timer);
		data->delayed_resume_timer = NULL;
	}

	data->is.field.initial_resumed = 1;
	return widget_viewer_set_visibility(data->handle, WIDGET_SHOW);
}

EAPI int widget_viewer_evas_destroy_glance_bar(Evas_Object *widget)
{
	struct widget_data *data;
	int ret;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	data = get_smart_data(widget);
	if (!data || data->state != WIDGET_DATA_CREATED || !data->is.field.created || !data->handle || !data->is.field.gbar_created) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	ret = widget_viewer_destroy_glance_bar(data->handle, __widget_destroy_gbar_cb, widget_ref(data));
	if (ret < 0) {
		widget_unref(data);
	}

	return ret;
}

EAPI const char *widget_viewer_evas_get_content_info(Evas_Object *widget)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		set_last_result(WIDGET_ERROR_NOT_SUPPORTED);
		return NULL;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		set_last_result(WIDGET_ERROR_PERMISSION_DENIED);
		return NULL;
	}

	if (!s_info.initialized) {
		return NULL;
	}

	data = get_smart_data(widget);
	if (!data || !data->is.field.created || !data->handle) {
		return NULL;
	}

	return widget_viewer_get_content_string(data->handle);
}

EAPI const char *widget_viewer_evas_get_title_string(Evas_Object *widget)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		set_last_result(WIDGET_ERROR_NOT_SUPPORTED);
		return NULL;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		set_last_result(WIDGET_ERROR_PERMISSION_DENIED);
		return NULL;
	}

	if (!s_info.initialized) {
		return NULL;
	}

	data = get_smart_data(widget);
	if (!data || !data->is.field.created || !data->handle) {
		return NULL;
	}

	return widget_viewer_get_title_string(data->handle);
}

EAPI const char *widget_viewer_evas_get_widget_id(Evas_Object *widget)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		set_last_result(WIDGET_ERROR_NOT_SUPPORTED);
		return NULL;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		set_last_result(WIDGET_ERROR_PERMISSION_DENIED);
		return NULL;
	}

	if (!s_info.initialized) {
		return NULL;
	}

	data = get_smart_data(widget);
	if (!data || data->state != WIDGET_DATA_CREATED) {
		return NULL;
	}

	return data->widget_id;
}

EAPI double widget_viewer_evas_get_period(Evas_Object *widget)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		set_last_result(WIDGET_ERROR_NOT_SUPPORTED);
		return 0.0f;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		set_last_result(WIDGET_ERROR_PERMISSION_DENIED);
		return 0.0f;
	}

	if (!s_info.initialized) {
		return 0.0f;
	}

	data = get_smart_data(widget);
	if (!data || !data->is.field.created || !data->handle) {
		return 0.0f;
	}

	return data->period;
}

EAPI void widget_viewer_evas_cancel_click_event(Evas_Object *widget)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		set_last_result(WIDGET_ERROR_NOT_SUPPORTED);
		return;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		set_last_result(WIDGET_ERROR_PERMISSION_DENIED);
		return;
	}

	if (!s_info.initialized) {
		return;
	}

	data = get_smart_data(widget);
	if (!data || !data->is.field.created || !data->handle) {
		return;
	}

	if (!data->pressed) {
		DbgPrint("Cancel ignored\n");
		return;
	}

	if (data->is.field.cancel_click == CANCEL_DISABLED) {
		data->is.field.cancel_click = CANCEL_USER;
	}
}

#if defined(ELM_ACCESS_ENABLED)
static void access_ret_cb(struct widget *handle, int ret, void *data)
{
	struct access_ret_cb_data *cb_data = data;

	switch (ret) {
	case WIDGET_ACCESS_STATUS_ERROR:
		ret = WIDGET_ACCESS_RESULT_ERROR;
		break;
	case WIDGET_ACCESS_STATUS_DONE:
		ret = WIDGET_ACCESS_RESULT_DONE;
		break;
	case WIDGET_ACCESS_STATUS_FIRST:
		ret = WIDGET_ACCESS_RESULT_FIRST;
		break;
	case WIDGET_ACCESS_STATUS_LAST:
		ret = WIDGET_ACCESS_RESULT_LAST;
		break;
	case WIDGET_ACCESS_STATUS_READ:
		ret = WIDGET_ACCESS_RESULT_READ;
		break;
	default:
		ret = WIDGET_ACCESS_RESULT_UNKNOWN;
		break;
	}

	if (cb_data->ret_cb) {
		cb_data->ret_cb(cb_data->obj, ret, cb_data->data);
	}

	free(cb_data);
}
#endif

EAPI int widget_viewer_evas_feed_mouse_up_event(Evas_Object *widget)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		return WIDGET_ERROR_PERMISSION_DENIED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	data = get_smart_data(widget);
	if (!data || !data->is.field.created || !data->handle) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return do_force_mouse_up(data);
}

EAPI int widget_viewer_evas_feed_access_event(Evas_Object *widget, int type, void *_info, void (*ret_cb)(Evas_Object *obj, int ret, void *data), void *cbdata)
{
	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

#if defined(ELM_ACCESS_ENABLED)
	struct widget_data *data;
	Elm_Access_Action_Info *info = _info;
	int w;
	int h;
	struct access_ret_cb_data *cb_data;
	int ret;
	struct widget_access_event_info ainfo;

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	data = get_smart_data(widget);
	if (!data || !data->is.field.created || !data->handle) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	evas_object_geometry_get(data->widget_layout, NULL, NULL, &w, &h);
	ainfo.x = (double)info->x / (double)w;
	ainfo.y = (double)info->y / (double)h;
	ainfo.info = 0;

	switch (type) {
	case ELM_ACCESS_ACTION_HIGHLIGHT: /* highlight an object */
		DbgPrint("Highlight %dx%d ignored\n", info->x, info->y);
		break;
	case ELM_ACCESS_ACTION_READ:
		cb_data = calloc(1, sizeof(*cb_data));
		if (!cb_data) {
			ErrPrint("Heap: %d\n", errno);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}

		cb_data->ret_cb = ret_cb;
		cb_data->data = cbdata;
		cb_data->obj = widget;

		ainfo.type = WIDGET_ACCESS_TYPE_HIGHLIGHT;
		ret = widget_viewer_feed_access_event(data->handle, WIDGET_ACCESS_HIGHLIGHT, &ainfo, access_ret_cb, cb_data);
		if (ret != WIDGET_ERROR_NONE) {
			free(cb_data);
		}
		break;
	case ELM_ACCESS_ACTION_UNHIGHLIGHT: /* unhighlight an object */
		cb_data = calloc(1, sizeof(*cb_data));
		if (!cb_data) {
			ErrPrint("Heap: %d\n", errno);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}

		cb_data->ret_cb = ret_cb;
		cb_data->data = cbdata;
		cb_data->obj = widget;

		ainfo.type = WIDGET_ACCESS_TYPE_UNHIGHLIGHT;
		ret = widget_viewer_feed_access_event(data->handle, WIDGET_ACCESS_HIGHLIGHT, &ainfo, access_ret_cb, cb_data);
		if (ret != WIDGET_ERROR_NONE) {
			free(cb_data);
		}
		break;
	case ELM_ACCESS_ACTION_HIGHLIGHT_NEXT: /* set highlight to next object */
		cb_data = calloc(1, sizeof(*cb_data));
		if (!cb_data) {
			ErrPrint("Heap: %d\n", errno);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}

		cb_data->ret_cb = ret_cb;
		cb_data->data = cbdata;
		cb_data->obj = widget;

		ainfo.type = WIDGET_ACCESS_TYPE_HIGHLIGHT_NEXT;
		ret = widget_viewer_feed_access_event(data->handle, WIDGET_ACCESS_HIGHLIGHT, &ainfo, access_ret_cb, cb_data);
		if (ret != WIDGET_ERROR_NONE) {
			free(cb_data);
		}
		break;
	case ELM_ACCESS_ACTION_HIGHLIGHT_PREV: /* set highlight to previous object */
		cb_data = calloc(1, sizeof(*cb_data));
		if (!cb_data) {
			ErrPrint("Heap: %d\n", errno);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}

		cb_data->ret_cb = ret_cb;
		cb_data->data = cbdata;
		cb_data->obj = widget;

		ainfo.type = WIDGET_ACCESS_TYPE_HIGHLIGHT_NEXT;
		ret = widget_viewer_feed_access_event(data->handle, WIDGET_ACCESS_HIGHLIGHT, &ainfo, access_ret_cb, cb_data);
		if (ret != WIDGET_ERROR_NONE) {
			free(cb_data);
		}
		break;
	case ELM_ACCESS_ACTION_ACTIVATE: /* activate a highlight object */
		cb_data = calloc(1, sizeof(*cb_data));
		if (!cb_data) {
			ErrPrint("Heap: %d\n", errno);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}

		cb_data->ret_cb = ret_cb;
		cb_data->data = cbdata;
		cb_data->obj = widget;

		ainfo.type = WIDGET_ACCESS_TYPE_NONE; /* meaningless */
		ret = widget_viewer_feed_access_event(data->handle, WIDGET_ACCESS_ACTIVATE, &ainfo, access_ret_cb, cb_data);
		if (ret != WIDGET_ERROR_NONE) {
			free(cb_data);
		}
		break;
	case ELM_ACCESS_ACTION_SCROLL: /* scroll if one of highlight object parents * is scrollable */
		cb_data = calloc(1, sizeof(*cb_data));
		if (!cb_data) {
			ErrPrint("Heap: %d\n", errno);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}

		cb_data->ret_cb = ret_cb;
		cb_data->data = cbdata;
		cb_data->obj = widget;

		switch (info->mouse_type) {
		case 0:
			ainfo.type = WIDGET_ACCESS_TYPE_DOWN;
			ret = widget_viewer_feed_access_event(data->handle, WIDGET_ACCESS_SCROLL, &ainfo, access_ret_cb, cb_data);
			if (ret != WIDGET_ERROR_NONE) {
				free(cb_data);
			}
			break;
		case 1:
			ainfo.type = WIDGET_ACCESS_TYPE_MOVE;
			ret = widget_viewer_feed_access_event(data->handle, WIDGET_ACCESS_SCROLL, &ainfo, access_ret_cb, cb_data);
			if (ret != WIDGET_ERROR_NONE) {
				free(cb_data);
			}
			break;
		case 2:
			ainfo.type = WIDGET_ACCESS_TYPE_UP;
			ret = widget_viewer_feed_access_event(data->handle, WIDGET_ACCESS_SCROLL, &ainfo, access_ret_cb, cb_data);
			if (ret != WIDGET_ERROR_NONE) {
				free(cb_data);
			}
			break;
		default:
			ret = WIDGET_ERROR_INVALID_PARAMETER;
			free(cb_data);
			break;
		}
		break;
	case ELM_ACCESS_ACTION_MOUSE: /* give mouse event to highlight object */
		cb_data = calloc(1, sizeof(*cb_data));
		if (!cb_data) {
			ErrPrint("Heap: %d\n", errno);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}

		cb_data->ret_cb = ret_cb;
		cb_data->data = cbdata;
		cb_data->obj = widget;
		ainfo.type = WIDGET_ACCESS_TYPE_NONE;
		ret = widget_viewer_feed_access_event(data->handle, WIDGET_ACCESS_MOUSE, &ainfo, access_ret_cb, cb_data);
		if (ret != WIDGET_ERROR_NONE) {
			free(cb_data);
		}
		break;
	case ELM_ACCESS_ACTION_UP: /* change value up of highlight object */
		cb_data = calloc(1, sizeof(*cb_data));
		if (!cb_data) {
			ErrPrint("Heap: %d\n", errno);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}

		cb_data->ret_cb = ret_cb;
		cb_data->data = cbdata;
		cb_data->obj = widget;
		ainfo.type = WIDGET_ACCESS_TYPE_UP;
		ret = widget_viewer_feed_access_event(data->handle, WIDGET_ACCESS_ACTION, &ainfo, access_ret_cb, cb_data);
		if (ret != WIDGET_ERROR_NONE) {
			free(cb_data);
		}
		break;
	case ELM_ACCESS_ACTION_DOWN: /* change value down of highlight object */
		cb_data = calloc(1, sizeof(*cb_data));
		if (!cb_data) {
			ErrPrint("Heap: %d\n", errno);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}

		cb_data->ret_cb = ret_cb;
		cb_data->data = cbdata;
		cb_data->obj = widget;

		ainfo.type = WIDGET_ACCESS_TYPE_DOWN;
		ret = widget_viewer_feed_access_event(data->handle, WIDGET_ACCESS_ACTION, &ainfo, access_ret_cb, cb_data);
		if (ret != WIDGET_ERROR_NONE) {
			free(cb_data);
		}
		break;
	case ELM_ACCESS_ACTION_VALUE_CHANGE: /* TODO: deprecate this */
		cb_data = calloc(1, sizeof(*cb_data));
		if (!cb_data) {
			ErrPrint("Heap: %d\n", errno);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}

		cb_data->ret_cb = ret_cb;
		cb_data->data = cbdata;
		cb_data->obj = widget;

		ainfo.type = WIDGET_ACCESS_TYPE_NONE;
		ret = widget_viewer_feed_access_event(data->handle, WIDGET_ACCESS_VALUE_CHANGE, &ainfo, access_ret_cb, cb_data);
		if (ret != WIDGET_ERROR_NONE) {
			free(cb_data);
		}
		break;
	case ELM_ACCESS_ACTION_BACK: /* go back to a previous view ex: pop naviframe item */
		cb_data = calloc(1, sizeof(*cb_data));
		if (!cb_data) {
			ErrPrint("Heap: %d\n", errno);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}

		cb_data->ret_cb = ret_cb;
		cb_data->data = cbdata;
		cb_data->obj = widget;

		ainfo.type = WIDGET_ACCESS_TYPE_NONE;
		ret = widget_viewer_feed_access_event(data->handle, WIDGET_ACCESS_BACK, &ainfo, access_ret_cb, cb_data);
		if (ret != WIDGET_ERROR_NONE) {
			free(cb_data);
		}
		break;
	case ELM_ACCESS_ACTION_OVER: /* mouse over an object */
		cb_data = calloc(1, sizeof(*cb_data));
		if (!cb_data) {
			ErrPrint("Heap: %d\n", errno);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}

		cb_data->ret_cb = ret_cb;
		cb_data->data = cbdata;
		cb_data->obj = widget;

		ainfo.type = WIDGET_ACCESS_TYPE_NONE;
		ret = widget_viewer_feed_access_event(data->handle, WIDGET_ACCESS_OVER, &ainfo, access_ret_cb, cb_data);
		if (ret != WIDGET_ERROR_NONE) {
			free(cb_data);
		}
		break;
	case ELM_ACCESS_ACTION_ENABLE: /* enable highlight and read ability */
		cb_data = calloc(1, sizeof(*cb_data));
		if (!cb_data) {
			ErrPrint("Heap: %d\n", errno);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}

		cb_data->ret_cb = ret_cb;
		cb_data->data = cbdata;
		cb_data->obj = widget;

		ainfo.type = WIDGET_ACCESS_TYPE_ENABLE;
		ret = widget_viewer_feed_access_event(data->handle, WIDGET_ACCESS_ENABLE, &ainfo, access_ret_cb, cb_data);
		if (ret != WIDGET_ERROR_NONE) {
			free(cb_data);
		}
		break;
	case ELM_ACCESS_ACTION_DISABLE: /* disable highlight and read ability */
		cb_data = calloc(1, sizeof(*cb_data));
		if (!cb_data) {
			ErrPrint("Heap: %d\n", errno);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}

		cb_data->ret_cb = ret_cb;
		cb_data->data = cbdata;
		cb_data->obj = widget;

		ainfo.type = WIDGET_ACCESS_TYPE_DISABLE;
		ret = widget_viewer_feed_access_event(data->handle, WIDGET_ACCESS_ENABLE, &ainfo, access_ret_cb, cb_data);
		if (ret != WIDGET_ERROR_NONE) {
			free(cb_data);
		}
		break;
	default:
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		break;
	}

	return ret;
#else
	return WIDGET_ERROR_NOT_SUPPORTED;
#endif
}

/**
 * @note
 * While displaying "loading" overlay, there is a preview image.
 * But if this function is called, the preview image will not be displayed.
 * Only the help text will be there. if it is not disabled of course.
 */
EAPI void widget_viewer_evas_disable_preview(Evas_Object *widget)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		set_last_result(WIDGET_ERROR_NOT_SUPPORTED);
		return;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		set_last_result(WIDGET_ERROR_PERMISSION_DENIED);
		return;
	}

	if (!s_info.initialized) {
		return;
	}

	data = get_smart_data(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return;
	}

	data->is.field.disable_preview = 1;
}

/**
 * @note
 * While displaying "loading" overlay, there is a help text.
 * But if this function is called, the text help message will not be displayed.
 * Only the preview image will be there. if it is not disabled of course.
 */
EAPI void widget_viewer_evas_disable_overlay_text(Evas_Object *widget)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		set_last_result(WIDGET_ERROR_NOT_SUPPORTED);
		return;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		set_last_result(WIDGET_ERROR_PERMISSION_DENIED);
		return;
	}

	if (!s_info.initialized) {
		return;
	}

	data = get_smart_data(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return;
	}

	data->is.field.disable_text = 1;
}

/**
 * @note
 * This function should be called before resize the widget object after create it.
 * If this function is called, the widget object will not display "preview" image.
 * Instead of it, the widget content object will be displayed immediately.
 */
EAPI void widget_viewer_evas_disable_loading(Evas_Object *widget)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		set_last_result(WIDGET_ERROR_NOT_SUPPORTED);
		return;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		set_last_result(WIDGET_ERROR_PERMISSION_DENIED);
		return;
	}

	if (!s_info.initialized) {
		return;
	}

	data = get_smart_data(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return;
	}

	data->is.field.disable_loading = 1;
}

EAPI void widget_viewer_evas_activate_faulted_widget(Evas_Object *widget)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		set_last_result(WIDGET_ERROR_NOT_SUPPORTED);
		return;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		set_last_result(WIDGET_ERROR_PERMISSION_DENIED);
		return;
	}

	if (!s_info.initialized) {
		return;
	}

	data = get_smart_data(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return;
	}

	if (data->is.field.faulted) {
		elm_object_signal_emit(data->widget_layout, "mouse,clicked,1", "overlay,content");
	} else {
		DbgPrint("widget is not faulted\n");
	}
}

EAPI bool widget_viewer_evas_is_faulted(Evas_Object *widget)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		set_last_result(WIDGET_ERROR_NOT_SUPPORTED);
		return false;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		set_last_result(WIDGET_ERROR_PERMISSION_DENIED);
		return false;
	}

	if (!s_info.initialized) {
		return 0;
	}

	data = get_smart_data(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return 0;
	}

	return (bool)data->is.field.faulted;
}

EAPI int widget_viewer_evas_set_raw_event_callback(widget_evas_raw_event_type_e type, raw_event_cb cb, void *data)
{
	struct raw_event_cbdata *cbdata;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	cbdata = calloc(1, sizeof(*cbdata));
	if (!cbdata) {
		ErrPrint("calloc: %d\n", errno);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	cbdata->cb = cb;
	cbdata->data = data;

	switch (type) {
	case WIDGET_VIEWER_EVAS_RAW_DELETE:
		s_info.raw_event.delete_list = eina_list_append(s_info.raw_event.delete_list, cbdata);
		break;
	case WIDGET_VIEWER_EVAS_RAW_CREATE:
		s_info.raw_event.create_list = eina_list_append(s_info.raw_event.create_list, cbdata);
		break;
	default:
		free(cbdata);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return WIDGET_ERROR_NONE;
}

EAPI int widget_viewer_evas_unset_raw_event_callback(widget_evas_raw_event_type_e type, raw_event_cb cb, void *data)
{
	Eina_List *l;
	Eina_List *n;
	struct raw_event_cbdata *cbdata;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	switch (type) {
	case WIDGET_VIEWER_EVAS_RAW_DELETE:
		EINA_LIST_FOREACH_SAFE(s_info.raw_event.delete_list, l, n, cbdata) {
			if (cbdata->cb == cb && cbdata->data == data) {
				s_info.raw_event.delete_list = eina_list_remove(s_info.raw_event.delete_list, cbdata);
				break;
			}
		}
		break;
	case WIDGET_VIEWER_EVAS_RAW_CREATE:
		EINA_LIST_FOREACH_SAFE(s_info.raw_event.create_list, l, n, cbdata) {
			if (cbdata->cb == cb && cbdata->data == data) {
				s_info.raw_event.create_list = eina_list_remove(s_info.raw_event.create_list, cbdata);
				break;
			}
		}
		break;
	default:
		break;
	}

	return WIDGET_ERROR_NONE;
}

EAPI int widget_viewer_evas_freeze_visibility(Evas_Object *widget, widget_visibility_status_e status)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		return WIDGET_ERROR_PERMISSION_DENIED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	data = get_smart_data(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	data->is.field.freeze_visibility = 1;
	data->freezed_visibility = status;
	return WIDGET_ERROR_NONE;
}

EAPI int widget_viewer_evas_thaw_visibility(Evas_Object *widget)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		return WIDGET_ERROR_PERMISSION_DENIED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	data = get_smart_data(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	data->is.field.freeze_visibility = 0;

	return WIDGET_ERROR_NONE;
}

EAPI bool widget_viewer_evas_is_visibility_frozen(Evas_Object *widget)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		set_last_result(WIDGET_ERROR_NOT_SUPPORTED);
		return false;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		set_last_result(WIDGET_ERROR_PERMISSION_DENIED);
		return false;
	}

	if (!s_info.initialized) {
		ErrPrint("Not initialized\n");
		return false;
	}

	data = get_smart_data(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return false;
	}

	return (bool)data->is.field.freeze_visibility;
}

EAPI int widget_viewer_evas_dump_to_file(Evas_Object *widget, const char *filename)
{
	struct widget_data *data;
	FILE *fp;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	data = get_smart_data(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	fp = fopen(filename, "w+");
	if (fp) {
		Evas_Object *image;
		image = elm_object_part_content_get(data->widget_layout, "widget,content");
		if (image) {
			void *data;
			Evas_Coord w, h;
			evas_object_geometry_get(image, NULL, NULL, &w, &h);

			data = evas_object_image_data_get(image, 0);
			if (data) {
				fwrite(data, w * h, sizeof(int), fp);
			}
		}
		fclose(fp);
	}

	return WIDGET_ERROR_NONE;
}

EAPI bool widget_viewer_evas_is_widget(Evas_Object *widget)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		set_last_result(WIDGET_ERROR_NOT_SUPPORTED);
		return false;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		set_last_result(WIDGET_ERROR_PERMISSION_DENIED);
		return false;
	}

	if (!s_info.initialized) {
		return false;
	}

	data = get_smart_data(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return false;
	}

	return true;
}

EAPI void widget_viewer_evas_set_permanent_delete(Evas_Object *widget, int flag)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		set_last_result(WIDGET_ERROR_NOT_SUPPORTED);
		return;
	}

	if (privilege_checker_check_privilege(WIDGET_VIEWER_PRIVILEGE) == PRIVILEGE_CHECKER_ERR_UNDECLARED_PRIVILEGE) {
		set_last_result(WIDGET_ERROR_PERMISSION_DENIED);
		return;
	}

	if (!s_info.initialized) {
		return;
	}

	data = get_smart_data(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return;
	}

	data->is.field.permanent_delete = !!flag;
}

EAPI int widget_viewer_evas_subscribe_group(const char *cluster, const char *sub_cluster)
{
	struct subscribe_group *group;
	Eina_List *l;
	int ret;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	if (!cluster || !sub_cluster) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	EINA_LIST_FOREACH(s_info.subscribed_group_list, l, group) {
		if (!strcasecmp(group->cluster, cluster) && !strcasecmp(group->sub_cluster, sub_cluster)) {
			return WIDGET_ERROR_ALREADY_EXIST;
		}
	}

	group = calloc(1, sizeof(*group));
	if (!group) {
		ErrPrint("calloc: %d\n", errno);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	group->cluster = strdup(cluster);
	if (!group->cluster) {
		ErrPrint("strdup: %d\n", errno);
		free(group);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	group->sub_cluster = strdup(sub_cluster);
	if (!group->sub_cluster) {
		ErrPrint("strdup: %d\n", errno);
		free(group->cluster);
		free(group);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	ret = widget_viewer_subscribe_group(cluster, sub_cluster);
	if (ret != WIDGET_ERROR_NONE) {
		free(group->sub_cluster);
		free(group->cluster);
		free(group);
		return ret;
	}

	s_info.subscribed_group_list = eina_list_append(s_info.subscribed_group_list, group);

	return WIDGET_ERROR_NONE;
}

EAPI int widget_viewer_evas_unsubscribe_group(const char *cluster, const char *sub_cluster)
{
	struct subscribe_group *group;
	Eina_List *l;
	Eina_List *n;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	if (!cluster || !sub_cluster) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	EINA_LIST_FOREACH_SAFE(s_info.subscribed_group_list, l, n, group) {
		if (!strcasecmp(group->cluster, cluster) && !strcasecmp(group->sub_cluster, sub_cluster)) {
			s_info.subscribed_group_list = eina_list_remove(s_info.subscribed_group_list, group);
			free(group->cluster);
			free(group->sub_cluster);
			free(group);
			return widget_viewer_unsubscribe_group(cluster, sub_cluster);
		}
	}

	return WIDGET_ERROR_NOT_EXIST;
}

EAPI int widget_viewer_evas_subscribe_category(const char *category)
{
	struct subscribe_category *item;
	Eina_List *l;
	int ret;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	if (!category) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	EINA_LIST_FOREACH(s_info.subscribed_category_list, l, item) {
		if (!strcmp(category, item->category)) {
			return WIDGET_ERROR_ALREADY_EXIST;
		}
	}

	item = calloc(1, sizeof(*item));
	if (!item) {
		ErrPrint("calloc: %d\n", errno);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	item->category = strdup(category);
	if (!item->category) {
		ErrPrint("strdup: %d\n", errno);
		free(item);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	ret = widget_viewer_subscribe_category(category);
	if (ret != WIDGET_ERROR_NONE) {
		free(item->category);
		free(item);
		return ret;
	}

	s_info.subscribed_category_list = eina_list_append(s_info.subscribed_category_list, item);
	return WIDGET_ERROR_NONE;
}

EAPI int widget_viewer_evas_unsubscribe_category(const char *category)
{
	Eina_List *l;
	Eina_List *n;
	struct subscribe_category *item;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	if (!category) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	EINA_LIST_FOREACH_SAFE(s_info.subscribed_category_list, l, n, item) {
		if (!strcmp(item->category, category)) {
			s_info.subscribed_category_list = eina_list_remove(s_info.subscribed_category_list, item);
			free(item->category);
			free(item);
			return widget_viewer_unsubscribe_category(category);
		}
	}

	return WIDGET_ERROR_NOT_EXIST;
}

static void text_signal_cb(widget_h handle, int ret, void *data)
{
	/* TODO : add codes to invoke smart event callback function */
}

EAPI int widget_viewer_evas_emit_text_signal(Evas_Object *widget, widget_text_signal_s event_info, void *data)
{
	struct widget_data *widget_data_from_evas;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	widget_data_from_evas = get_smart_data(widget);

	if (!widget_data_from_evas) {
		ErrPrint("Invalid object\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return widget_viewer_emit_text_signal(widget_data_from_evas->handle, event_info, text_signal_cb, data);
}

EAPI int widget_viewer_evas_get_instance_id(Evas_Object *widget, char **instance_id)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	data = get_smart_data(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return widget_viewer_get_instance_id(data->handle, instance_id);
}

EAPI int widget_viewer_evas_set_widget_option(Evas_Object *widget, widget_option_widget_e option, int value)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	data = get_smart_data(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	switch (option) {
	case WIDGET_OPTION_WIDGET_DELAYED_RESUME:
		data->is.field.delayed_resume = (value & 0x03);
		break;
	case WIDGET_OPTION_WIDGET_DISABLE_OVERLAY_ANIMATION:
		data->is.field.disabled_overlay_animation = !!value;
		break;
	case WIDGET_OPTION_WIDGET_MANUAL_OVERLAY_HIDE:
		data->is.field.hide_overlay_manually = !!value;
		break;
	default:
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return WIDGET_ERROR_NONE;
}

EAPI int widget_viewer_evas_set_preview_image(Evas_Object *widget, widget_size_type_e type, const char *preview)
{
	struct widget_data *data;
	Eina_List *l;
	Eina_List *n;
	struct preview_info *info;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	data = get_smart_data(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	EINA_LIST_FOREACH_SAFE(data->preview_list, l, n, info) {
		if (info->type == type) {
			char *tmp;

			if (preview) {
				tmp = strdup(preview);
				if (!tmp) {
					ErrPrint("strdup: %d\n", errno);
					return WIDGET_ERROR_OUT_OF_MEMORY;
				}

				free(info->preview);
				info->preview = tmp;
				return WIDGET_ERROR_NONE;
			}

			data->preview_list = eina_list_remove(data->preview_list, info);
			free(info->preview);
			free(info);
			return WIDGET_ERROR_NONE;
		}
	}

	info = malloc(sizeof(struct preview_info));
	if (!info) {
		ErrPrint("malloc: %d\n", errno);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	info->type = type;
	info->preview = strdup(preview);
	if (!info->preview) {
		ErrPrint("strdup: %d\n", errno);
		free(info);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	data->preview_list = eina_list_append(data->preview_list, info);
	return WIDGET_ERROR_NONE;
}

EAPI int widget_viewer_evas_hide_overlay(Evas_Object *widget)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	data = get_smart_data(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!data->is.field.widget_overlay_loaded) {
		/**
		 * Already disabled
		 */
		return WIDGET_ERROR_NONE;
	}

	if (!data->is.field.hide_overlay_manually) {
		DbgPrint("Overlay is not controlled manually\n");
	}

	if (data->overlay_timer) {
		ecore_timer_del(data->overlay_timer);
		/**
		 * @note
		 * data->overlay_timer will be reset'd to NULL by delayed_overlay_disable_cb().
		 */
	}
	delayed_overlay_disable_cb(data);

	return WIDGET_ERROR_NONE;
}

/**
 * @note
 * This function must has to be called after the object is faulted.
 * And this will not reset the faulted flag. it just dislable the faulted overlay if it is exists.
 *
 * The best usage of this function is called from smart-callback.
 * Any faulted smart callback can call this.
 * Then the widget_viewer_evas will check the flag to decide whether enable the faulted overlay or not.
 */
EAPI int widget_viewer_evas_hide_faulted_overlay_once(Evas_Object *widget)
{
	struct widget_data *data;

	if (!is_widget_feature_enabled()) {
		return WIDGET_ERROR_NOT_SUPPORTED;
	}

	if (!s_info.initialized) {
		return WIDGET_ERROR_FAULT;
	}

	data = get_smart_data(widget);
	if (!data) {
		ErrPrint("Invalid object\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (data->is.field.faulted == 0) {
		ErrPrint("Object is not faulted\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	/**
	 * @note
	 * If the overlay (faulted) is already enabled,
	 * Turn it off first.
	 */
	if (data->is.field.widget_overlay_loaded) {
		DbgPrint("Faulted overlay is disabled\n");
		__widget_overlay_disable(data, 1, 1);
	}

	/**
	 * This flag will be checked before enable the faulted overlay.
	 * And this will be reset'd right after ignore the faulted overlay.
	 */
	data->is.field.hide_faulted_overlay = 1;
	return WIDGET_ERROR_NONE;
}

/* End of a file */
