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

extern const char *widget_find_pkgname(const char *uri);
extern int widget_request_update_by_id(const char *uri);
extern int widget_tirgger_update_monitor(const char *id, int is_pd);
extern int widget_update_extra_info(const char *id, const char *content, const char *title, const char *icon, const char *name);
extern struct conf *widget_conf_get_conf(void);

/* End of a file */
