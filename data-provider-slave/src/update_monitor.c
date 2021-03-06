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
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <Ecore.h>
#include <Ecore_File.h>
#include <Eina.h>

#include <dlog.h>
#include <widget_errno.h>
#include <widget_conf.h>

#include "critical_log.h"
#include "update_monitor.h"
#include "util.h"
#include "debug.h"
#include "conf.h"

int errno;

struct cb_item {
	char *filename;
	int (*cb)(const char *filename, void *data, int over);
	void *data;
	int deleted;
};

static struct info {
	int ifd;
	int iwd;
	Ecore_Fd_Handler *handler;
	Eina_List *update_list;
	Eina_List *delete_list;
	unsigned int update_list_in_use;
	unsigned int delete_list_in_use;
} s_info = {
	.ifd = -EINVAL,
	.iwd = -EINVAL,
	.handler = NULL,
	.update_list = NULL,
	.delete_list = NULL,
	.update_list_in_use = 0,
	.delete_list_in_use = 0,
};

static void *update_item_destroy(struct cb_item *item, Eina_List *l)
{
	void *data;

	if (s_info.update_list_in_use) {
		item->deleted = 1;
		data = item->data;
	} else {
		s_info.update_list = eina_list_remove_list(s_info.update_list, l);
		data = item->data;
		free(item->filename);
		free(item);
	}

	return data;
}

static void *delete_item_destroy(struct cb_item *item, Eina_List *l)
{
	void *data;

	if (s_info.delete_list_in_use == 1) {
		item->deleted = 1;
		data = item->data;
	} else {
		s_info.delete_list = eina_list_remove_list(s_info.delete_list, l);
		data = item->data;
		free(item->filename);
		free(item);
	}

	return data;
}

static Eina_Bool monitor_cb(void *data, Ecore_Fd_Handler *handler)
{
	int fd;
	int read_size;
	char *buffer;
	register int i;
	struct inotify_event *evt;
	char *filename;
	int len;
	int ret;

	fd = ecore_main_fd_handler_fd_get(handler);
	if (fd < 0) {
		ErrPrint("Failed to get file handler\n");
		return ECORE_CALLBACK_CANCEL;
	}

	if (ioctl(fd, FIONREAD, &read_size) < 0) {
		ErrPrint("Failed to get q size (%d)\n", errno);
		return ECORE_CALLBACK_CANCEL;
	}

	if (read_size <= 0) {
		ErrPrint("Buffer is not ready!!!\n");
		return ECORE_CALLBACK_RENEW;
	}

	buffer = calloc(read_size+1, sizeof(char));
	if (!buffer) {
		ErrPrint("calloc: %d\n", errno);
		return ECORE_CALLBACK_CANCEL;
	}

	if (read(fd, buffer, read_size) != read_size) {
		ErrPrint("Could not get entire events (%d)\n", errno);
		free(buffer);
		return ECORE_CALLBACK_CANCEL;
	}

	i = 0;
	while (i < read_size) {
		evt = (struct inotify_event *)(buffer + i);
		i += sizeof(*evt) + evt->len;

		if (util_check_ext(evt->name, "gnp.") == 0
				&& util_check_ext(evt->name, "csed.") == 0)
		{
			continue;
		}

		len = strlen(evt->name) + strlen(WIDGET_CONF_IMAGE_PATH) + 1;
		filename = malloc(len);
		if (!filename) {
			ErrPrint("malloc: %d\n", errno);
			/* We met error, but keep going,
			 * and care the remained buffer.
			 */
			continue;
		}

		ret = snprintf(filename, len, "%s%s", WIDGET_CONF_IMAGE_PATH, evt->name);
		if (ret < 0) {
			ErrPrint("snprintf: %d\n", errno);
			/* We met error, but keep goging.
			 * and care the remained buffer.
			 */
			free(filename);
			continue;
		}

		if (evt->mask & (IN_DELETE | IN_MOVED_FROM)) {
			update_monitor_trigger_delete_cb(filename, !!(evt->mask & IN_Q_OVERFLOW));
		} else if (evt->mask & (IN_CLOSE_WRITE | IN_MOVED_TO)) {
			update_monitor_trigger_update_cb(filename, !!(evt->mask & IN_Q_OVERFLOW));
		}

		free(filename);
	}

	free(buffer);
	return ECORE_CALLBACK_RENEW;
}

HAPI int update_monitor_init(void)
{
	DbgPrint("Shared folder: %s\n", WIDGET_CONF_IMAGE_PATH);

	s_info.ifd = inotify_init();
	if (s_info.ifd < 0) {
		ErrPrint("inotify_init: %d\n", errno);
		return WIDGET_ERROR_IO_ERROR;
	}

	if (access(WIDGET_CONF_IMAGE_PATH, R_OK | X_OK) != 0) {
		ErrPrint("Image folder is not exists\n");
		return WIDGET_ERROR_IO_ERROR;
	}

	s_info.iwd = inotify_add_watch(s_info.ifd, WIDGET_CONF_IMAGE_PATH,
			IN_DELETE | IN_CLOSE_WRITE | IN_MOVED_TO | IN_MOVED_FROM);

	if (s_info.iwd < 0) {
		ErrPrint("inotify_add_watch: %d\n", errno);
		if (close(s_info.ifd) < 0) {
			ErrPrint("close: %d\n", errno);
		}
		s_info.ifd = WIDGET_ERROR_INVALID_PARAMETER;
		return WIDGET_ERROR_IO_ERROR;
	}

	s_info.handler = ecore_main_fd_handler_add(s_info.ifd,
			ECORE_FD_READ, monitor_cb, NULL, NULL, NULL);
	if (!s_info.handler) {
		ErrPrint("Failed to add a FD handler\n");
		if (inotify_rm_watch(s_info.ifd, s_info.iwd) < 0) {
			ErrPrint("inotify_rm_watch: %d", errno);
		}
		s_info.iwd = -EINVAL;

		if (close(s_info.ifd) < 0) {
			ErrPrint("close: %d\n", errno);
		}
		s_info.ifd = WIDGET_ERROR_INVALID_PARAMETER;
		return WIDGET_ERROR_FAULT;
	}

	DbgPrint("Update monitor is successfully initialized\n");
	return WIDGET_ERROR_NONE;
}

