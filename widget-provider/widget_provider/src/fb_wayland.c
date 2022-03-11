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

#include <dlog.h>
#include <widget_errno.h>
#include <widget_service.h>
#include <widget_buffer.h>
#include <widget_util.h>

#include <wayland-client.h>
#include <tbm_bufmgr.h>

#include "debug.h"
#include "util.h"
#include "widget_provider.h"
#include "widget_provider_buffer.h"
#include "provider_buffer_internal.h"
#include "fb.h"
#include "dlist.h"

int errno;

struct gem_data {
	tbm_bo pixmap_bo;
	int count;
	int buf_count;
	int w;
	int h;
	int depth;
	void *data; /* Gem layer */
	int refcnt;
	int pixmap; /* FD in case of wayland */
};

static struct info {
	tbm_bufmgr bufmgr;
	int fd;
	struct wl_display *disp;
	int disp_is_opened;
	int master_disconnected;
	struct dlist *gem_list;
} s_info = {
	.bufmgr = NULL,
	.fd = -1,
	.disp = NULL,
	.disp_is_opened = 0,
	.master_disconnected = 0,
	.gem_list = NULL,
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
		return WIDGET_ERROR_NONE;
	}

	s_info.bufmgr = tbm_bufmgr_init(s_info.fd);
	if (!s_info.bufmgr) {
		ErrPrint("Failed to init bufmgr\n");
		widget_util_release_drm_fd(s_info.fd);
		s_info.fd = -1;
		return WIDGET_ERROR_NONE;
	}

	s_info.master_disconnected = 0;
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

	return WIDGET_ERROR_NONE;
}

