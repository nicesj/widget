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

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * Implement below functions
 */
extern int script_update_color(void *handle, const char *id, const char *part, const char *rgba);
extern int script_update_text(void *handle, const char *id, const char *part, const char *text);
extern int script_update_image(void *handle, const char *id, const char *part, const char *path, const char *option);
extern int script_update_access(void *handle, const char *id, const char *part, const char *text, const char *option);
extern int script_operate_access(void *handle, const char *id, const char *part, const char *operation, const char *option);
extern int script_update_script(void *handle, const char *src_id, const char *target_id, const char *part, const char *path, const char *group);
extern int script_update_signal(void *handle, const char *id, const char *part, const char *signal);
extern int script_update_drag(void *handle, const char *id, const char *part, double x, double y);
extern int script_update_size(void *handle, const char *id, int w, int h);
extern int script_update_category(void *handle, const char *id, const char *category);

extern void *script_create(void *buffer_handle, const char *file, const char *group);
extern int script_destroy(void *handle);

extern int script_load(void *handle, int (*render_pre)(void *buffer_handle, void *data), int (*render_post)(void *render_handle, void *data), void *data);
extern int script_unload(void *handle);

/*!
	WIDGET_ACCESS_HIGHLIGHT		0
	WIDGET_ACCESS_HIGHLIGHT_NEXT	1
	WIDGET_ACCESS_HIGHLIGHT_PREV	2
	WIDGET_ACCESS_ACTIVATE		3
	WIDGET_ACCESS_VALUE_CHANGE		4
	WIDGET_ACCESS_SCROLL		5
*/
extern int script_feed_event(void *handle, int event_type, int x, int y, int down, unsigned int keycode, double timestamp);

extern int script_init(double scale, int premultiplied);
extern int script_fini(void);

extern const char *script_magic_id(void);

#ifdef __cplusplus
}
#endif

/* End of a file */
