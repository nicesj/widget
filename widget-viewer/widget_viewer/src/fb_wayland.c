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

#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/shm.h>
#include <sys/ipc.h>

#include <wayland-client.h>
#include <tbm_bufmgr.h>

#include <dlog.h>
#include <widget_errno.h> /* For error code */
#include <widget_service.h> /* For buffer event data */
#include <widget_buffer.h>
#include <widget_util.h>

#include "debug.h"
#include "util.h"
#include "fb.h"
#include "dlist.h"

int errno;

struct gem_data {
	tbm_bo pixmap_bo;
	tbm_bo_handle handle;
	widget_fb_t buffer;
};

static struct {
	struct wl_display *disp;
	tbm_bufmgr bufmgr;
	int disp_is_opened;
	int fd;
	struct dlist *canvas_list;
} s_info = {
	.disp = NULL,
	.bufmgr = NULL,
	.disp_is_opened = 0,
	.fd = -1,
	.canvas_list = NULL,
};

int fb_init(void *disp)
{
	int ret;

	s_info.disp = disp;
	if (!s_info.disp) {
		s_info.disp = wl_display_connect(NULL);
		if (!s_info.disp) {
			ErrPrint("Failed to open a display\n");
			return WIDGET_ERROR_FAULT;
		}

		s_info.disp_is_opened = 1;
	}

	ret = widget_util_get_drm_fd(s_info.disp, &s_info.fd);
	if (ret != WIDGET_ERROR_NONE || s_info.fd < 0) {
		ErrPrint("Failed to open a drm device: (%d)\n", errno);
		return WIDGET_ERROR_FAULT;
	}

	s_info.bufmgr = tbm_bufmgr_init(s_info.fd);
	if (!s_info.bufmgr) {
		ErrPrint("Failed to init bufmgr\n");
		widget_util_release_drm_fd(s_info.fd);
		s_info.fd = -1;
		return WIDGET_ERROR_FAULT;
	}

	return WIDGET_ERROR_NONE;
}

int fb_fini(void)
{
	if (s_info.bufmgr) {
		tbm_bufmgr_deinit(s_info.bufmgr);
		s_info.bufmgr = NULL;
	}

	if (s_info.fd >= 0) {
		widget_util_release_drm_fd(s_info.fd);
		s_info.fd = -1;
	}

	if (s_info.disp_is_opened && s_info.disp) {
		wl_display_disconnect(s_info.disp);
		s_info.disp = NULL;
	}

	s_info.disp = NULL;
	return WIDGET_ERROR_NONE;
}

static inline void update_fb_size(struct fb_info *info)
{
	info->bufsz = info->w * info->h * info->pixels;
}