static inline int sync_for_file(struct fb_info *info)
{
	int fd;
	widget_fb_t buffer;
	const char *path;

	buffer = info->buffer;
	if (!buffer) {
		DbgPrint("Buffer is NIL, skip sync\n");
		return WIDGET_ERROR_NONE;
	}

	if (buffer->state != WIDGET_FB_STATE_CREATED) {
		ErrPrint("Invalid state of a FB\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (buffer->type != WIDGET_FB_TYPE_FILE) {
		DbgPrint("Ingore sync\n");
		return WIDGET_ERROR_NONE;
	}

	path = widget_util_uri_to_path(info->id);
	if (!path) {
		ErrPrint("Invalid id: %s\n", info->id);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	fd = open(path, O_WRONLY | O_CREAT, 0644);
	if (fd < 0) {
		ErrPrint("open %s: %d\n", path, errno);
		return WIDGET_ERROR_IO_ERROR;
	}

	if (write(fd, buffer->data, info->bufsz) != info->bufsz) {
		ErrPrint("write: %d\n", errno);
		if (close(fd) < 0) {
			ErrPrint("close: %d\n", errno);
		}
		return WIDGET_ERROR_IO_ERROR;
	}

	if (close(fd) < 0) {
		ErrPrint("close: %d\n", errno);
	}
	return WIDGET_ERROR_NONE;
}

static inline int sync_for_pixmap(struct fb_info *info)
{
	widget_fb_t buffer;

	buffer = info->buffer;
	if (!buffer) {
		DbgPrint("Buffer is NIL, skip sync\n");
		return WIDGET_ERROR_NONE;
	}

	if (buffer->state != WIDGET_FB_STATE_CREATED) {
		ErrPrint("Invalid state of a FB\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (buffer->type != WIDGET_FB_TYPE_PIXMAP) {
		DbgPrint("Invalid buffer\n");
		return WIDGET_ERROR_NONE;
	}

	if (info->handle == 0) {
		ErrPrint("Pixmap ID is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (info->bufsz == 0) {
		DbgPrint("Nothing can be sync\n");
		return WIDGET_ERROR_NONE;
	}

	/**
	 * @note
	 * Do nothing for Wayland
	 */

	return WIDGET_ERROR_NONE;
}

int fb_sync(struct fb_info *info)
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
		return sync_for_file(info);
	} else if (!strncasecmp(info->id, SCHEMA_PIXMAP, strlen(SCHEMA_PIXMAP))) {
		return sync_for_pixmap(info);
	} else if (!strncasecmp(info->id, SCHEMA_SHM, strlen(SCHEMA_SHM))) {
		return WIDGET_ERROR_NONE;
	}

	ErrPrint("Invalid URI: [%s]\n", info->id);
	return WIDGET_ERROR_INVALID_PARAMETER;
}

struct fb_info *fb_create(const char *id, int w, int h)
{
	struct fb_info *info;

	if (!id || id[0] == '\0') {
		ErrPrint("Invalid id\n");
		return NULL;
	}

	info = calloc(1, sizeof(*info));
	if (!info) {
		ErrPrint("calloc: %d\n", errno);
		return NULL;
	}

	info->id = strdup(id);
	if (!info->id) {
		ErrPrint("strdup: %d\n", errno);
		free(info);
		return NULL;
	}

	info->pixels = sizeof(int);

	if (sscanf(info->id, SCHEMA_SHM "%d", &info->handle) == 1) {
		DbgPrint("SHMID: %d\n", info->handle);
	} else if (sscanf(info->id, SCHEMA_PIXMAP "%d:%d", &info->handle, &info->pixels) == 2) {
		DbgPrint("PIXMAP: %d\n", info->handle);
	} else if (!strncasecmp(info->id, SCHEMA_FILE, strlen(SCHEMA_FILE))) {
		info->handle = -1;
	} else {
		ErrPrint("Unsupported schema: %s\n", info->id);
		free(info->id);
		free(info);
		return NULL;
	}

	info->w = w;
	info->h = h;

	return info;
}

int fb_has_gem(struct fb_info *info)
{
	return !strncasecmp(info->id, SCHEMA_PIXMAP, strlen(SCHEMA_PIXMAP)) && info->gem;
}

int fb_support_gem(struct fb_info *info)
{
	return !strncasecmp(info->id, SCHEMA_PIXMAP, strlen(SCHEMA_PIXMAP)) && (s_info.fd >= 0);
}

static inline struct gem_data *create_gem(unsigned int pixmap, int w, int h, int depth, int auto_align)
{
	struct gem_data *gem;

	if (!s_info.bufmgr) {
		/*
		 * @note
		 * In rare case,
		 * if the mastser is disconnected, service provider should not try to create the gem buffer
		 * To avoid this, returns NULL from here.
		 */
		ErrPrint("Buffer manager is not initialized\n");
		return NULL;
	}

	gem = calloc(1, sizeof(*gem));
	if (!gem) {
		ErrPrint("calloc: %d\n", errno);
		return NULL;
	}

	gem->count = 1;
	gem->w = w;
	gem->h = h;
	gem->depth = depth;
	gem->pixmap = pixmap;

	gem->pixmap_bo = tbm_bo_import(s_info.bufmgr, gem->pixmap);
	if (!gem->pixmap_bo) {
		ErrPrint("Failed to import BO\n");
		free(gem);
		return NULL;
	}

	s_info.gem_list = dlist_append(s_info.gem_list, gem);
	return gem;
}

static inline int destroy_gem(struct gem_data *gem)
{
	dlist_remove_data(s_info.gem_list, gem);

	if (!s_info.master_disconnected && gem->pixmap_bo) {
		DbgPrint("unref pixmap bo\n");
		tbm_bo_unref(gem->pixmap_bo);
		gem->pixmap_bo = NULL;
	} else {
		ErrPrint("Resource is not trusted anymore(%u)\n", gem->pixmap);
	}

	free(gem);
	return WIDGET_ERROR_NONE;
}

static inline void *acquire_gem(struct gem_data *gem)
{
	if (!gem->data) {
		tbm_bo_handle handle;
		handle = tbm_bo_map(gem->pixmap_bo, TBM_DEVICE_CPU, TBM_OPTION_READ | TBM_OPTION_WRITE);
		gem->data = handle.ptr;
		if (!gem->data) {
			ErrPrint("Failed to get BO\n");
		}
	}

	gem->refcnt++;
	return gem->data;
}

static inline void release_gem(struct gem_data *gem)
{
	gem->refcnt--;
	if (gem->refcnt == 0) {
		tbm_bo_unmap(gem->pixmap_bo);
		gem->data = NULL;
	} else if (gem->refcnt < 0) {
		ErrPrint("Invalid refcnt: %d (reset)\n", gem->refcnt);
		gem->refcnt = 0;
	}
}

void *fb_dump_frame(unsigned int pixmap, int w, int h, int depth)
{
	struct gem_data *gem;
	void *canvas;
	void *buffer = NULL;

	gem = create_gem(pixmap, w, h, depth, 1);
	if (!gem) {
		ErrPrint("Failed to create a gem\n");
		return NULL;
	}

	canvas = acquire_gem(gem);
	if (canvas) {
		buffer = malloc(w * h * depth);
		if (buffer) {
			memcpy(buffer, canvas, w * h * depth);
		} else {
			ErrPrint("malloc: %d\n", errno);
		}
		release_gem(gem);
	}

	(void)destroy_gem(gem);
	return buffer;
}

int fb_create_gem(struct fb_info *info, int auto_align)
{
	if (info->gem) {
		DbgPrint("Already created\n");
		return WIDGET_ERROR_NONE;
	}

	info->gem = create_gem(info->handle, info->w, info->h, info->pixels, auto_align);
	return info->gem ? WIDGET_ERROR_NONE : WIDGET_ERROR_FAULT;
}

int fb_destroy_gem(struct fb_info *info)
{
	if (!info || !info->gem) {
		ErrPrint("GEM is not created\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	destroy_gem(info->gem);
	info->gem = NULL;
	return WIDGET_ERROR_NONE;
}

void *fb_acquire_gem(struct fb_info *info)
{
	if (!info->gem) {
		ErrPrint("Invalid FB info\n");
		return NULL;
	}

	return acquire_gem(info->gem);
}

int fb_release_gem(struct fb_info *info)
{
	if (!info->gem) {
		ErrPrint("Invalid FB info\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	release_gem(info->gem);
	return WIDGET_ERROR_NONE;
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
		path = widget_util_uri_to_path(info->id);
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
	void *addr;

	if (!info) {
		return NULL;
	}

	if (!info->buffer) {
		if (!strncasecmp(info->id, SCHEMA_PIXMAP, strlen(SCHEMA_PIXMAP))) {
			ErrPrint("S/W Backed is not supported\n");
			return NULL;
		} else if (!strncasecmp(info->id, SCHEMA_FILE, strlen(SCHEMA_FILE))) {
			info->bufsz = info->w * info->h * info->pixels;
			buffer = calloc(1, sizeof(*buffer) + info->bufsz);
			if (!buffer) {
				ErrPrint("calloc: %d\n", errno);
				info->bufsz = 0;
				return NULL;
			}

			buffer->type = WIDGET_FB_TYPE_FILE;
			buffer->refcnt = 0;
			buffer->state = WIDGET_FB_STATE_CREATED;
			buffer->info = info;
			info->buffer = buffer;
		} else if (!strncasecmp(info->id, SCHEMA_SHM, strlen(SCHEMA_SHM))) {
			buffer = shmat(info->handle, NULL, 0);
			if (buffer == (void *)-1) {
				ErrPrint("shmat: %d\n", errno);
				return NULL;
			}

			info->buffer = buffer;
		} else {
			DbgPrint("Buffer is NIL\n");
			return NULL;
		}
	}

	buffer = info->buffer;
	switch (buffer->type) {
	case WIDGET_FB_TYPE_FILE:
		buffer->refcnt++;
		/* Fall through */
	case WIDGET_FB_TYPE_SHM:
		addr = buffer->data;
		break;
	case WIDGET_FB_TYPE_PIXMAP:
		ErrPrint("S/W Backend is not supported\n");
	default:
		addr = NULL;
		break;
	}

	return addr;
}

int fb_release_buffer(void *data)
{
	widget_fb_t buffer;

	if (!data) {
		return WIDGET_ERROR_NONE;
	}

	buffer = container_of(data, struct widget_fb, data);

	if (buffer->state != WIDGET_FB_STATE_CREATED) {
		ErrPrint("Invalid handle\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	switch (buffer->type) {
	case WIDGET_FB_TYPE_SHM:
		/*!
		 * \note
		 * SHM can not use the "info"
		 * it is NULL
		 */
		if (shmdt(buffer) < 0) {
			ErrPrint("shmdt: %d\n", errno);
		}
		break;
	case WIDGET_FB_TYPE_PIXMAP:
		ErrPrint("S/W Backend is not supported\n");
		return WIDGET_ERROR_NOT_SUPPORTED;
	case WIDGET_FB_TYPE_FILE:
		buffer->refcnt--;
		if (buffer->refcnt == 0) {
			struct fb_info *info;
			info = buffer->info;
			if (info && info->buffer == buffer) {
				info->buffer = NULL;
			}
			buffer->state = WIDGET_FB_STATE_DESTROYED;
			free(buffer);
		}
		break;
	default:
		ErrPrint("Unknown type\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return WIDGET_ERROR_NONE;
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

int fb_refcnt(void *data)
{
	widget_fb_t buffer;
	struct shmid_ds buf;
	int ret;

	if (!data) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	buffer = container_of(data, struct widget_fb, data);

	if (buffer->state != WIDGET_FB_STATE_CREATED) {
		ErrPrint("Invalid handle\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	switch (buffer->type) {
	case WIDGET_FB_TYPE_SHM:
		if (shmctl(buffer->refcnt, IPC_STAT, &buf) < 0) {
			ErrPrint("shmctl: %d\n", errno);
			ret = WIDGET_ERROR_FAULT;
			break;
		}

		ret = buf.shm_nattch; /*!< This means attached count of process */
		break;
	case WIDGET_FB_TYPE_PIXMAP:
		ret = buffer->refcnt;
		break;
	case WIDGET_FB_TYPE_FILE:
		ret = buffer->refcnt;
		break;
	default:
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
		ErrPrint("Handle is not valid\n");
		return 0;
	}

	info->bufsz = info->w * info->h * info->pixels;
	return info->bufsz;
}

int fb_sync_xdamage(struct fb_info *info, widget_damage_region_s *region)
{
	return WIDGET_ERROR_NOT_SUPPORTED;
}

int fb_stride(struct fb_info *info)
{
	int stride;

	stride = info->w * info->pixels;
	DbgPrint("Stride: %d\n", stride);

	return stride;
}

void fb_master_disconnected(void)
{
	s_info.master_disconnected = 1;
}

/* End of a file */
