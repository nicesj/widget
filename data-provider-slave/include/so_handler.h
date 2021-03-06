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

typedef int (*create_t)(const char *filename, const char *content, const char *cluster, const char *category);
typedef int (*destroy_t)(const char *filename);
typedef int (*is_updated_t)(const char *filename);
typedef int (*need_to_destroy_t)(const char *filename);
typedef int (*update_content_t)(const char *filename);
typedef int (*clicked_t)(const char *filename, const char *event, double timestamp, double x, double y);
typedef int (*text_signal_t)(const char *filename, const char *signal_name, const char *source, widget_event_info_s event_info);
typedef int (*resize_t)(const char *filename, int type);
typedef int (*create_needed_t)(const char *cluster, const char *category);
typedef int (*change_group_t)(const char *filename, const char *cluster, const char *category);
typedef int (*get_output_info_t)(const char *filename, int *w, int *h, double *priority, char **content, char **title);
typedef int (*initialize_t)(const char *pkgname);
typedef int (*finalize_t)(void);
typedef char *(*pinup_t)(const char *filename, int pinup);
typedef int (*is_pinned_up_t)(const char *filename);
typedef int (*system_event_t)(const char *filename, int type);
typedef int (*get_alt_info_t)(const char *filename, char **icon, char **name);
typedef int (*set_content_info_t)(const char *filename, bundle *b);

typedef int (*adaptor_create_t)(const char *pkgname, const char *filename, const char *content, const char *cluster, const char *category);
typedef int (*adaptor_destroy_t)(const char *pkgname, const char *filename);
typedef int (*adaptor_is_updated_t)(const char *pkgname, const char *filename);
typedef int (*adaptor_need_to_destroy_t)(const char *pkgname, const char *filename);
typedef int (*adaptor_update_content_t)(const char *pkgname, const char *filename);
typedef int (*adaptor_clicked_t)(const char *pkgname, const char *filename, const char *event, double timestamp, double x, double y);
typedef int (*adaptor_text_signal_t)(const char *pkgname, const char *filename, const char *signal_name, const char *source, widget_event_info_s event_info);
typedef int (*adaptor_resize_t)(const char *pkgname, const char *filename, int type);
typedef int (*adaptor_create_needed_t)(const char *pkgname, const char *cluster, const char *category);
typedef int (*adaptor_change_group_t)(const char *pkgname, const char *filename, const char *cluster, const char *category);
typedef int (*adaptor_get_output_info_t)(const char *pkgname, const char *filename, int *w, int *h, double *priority, char **content, char **title);
typedef int (*adaptor_initialize_t)(const char *pkgname);
typedef int (*adaptor_finalize_t)(const char *pkgname);
typedef char *(*adaptor_pinup_t)(const char *pkgname, const char *filename, int pinup);
typedef int (*adaptor_is_pinned_up_t)(const char *pkgname, const char *filename);
typedef int (*adaptor_system_event_t)(const char *pkgname, const char *filename, int type);
typedef int (*adaptor_get_alt_info_t)(const char *pkgname, const char *filename, char **icon, char **name);
typedef int (*adaptor_set_content_info_t)(const char *pkgname, const char *filename, bundle *b);

struct instance {
	struct so_item *item;
	char *id;
	char *content;
	char *title;
	char *icon;	// alternative icon
	char *name;	// alternative name
	int w;
	int h;
	double priority;
	char *cluster;
	char *category;
	int orientation;
};

struct so_item {
	char *so_fname;
	char *pkgname;
	void *handle;
	int timeout;
	int has_widget_script;

	Eina_List *inst_list;

	struct {
		initialize_t initialize;
		finalize_t finalize;
		create_t create;
		destroy_t destroy;
		is_updated_t is_updated;
		update_content_t update_content;
		clicked_t clicked;
		text_signal_t text_signal;
		resize_t resize;
		create_needed_t create_needed;
		change_group_t change_group;
		get_output_info_t get_output_info;
		need_to_destroy_t need_to_destroy;
		pinup_t pinup;
		is_pinned_up_t is_pinned_up;
		system_event_t sys_event;
		get_alt_info_t get_alt_info;
		set_content_info_t set_content_info;
	} widget;

	struct {
		adaptor_initialize_t initialize;
		adaptor_finalize_t finalize;
		adaptor_create_t create;
		adaptor_destroy_t destroy;
		adaptor_is_updated_t is_updated;
		adaptor_update_content_t update_content;
		adaptor_clicked_t clicked;
		adaptor_text_signal_t text_signal;
		adaptor_resize_t resize;
		adaptor_create_needed_t create_needed;
		adaptor_change_group_t change_group;
		adaptor_get_output_info_t get_output_info;
		adaptor_need_to_destroy_t need_to_destroy;
		adaptor_pinup_t pinup;
		adaptor_is_pinned_up_t is_pinned_up;
		adaptor_system_event_t sys_event;
		adaptor_get_alt_info_t get_alt_info;
		adaptor_set_content_info_t set_content_info;
	} adaptor;
};

enum current_operations {
	WIDGET_OP_UNKNOWN,
	WIDGET_OP_CREATE,
	WIDGET_OP_RESIZE,
	WIDGET_OP_CONTENT_EVENT,
	WIDGET_OP_NEED_TO_UPDATE,
	WIDGET_OP_NEED_TO_DESTROY,
	WIDGET_OP_NEED_TO_CREATE,
	WIDGET_OP_CHANGE_GROUP,
	WIDGET_OP_GET_INFO,
	WIDGET_OP_UPDATE_CONTENT,
	WIDGET_OP_CLICKED,
	WIDGET_OP_SYSTEM_EVENT,
	WIDGET_OP_PINUP,
	WIDGET_OP_IS_PINNED_UP,
	WIDGET_OP_DESTROY,
	WIDGET_OP_GET_ALT_INFO,
	WIDGET_OP_SET_CONTENT_INFO
};

extern struct instance *so_find_instance(const char *pkgname, const char *filename);
extern int so_create(const char *pkgname, const char *filename, const char *content_info, int timeout, int has_widget_script, const char *cluster, const char *category, const char *abi, struct instance **inst);
extern int so_is_updated(struct instance *inst);
extern int so_need_to_destroy(struct instance *inst);
extern int so_update(struct instance *inst);
extern int so_destroy(struct instance *inst, int unload);
extern int so_clicked(struct instance *inst, const char *event, double timestamp, double x, double y);
extern int so_script_event(struct instance *inst, const char *signal_name, const char *source, widget_event_info_s event_info);
extern int so_resize(struct instance *inst, int w, int h);
extern int so_create_needed(const char *pkgname, const char *cluster, const char *category, const char *abi);
extern int so_change_group(struct instance *inst, const char *cluster, const char *category);
extern int so_get_output_info(struct instance *inst, int *w, int *h, double *priority, char **content, char **title);
extern char *so_pinup(struct instance *inst, int pinup);
extern int so_is_pinned_up(struct instance *inst);
extern int so_sys_event(struct instance *inst, int event);
extern int so_get_alt_info(struct instance *inst, char **icon, char **name);
extern int so_set_content_info(struct instance *inst, const char *content_info);

extern enum current_operations so_current_op(void);

/* End of a file */
