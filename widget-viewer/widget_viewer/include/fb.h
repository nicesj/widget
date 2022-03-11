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

struct fb_info;

extern int fb_init(void *disp);
extern int fb_fini(void);
extern const char *fb_id(struct fb_info *info);
extern int fb_get_size(struct fb_info *info, int *w, int *h);
extern int fb_sync(struct fb_info *info, int x, int y, int w, int h);
extern int fb_size(struct fb_info *info);
extern int fb_refcnt(void *data);
extern int fb_is_created(struct fb_info *info);
extern int fb_type(struct fb_info *info);

extern struct fb_info *fb_create(const char *filename, int w, int h);
extern int fb_destroy(struct fb_info *info);

extern void *fb_acquire_buffer(struct fb_info *info);
extern int fb_release_buffer(void *data);

/* End of a file */
