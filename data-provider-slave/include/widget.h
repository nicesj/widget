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

extern void widget_viewer_init(void);
extern void widget_viewer_fini(void);

struct widget_create_arg {
	double period;
	int timeout;
	int has_widget_script;
	int skip_need_to_create;
	const char *content;
	const char *cluster;
	const char *category;
	const char *abi;
	const char *direct_addr;
};

extern int widget_create(const char *pkgname, const char *id, struct widget_create_arg *arg, int *w, int *h, double *priority, char **content, char **title);
extern int widget_destroy(const char *pkgname, const char *id, int type);

extern int widget_viewer_resize_widget(const char *pkgname, const char *id, int w, int h);
extern int widget_clicked(const char *pkgname, const char *id, const char *event, double timestamp, double x, double y);
extern int widget_set_content_info(const char *pkgname, const char *id, const char *content_info);
extern int widget_set_content_info_all(const char *pkgname, const char *content);

extern int widget_script_event(const char *pkgname, const char *id, const char *signal_name, const char *source, widget_event_info_s event_info);
extern int widget_change_group(const char *pkgname, const char *id, const char *cluster, const char *category);

extern int widget_update(const char *pkgname, const char *id, int force);
extern int widget_update_all(const char *pkgname, const char *cluster, const char *category, int force);
extern void widget_pause_all(void);
extern void widget_resume_all(void);
extern int widget_viewer_set_period(const char *pkgname, const char *id, double period);
extern char *widget_pinup(const char *pkgname, const char *id, int pinup);
extern int widget_system_event(const char *pkgname, const char *id, int event);
extern int widget_system_event_all(int event);

extern int widget_open_gbar(const char *pkgname, const char *id);
extern int widget_close_gbar(const char *pkgname, const char *id);

extern int widget_pause(const char *pkgname, const char *id);
extern int widget_resume(const char *pkgname, const char *id);

extern int widget_viewer_is_pinned_up(const char *pkgname, const char *id);

extern void widget_turn_secured_on(void);
extern int widget_is_secured(void);

extern int widget_is_all_paused(void);
extern int widget_delete_all_deleteme(void);
extern int widget_delete_all(void);

extern int widget_viewer_connected(const char *pkgname, const char *id, const char *direct_addr);
extern int widget_viewer_disconnected(const char *pkgname, const char *id, const char *direct_addr);

extern int widget_set_orientation(const char *pkgname, const char *id, int orientation);

/**
 * @brief
 * Exported API for each widgets
 */
extern const char *widget_find_pkgname(const char *filename);
extern int widget_request_update_by_id(const char *filename);
extern int widget_trigger_update_monitor(const char *id, int is_gbar);
extern int widget_update_extra_info(const char *id, const char *content, const char *title, const char *icon, const char *name);
extern int widget_send_updated(const char *pkgname, const char *id, int idx, int x, int y, int w, int h, int gbar, const char *descfile);
extern int widget_send_buffer_updated(const char *pkgname, const char *id, widget_buffer_h handle, int idx, int x, int y, int w, int h, int gbar, const char *descfile);
extern int widget_orientation(const char *filename);

/* End of a file */
