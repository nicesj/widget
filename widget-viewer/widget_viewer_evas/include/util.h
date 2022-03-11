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

#ifndef __WIDGET_VIEWER_UTIL_H
#define __WIDGET_VIEWER_UTIL_H

extern int util_screen_size_get(int *w, int *h);
extern unsigned int util_replace_native_surface(struct widget *handle, int gbar, Evas_Object *content, unsigned int pixmap);
extern int util_set_native_surface(struct widget *handle, int gbar, Evas_Object *content, unsigned int pixmap, int skip_acquire);
extern unsigned int util_get_resource_id_of_native_surface(Evas_Native_Surface *surface);
extern void *util_display_get(void);

#endif

/* End of a file */