static int sync_for_file(struct fb_info *info, int x, int y, int w, int h)
{
	int fd;
	widget_fb_t buffer;
	const char *path = NULL;

	buffer = info->buffer;

	if (!buffer) { /* Ignore this sync request */
		return WIDGET_ERROR_NONE;
	}

	if (buffer->state != WIDGET_FB_STATE_CREATED) {
		ErrPrint("Invalid state of a FB\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (buffer->type != WIDGET_FB_TYPE_FILE) {
		ErrPrint("Invalid buffer\n");
		return WIDGET_ERROR_NONE;
	}

	path = util_uri_to_path(info->id);

	if (path == NULL) {
		ErrPrint("Invalid parameter\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		ErrPrint("Failed to open a file (%s) because of (%d)\n",
				util_uri_to_path(info->id), errno);

		/**
		 * @note
		 * But return ZERO, even if we couldn't get a buffer file,
		 * the viewer can draw empty screen.
		 *
		 * and then update it after it gots update events
		 */
		return WIDGET_ERROR_NONE;
	}

	/**
	 * @note
	 * Could we get some advantage if we load a part of file instead of loading all of them?
	 */
	if (x != 0 || y != 0 || info->w != w || info->h != h) {
		int iy;
		register int index;
		register int width;

		for (iy = y; iy < h; iy++) {
			index = iy * info->w + x;
			width = w * info->pixels;

			if (lseek(fd, index * info->pixels, SEEK_SET) != index * info->pixels) {
				ErrPrint("lseek: %d\n", errno);
				if (close(fd) < 0) {
					ErrPrint("close: %d\n", errno);
				}
				/**
				 * @note
				 * But return ZERO, even if we couldn't get a buffer file,
				 * the viewer can draw empty screen.
				 *
				 * and then update it after it gots update events
				 */
				return WIDGET_ERROR_NONE;
			}

			if (read(fd, ((unsigned int *)buffer->data) + index, width) != width) {
				if (close(fd) < 0) {
					ErrPrint("close: %d\n", errno);
				}
				/**
				 * @note
				 * But return ZERO, even if we couldn't get a buffer file,
				 * the viewer can draw empty screen.
				 *
				 * and then update it after it gots update events
				 */
				return WIDGET_ERROR_NONE;
			}
		}
	} else {
		if (read(fd, buffer->data, info->bufsz) != info->bufsz) {
			ErrPrint("read: %d\n", errno);
			if (close(fd) < 0) {
				ErrPrint("close: %d\n", errno);
			}

			/**
			 * @note
			 * But return ZERO, even if we couldn't get a buffer file,
			 * the viewer can draw empty screen.
			 *
			 * and then update it after it gots update events
			 */
			return WIDGET_ERROR_NONE;
		}
	}

	if (close(fd) < 0) {
		ErrPrint("close: %d\n", errno);
	}
	return WIDGET_ERROR_NONE;
}

int fb_sync(struct fb_info *info, int x, int y, int w, int h)
{
	if (!info) {
		ErrPrint("FB Handle is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!info->id || info->id[0] == '\0') {
		DbgPrint("Ingore sync\n");
		return WIDGET_ERROR_NONE;
	}

	if (!strncasecmp(info->id, SCHEMA_FILE, strlen(SCHEMA_FILE))) {
		return sync_for_file(info, x, y, w, h);
	} else if (!strncasecmp(info->id, SCHEMA_PIXMAP, strlen(SCHEMA_PIXMAP))) {
		return WIDGET_ERROR_NONE;
	} else if (!strncasecmp(info->id, SCHEMA_SHM, strlen(SCHEMA_SHM))) {
		/* No need to do sync */
		return WIDGET_ERROR_NONE;
	}

	return WIDGET_ERROR_INVALID_PARAMETER;
}

struct fb_info *fb_create(const char *id, int w, int h)
{
	struct fb_info *info;

	if (!id || id[0] == '\0') {
		ErrPrint("Invalid ID\n");
		return NULL;
	}

	info = calloc(1, sizeof(*info));
	if (!info) {
		ErrPrint("Heap: %d\n", errno);
		return NULL;
	}

	info->id = strdup(id);
	if (!info->id) {
		ErrPrint("Heap: %d\n", errno);
		free(info);
		return NULL;
	}

	info->pixels = sizeof(int);    /* Use the default pixels(depth) */

	if (sscanf(info->id, SCHEMA_SHM "%d", &info->handle) == 1) {
		DbgPrint("SHMID: %d is gotten\n", info->handle);
	} else if (sscanf(info->id, SCHEMA_PIXMAP "%d:%d", &info->handle, &info->pixels) == 2) {
		DbgPrint("PIXMAP-SHMID: %d is gotten (%d)\n", info->handle, info->pixels);
	} else {
		info->handle = WIDGET_ERROR_INVALID_PARAMETER;
	}

	info->bufsz = 0;
	info->buffer = NULL;
	info->w = w;
	info->h = h;

	return info;
}

int fb_destroy(struct fb_info *info)
{
	if (!info) {
		ErrPrint("Handle is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (info->buffer) {
		widget_fb_t buffer;
		buffer = info->buffer;

		buffer->info = NULL;
	}

	free(info->id);
	free(info);
	return WIDGET_ERROR_NONE;
}

int fb_is_created(struct fb_info *info)
{
	if (!info) {
		ErrPrint("Handle is not valid\n");
		return 0;
	}

	if (!strncasecmp(info->id, SCHEMA_PIXMAP, strlen(SCHEMA_PIXMAP)) && info->handle != 0) {
		return 1;
	} else if (!strncasecmp(info->id, SCHEMA_SHM, strlen(SCHEMA_SHM)) && info->handle > 0) {
		return 1;
	} else {
		const char *path;
		path = util_uri_to_path(info->id);
		if (path && access(path, F_OK | R_OK) == 0) {
			return 1;
		} else {
			ErrPrint("access: %d (%s)\n", errno, path);
		}
	}

	return 0;
}

void *fb_acquire_buffer(struct fb_info *info)
{
	widget_fb_t buffer;

	if (!info) {
		ErrPrint("info == NIL\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return NULL;
	}

	if (!info->buffer) {
		if (!strncasecmp(info->id, SCHEMA_PIXMAP, strlen(SCHEMA_PIXMAP))) {
			struct gem_data *gem;

			update_fb_size(info);

			buffer = calloc(1, sizeof(*buffer) + info->bufsz);
			if (!buffer) {
				ErrPrint("Heap: %d\n", errno);
				set_last_result(WIDGET_ERROR_OUT_OF_MEMORY);
				info->bufsz = 0;
				return NULL;
			}

			gem = calloc(1, sizeof(*gem));
			if (!gem) {
				ErrPrint("Heap: %d\n", errno);
				return NULL;
			}

			buffer->type = WIDGET_FB_TYPE_PIXMAP;
			buffer->refcnt = 0;
			buffer->state = WIDGET_FB_STATE_CREATED;
			buffer->info = info;
			info->buffer = buffer;
			info->gem = gem;

			gem->pixmap_bo = tbm_bo_import(s_info.bufmgr, info->handle);
			if (!gem->pixmap_bo) {
				ErrPrint("Failed to get bo\n");
				free(gem);
				free(buffer);
				return NULL;
			}

			gem->handle = tbm_bo_map(gem->pixmap_bo, TBM_DEVICE_CPU, TBM_OPTION_READ);
			gem->buffer = buffer;

			s_info.canvas_list = dlist_append(s_info.canvas_list, buffer);
		} else if (!strncasecmp(info->id, SCHEMA_FILE, strlen(SCHEMA_FILE))) {
			update_fb_size(info);

			buffer = calloc(1, sizeof(*buffer) + info->bufsz);
			if (!buffer) {
				ErrPrint("Heap: %d\n", errno);
				info->bufsz = 0;
				set_last_result(WIDGET_ERROR_OUT_OF_MEMORY);
				return NULL;
			}

			buffer->type = WIDGET_FB_TYPE_FILE;
			buffer->refcnt = 0;
			buffer->state = WIDGET_FB_STATE_CREATED;
			buffer->info = info;
			info->buffer = buffer;

			sync_for_file(info, 0, 0, info->w, info->h);
		} else if (!strncasecmp(info->id, SCHEMA_SHM, strlen(SCHEMA_SHM))) {
			buffer = shmat(info->handle, NULL, 0);
			if (buffer == (void *)-1) {
				ErrPrint("shmat: %d (%d)\n", errno, info->handle);
				set_last_result(WIDGET_ERROR_FAULT);
				return NULL;
			}

			return buffer->data;
		} else {
			ErrPrint("Buffer is not created (%s)\n", info->id);
			set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
			return NULL;
		}
	}

	buffer = info->buffer;

	switch (buffer->type) {
	case WIDGET_FB_TYPE_PIXMAP:
		{
			struct gem_data *gem;

			buffer->refcnt++;
			gem = info->gem;

			return gem->handle.ptr;
		}
		break;
	case WIDGET_FB_TYPE_FILE:
		buffer->refcnt++;
		break;
	default:
		DbgPrint("Unknwon FP: %d\n", buffer->type);
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		break;
	}

	return buffer->data;
}

int fb_release_buffer(void *data)
{
	widget_fb_t buffer;
	struct dlist *l;

	if (!data) {
		ErrPrint("buffer data == NIL\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	buffer = NULL;
	dlist_foreach(s_info.canvas_list, l, buffer) {
		if (buffer) {
			struct fb_info *info;
			struct gem_data *gem;

			info = buffer->info;
			gem = info->gem;

			if (gem->handle.ptr == data) {
				break;
			}
		}
		buffer = NULL;
	}

	if (!buffer) {
		buffer = container_of(data, struct widget_fb, data);
	}

	if (buffer->state != WIDGET_FB_STATE_CREATED) {
		ErrPrint("Invalid handle\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	switch (buffer->type) {
	case WIDGET_FB_TYPE_SHM:
		if (shmdt(buffer) < 0) {
			ErrPrint("shmdt: %d\n", errno);
		}
		break;
	case WIDGET_FB_TYPE_PIXMAP:
		buffer->refcnt--;
		if (buffer->refcnt == 0) {
			struct fb_info *info;
			struct gem_data *gem;

			info = buffer->info;
			gem = info->gem;

			if (gem->pixmap_bo) {
				tbm_bo_unmap(gem->pixmap_bo);
				gem->pixmap_bo = NULL;
			}

			dlist_remove_data(s_info.canvas_list, buffer);
			free(gem);
			free(buffer);
		}
		break;
	case WIDGET_FB_TYPE_FILE:
		buffer->refcnt--;
		if (buffer->refcnt == 0) {
			struct fb_info *info;
			info = buffer->info;

			buffer->state = WIDGET_FB_STATE_DESTROYED;

			if (info && info->buffer == buffer) {
				info->buffer = NULL;
			}

			free(buffer);
		}
		break;
	default:
		ErrPrint("Unknwon buffer type\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		break;
	}

	return WIDGET_ERROR_NONE;
}

int fb_refcnt(void *data)
{
	widget_fb_t buffer;
	struct shmid_ds buf;
	int ret;

	if (!data) {
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	buffer = container_of(data, struct widget_fb, data);

	if (buffer->state != WIDGET_FB_STATE_CREATED) {
		ErrPrint("Invalid handle\n");
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	switch (buffer->type) {
	case WIDGET_FB_TYPE_SHM:
		if (shmctl(buffer->refcnt, IPC_STAT, &buf) < 0) {
			ErrPrint("Error: %d\n", errno);
			set_last_result(WIDGET_ERROR_FAULT);
			return WIDGET_ERROR_FAULT;
		}

		ret = buf.shm_nattch;
		break;
	case WIDGET_FB_TYPE_PIXMAP:
		ret = buffer->refcnt;
		break;
	case WIDGET_FB_TYPE_FILE:
		ret = buffer->refcnt;
		break;
	default:
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		break;
	}

	return ret;
}

const char *fb_id(struct fb_info *info)
{
	return info ? info->id : NULL;
}

int fb_get_size(struct fb_info *info, int *w, int *h)
{
	if (!info) {
		ErrPrint("Handle is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	*w = info->w;
	*h = info->h;
	return WIDGET_ERROR_NONE;
}

int fb_size(struct fb_info *info)
{
	if (!info) {
		set_last_result(WIDGET_ERROR_INVALID_PARAMETER);
		return 0;
	}

	update_fb_size(info);

	return info->bufsz;
}

int fb_type(struct fb_info *info)
{
	widget_fb_t buffer;

	if (!info) {
		return WIDGET_FB_TYPE_ERROR;
	}

	buffer = info->buffer;
	if (!buffer) {
		int type = WIDGET_FB_TYPE_ERROR;
		/*!
		 * \note
		 * Try to get this from SCHEMA
		 */
		if (info->id) {
			if (!strncasecmp(info->id, SCHEMA_FILE, strlen(SCHEMA_FILE))) {
				type = WIDGET_FB_TYPE_FILE;
			} else if (!strncasecmp(info->id, SCHEMA_PIXMAP, strlen(SCHEMA_PIXMAP))) {
				type = WIDGET_FB_TYPE_PIXMAP;
			} else if (!strncasecmp(info->id, SCHEMA_SHM, strlen(SCHEMA_SHM))) {
				type = WIDGET_FB_TYPE_SHM;
			}
		}

		return type;
	}

	return buffer->type;
}

/* End of a file */
