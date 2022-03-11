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

#ifndef __WIDGET_VIEWER_INTERNAL_H
#define __WIDGET_VIEWER_INTERNAL_H

#include "widget_viewer.h"
#include "widget_buffer.h"

struct cb_info {
    widget_ret_cb cb;
    void *data;
};

extern void _widget_invoke_event_handler(struct widget *handler, widget_event_type_e event);
extern void _widget_invoke_fault_handler(widget_fault_type_e type, const char *pkgname, const char *filename, const char *function);

extern struct widget_common *_widget_find_common_handle(const char *pkgname, const char *filename);
extern struct widget *_widget_new_widget(const char *pkgname, const char *id, double timestamp, const char *cluster, const char *category);
extern struct widget_common *_widget_find_common_handle_by_timestamp(double timestamp);

extern int _widget_set_group(struct widget_common *common, const char *cluster, const char *category);
extern void _widget_set_size(struct widget_common *common, int w, int h);
extern void _widget_set_gbarsize(struct widget_common *common, int w, int h);
extern void _widget_set_default_gbarsize(struct widget_common *common, int w, int h);
extern int _widget_set_content(struct widget_common *common, const char *content);
extern int _widget_set_title(struct widget_common *handler, const char *title);
extern void _widget_set_auto_launch(struct widget_common *handler, const char *auto_launch);
extern void _widget_set_id(struct widget_common *handler, const char *id);
extern void _widget_set_size_list(struct widget_common *handler, int size_list);
extern void _widget_set_priority(struct widget_common *handler, double priority);
extern int _widget_set_widget_fb(struct widget_common *handler, const char *filename);
extern int _widget_set_gbar_fb(struct widget_common *handler, const char *filename);
extern struct fb_info *_widget_get_gbar_fb(struct widget_common *handler);
extern struct fb_info *_widget_get_widget_fb(struct widget_common *handler);
extern void _widget_set_user(struct widget_common *handler, int user);
extern void _widget_set_pinup(struct widget_common *handler, int pinup);
extern void _widget_set_text_widget(struct widget_common *handler);
extern void _widget_set_text_gbar(struct widget_common *handler);
extern int _widget_text_widget(struct widget_common *handler);
extern int _widget_text_gbar(struct widget_common *handler);
extern void _widget_set_period(struct widget_common *handler, double period);
extern void _widget_set_update_mode(struct widget_common *handler, int active_mode);
extern void _widget_set_filename(struct widget_common *handler, const char *filename);
extern void _widget_unlink_filename(struct widget_common *common);
extern void _widget_set_alt_icon(struct widget_common *handler, const char *icon);
extern void _widget_set_alt_name(struct widget_common *handle, const char *name);
extern int _widget_destroy_common_handle(struct widget_common *common);
extern struct widget_common *_widget_create_common_handle(struct widget *handle, const char *pkgname, const char *cluster, const char *category);
extern int _widget_sync_gbar_fb(struct widget_common *common);
extern int _widget_sync_widget_fb(struct widget_common *common);
extern int _widget_common_unref(struct widget_common *common, struct widget *handle);
extern int _widget_common_ref(struct widget_common *common, struct widget *handle);
extern struct widget_common *_widget_find_sharable_common_handle(const char *pkgname, const char *content, int w, int h, const char *cluster, const char *category);
extern struct widget *_widget_find_widget_in_show(struct widget_common *common);
extern struct widget *_widget_get_widget_nth(struct widget_common *common, int nth);
extern void *_widget_remove_event_handler(widget_event_handler_cb widget_cb);
extern int _widget_add_event_handler(widget_event_handler_cb widget_cb, void *data);
extern int _widget_add_fault_handler(widget_fault_handler_cb widget_cb, void *data);
extern void *_widget_remove_fault_handler(widget_fault_handler_cb widget_cb);
extern struct cb_info *_widget_create_cb_info(widget_ret_cb cb, void *data);
extern void _widget_destroy_cb_info(struct cb_info *info);