HAPI int update_monitor_fini(void)
{
	if (!s_info.handler) {
		ErrPrint("Invalid fd handler\n");
	} else {
		ecore_main_fd_handler_del(s_info.handler);
		s_info.handler = NULL;
	}

	if (s_info.ifd >= 0) {
		if (inotify_rm_watch(s_info.ifd, s_info.iwd) < 0) {
			ErrPrint("inotify_rm_watch:%d", errno);
		}

		s_info.iwd = WIDGET_ERROR_INVALID_PARAMETER;

		if (close(s_info.ifd) < 0) {
			ErrPrint("close: %d\n", errno);
		}
		s_info.ifd = WIDGET_ERROR_INVALID_PARAMETER;
	}

	return WIDGET_ERROR_NONE;
}

HAPI int update_monitor_trigger_update_cb(const char *filename, int over)
{
	Eina_List *l;
	Eina_List *n;
	struct cb_item *item;
	int cnt = 0;

	s_info.update_list_in_use = 1;
	EINA_LIST_FOREACH_SAFE(s_info.update_list, l, n, item) {
		if (!strcmp(filename, item->filename)) {
			if (item->deleted || item->cb(filename, item->data, over) == EXIT_FAILURE || item->deleted) {
				/* Item can be deleted from the callback, so need to check existence */
				s_info.update_list = eina_list_remove_list(s_info.update_list, l);
				free(item->filename);
				free(item);
			}
			cnt++;
		}
	}
	s_info.update_list_in_use = 0;

	return cnt == 0 ? WIDGET_ERROR_INVALID_PARAMETER : WIDGET_ERROR_NONE;
}

HAPI int update_monitor_trigger_delete_cb(const char *filename, int over)
{
	Eina_List *l;
	Eina_List *n;
	struct cb_item *item;
	int cnt = 0;

	s_info.delete_list_in_use = 1;
	EINA_LIST_FOREACH_SAFE(s_info.delete_list, l, n, item) {
		if (!strcmp(filename, item->filename)) {
			/* delete should be checked before call the callback & after call the callback */
			if (item->deleted || item->cb(filename, item->data, over) == EXIT_FAILURE || item->deleted) {
				s_info.delete_list = eina_list_remove_list(s_info.delete_list, l);
				free(item->filename);
				free(item);
			}

			cnt++;
		}
	}
	s_info.delete_list_in_use = 0;

	return cnt == 0 ? WIDGET_ERROR_INVALID_PARAMETER : WIDGET_ERROR_NONE;
}

HAPI int update_monitor_add_update_cb(const char *filename,
		int (*cb)(const char *filename, void *data, int over), void *data)
{
	struct cb_item *item;

	item = calloc(1, sizeof(*item));
	if (!item) {
		ErrPrint("calloc:%d\n", errno);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	item->filename = strdup(filename);
	if (!item->filename) {
		ErrPrint("strdup: %d\n", errno);
		free(item);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}
	item->cb = cb;
	item->data = data;

	s_info.update_list = eina_list_append(s_info.update_list, item);
	return WIDGET_ERROR_NONE;
}

HAPI int update_monitor_add_delete_cb(const char *filename,
		int (*cb)(const char *filename, void *data, int over), void *data)
{
	struct cb_item *item;

	item = calloc(1, sizeof(*item));
	if (!item) {
		ErrPrint("calloc:%d", errno);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	item->filename = strdup(filename);
	if (!item->filename) {
		ErrPrint("strdup: %d\n", errno);
		free(item);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	item->cb = cb;
	item->data = data;

	s_info.delete_list = eina_list_append(s_info.delete_list, item);
	return WIDGET_ERROR_NONE;
}

HAPI void *update_monitor_del_update_cb(const char *filename,
		int (*cb)(const char *filename, void *data, int over))
{
	Eina_List *l;
	struct cb_item *item;

	EINA_LIST_FOREACH(s_info.update_list, l, item) {
		if (item->cb == cb && !strcmp(item->filename, filename)) {
			return update_item_destroy(item, l);
		}
	}

	return NULL;
}

HAPI void *update_monitor_del_delete_cb(const char *filename,
		int (*cb)(const char *filename, void *data, int over))
{
	Eina_List *l;
	struct cb_item *item;

	EINA_LIST_FOREACH(s_info.delete_list, l, item) {
		if (item->cb == cb && !strcmp(item->filename, filename)) {
			return delete_item_destroy(item, l);
		}
	}

	return NULL;
}

/* End of a file */