extern struct widget *_widget_ref(struct widget *handler);
extern struct widget *_widget_unref(struct widget *handler, int destroy_common);
extern int _widget_send_delete(struct widget *handler, int type, widget_ret_cb cb, void *data);
extern int _widget_delete_all(void);

typedef enum widget_state {
    WIDGET_STATE_CREATE = 0xBEEFbeef,
    WIDGET_STATE_DELETE = 0xDEADdead, /* Delete only for this client */
    WIDGET_STATE_DESTROYED = 0x00DEAD00
} widget_state_e;

struct widget_common {
    widget_state_e state;

    struct dlist *widget_list;
    int refcnt;

    char *cluster;
    char *category;

    char *pkgname;
    char *id;

    char *content;
    char *title;
    char *filename;

    double timestamp;

    struct alt_info {
        char *icon;
        char *name;
    } alt;

    widget_delete_type_e delete_type;

    int is_user;
    int is_gbar_created;
    int is_pinned_up;
    int is_active_update;
    widget_visible_state_e visible;

    struct {
        widget_widget_type_e type;
        struct fb_info *fb;

        int size_list;

        int width;
        int height;
        double priority;

        char *auto_launch;
        double period;
        int pinup_supported;
        bool mouse_event;

        /* For the filtering event */
        double x;
        double y;

        /* For the extra buffer */
        unsigned int *extra_buffer;
        int last_extra_buffer_idx;

        /* Lock */
        widget_lock_info_t lock;

        /* For damaged region */
        struct widget_damage_region last_damage;
    } widget;

    struct {
        widget_gbar_type_e type;
        struct fb_info *fb;

        int width;
        int height;

        int default_width;
        int default_height;

        /* For the filtering event */
        double x;
        double y;

        /* For the extra buffer */
        unsigned int *extra_buffer;
        int last_extra_buffer_idx;

        /* Lock */
        widget_lock_info_t lock;

        /* For damaged region */
        struct widget_damage_region last_damage;
    } gbar;

    int nr_of_sizes;

    struct requested_flag {
        unsigned int created:1;
        unsigned int deleted:1;
        unsigned int pinup:1;
        unsigned int group_changed:1;
        unsigned int period_changed:1;
        unsigned int size_changed:1;
        unsigned int gbar_created:1;
        unsigned int gbar_destroyed:1;
        unsigned int update_mode:1;
        unsigned int access_event:1;
        unsigned int key_event:1;

        /*!
         * \note
         * Reserved
         */
        unsigned int reserved:21;
    } request;
};

struct job_item {
    struct widget *handle;
    widget_ret_cb cb;
    int ret;
    void *data;
};

struct widget {
    widget_state_e state;

    int refcnt;
    int paused_updating;

    widget_visible_state_e visible;
    struct widget_common *common;

    void *data;

    struct callback_table {
        struct widget_script_operators widget_ops;
        struct widget_script_operators gbar_ops;

        struct created {
            widget_ret_cb cb;
            void *data;
        } created;

        struct deleted {
            widget_ret_cb cb;
            void *data;
        } deleted;

        struct pinup {
            widget_ret_cb cb;
            void *data;
        } pinup;

        struct group_changed {
            widget_ret_cb cb;
            void *data;
        } group_changed;

        struct period_changed {
            widget_ret_cb cb;
            void *data;
        } period_changed;

        struct size_changed {
            widget_ret_cb cb;
            void *data;
        } size_changed;

        struct gbar_created {
            widget_ret_cb cb;
            void *data;
        } gbar_created;

        struct gbar_destroyed {
            widget_ret_cb cb;
            void *data;
        } gbar_destroyed;

        struct update_mode {
            widget_ret_cb cb;
            void *data;
        } update_mode;

        struct access_event {
            widget_ret_cb cb;
            void *data;
        } access_event;

        struct key_event {
            widget_ret_cb cb;
            void *data;
        } key_event;
    } cbs;
};

#endif /* __WIDGET_VIEWER_INTERNAL_H */

/* End of a file */
