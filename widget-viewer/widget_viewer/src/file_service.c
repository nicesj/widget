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

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib.h>

#include <dlog.h>
#include <secure_socket.h>
#include <widget_errno.h>

#include "client.h"
#include "debug.h"
#include "dlist.h"

#define FILE_SERVICE_PORT    8209

#define CRITICAL_SECTION_BEGIN(handle) \
	do { \
		int ret; \
		ret = pthread_mutex_lock(handle); \
		if (ret != 0) { \
			ErrPrint("Failed to lock: %s\n", strerror(ret)); \
		} \
	} while (0)

#define CRITICAL_SECTION_END(handle) \
	do { \
		int ret; \
		ret = pthread_mutex_unlock(handle); \
		if (ret != 0) { \
			ErrPrint("Failed to unlock: %s\n", strerror(ret)); \
		} \
	} while (0)

#define CANCEL_SECTION_BEGIN() do { \
	int ret; \
	ret = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL); \
	if (ret != 0) { \
		ErrPrint("Unable to set cancelate state: %s\n", strerror(ret)); \
	} \
} while (0)

#define CANCEL_SECTION_END() do { \
	int ret; \
	ret = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL); \
	if (ret != 0) { \
		ErrPrint("Unable to set cancelate state: %s\n", strerror(ret)); \
	} \
} while (0)

#define CLOSE_PIPE(p)    do { \
	int status; \
	status = close(p[PIPE_READ]); \
	if (status < 0) { \
		ErrPrint("close: %d\n", errno); \
	} \
	status = close(p[PIPE_WRITE]); \
	if (status < 0) { \
		ErrPrint("close: %d\n", errno); \
	} \
} while (0)

#define PIPE_READ 0
#define PIPE_WRITE 1
#define PIPE_MAX 2

#define EVT_END_CH    'c'
#define EVT_CH        'e'

static struct {
	pthread_t file_svc_thid;
	pthread_mutex_t file_svc_lock;
	int ctrl_pipe[PIPE_MAX];
	int evt_pipe[PIPE_MAX];
	struct dlist *request_list;
	int file_service_fd;
} s_info = {
	.ctrl_pipe = { -1, -1 },
	.evt_pipe = { -1, -1 },
	.request_list = NULL,
	.file_service_fd = -1,
};

struct request_item {
	char *filename;
	char *save_to;
	void (*result_cb)(const char *filename, const char *save_to, int ret, void *data);
	void *data;
	int ret;
};

/*!
 * File transfer header.
 * This must should be shared with client.
 */
struct burst_head {
	off_t size;
	int flen;
	char fname[];
};

struct burst_data {
	int size;
	char data[];
};

static inline int put_event_ch(int fd, char ch)
{
	int ret;

	ret = write(fd, &ch, sizeof(ch));
	if (ret != sizeof(ch)) {
		ErrPrint("write: %d\n", errno);
		return ret;
	}

	return 0;
}

static inline int get_event_ch(int fd)
{
	int ret;
	char ch;

	ret = read(fd, &ch, sizeof(ch));
	if (ret != sizeof(ch)) {
		ErrPrint("read: %d\n", errno);
		return ret;
	}

	ret = (int)((unsigned int)ch);
	return ret;
}

static inline int file_service_close(int fd)
{
	return secure_socket_destroy_handle(fd);
}

static inline int file_service_open(void)
{
	char *addr;
	int port;
	char *file_addr;
	int len;
	int fd;

	addr = malloc(strlen(client_addr()) + 1);
	if (!addr) {
		ErrPrint("Heap: %d\n", errno);
		return -ENOMEM;
	}

	if (sscanf(client_addr(), COM_CORE_REMOTE_SCHEME"%[^:]:%d", addr, &port) != 2) {
		ErrPrint("Invalid URL\n");
		free(addr);
		return -EINVAL;
	}

	len = strlen(COM_CORE_REMOTE_SCHEME);
	len += strlen(addr);
	len += 6;    /* Port length? */

	file_addr = malloc(len);
	if (!file_addr) {
		ErrPrint("Heap: %d\n", errno);
		free(addr);
		return -ENOMEM;
	}

	snprintf(file_addr, len, COM_CORE_REMOTE_SCHEME"%s:%d", addr, FILE_SERVICE_PORT);
	DbgPrint("File service: %s\n", file_addr);
	fd = secure_socket_create_client(file_addr);
	free(file_addr);
	free(addr);

	return fd;
}

/*!
 * Service Thread
 */
static void write_item_to_pipe(struct request_item *item, int ret)
{
	item->ret = WIDGET_ERROR_FAULT;
	if (write(s_info.evt_pipe[PIPE_WRITE], &item, sizeof(item)) != sizeof(item)) {
		ErrPrint("write: %d\n", errno);
		free(item->filename);
		free(item->save_to);
		free(item);
		item = NULL;
	}
}

/*!
 * Service Thread
 */
static void *file_service_main(void *data)
{
	int ret = 0;
	int select_fd;
	struct timeval tv;
	fd_set set;
	int offset;
	enum {
		RECV_INIT,
		RECV_HEADER,
		RECV_DATA,
	} recv_state;
	struct burst_head *head;
	struct burst_data *body;
	int recvsz;
	struct request_item *item;
	int file_offset;
	int file_fd;

	head = NULL;
	item = NULL;
	recv_state = RECV_INIT;
	select_fd = (s_info.file_service_fd > s_info.ctrl_pipe[PIPE_READ] ? s_info.file_service_fd : s_info.ctrl_pipe[PIPE_READ]) + 1;
	while (ret == 0) {
		FD_ZERO(&set);
		FD_SET(s_info.file_service_fd, &set);
		FD_SET(s_info.ctrl_pipe[PIPE_READ], &set);

		tv.tv_sec = 3;
		tv.tv_usec = 0;
		ret = select(select_fd , &set, NULL, NULL, &tv);
		if (ret < 0) {
			ret = -errno;
			if (errno == EINTR) {
				ErrPrint("INTERRUPTED\n");
				ret = 0;
				continue;
			}
			ErrPrint("Error: %d\n", errno);
			break;
		} else if (ret == 0) {
			ErrPrint("Timeout\n");
			ret = -ETIMEDOUT;
			break;
		}

		if (item && FD_ISSET(s_info.file_service_fd, &set)) {
			switch (recv_state) {
			case RECV_INIT:
				if (head == NULL) {
					recvsz = sizeof(*head);

					head = malloc(recvsz);
					if (!head) {
						ErrPrint("Heap: %d\n", errno);
						ret = WIDGET_ERROR_OUT_OF_MEMORY;
						write_item_to_pipe(item, ret);
						item = NULL;
						break;
					}

					offset = 0;
					recv_state = RECV_HEADER;
				}
			case RECV_HEADER:
				if (offset < recvsz) {
					ret = secure_socket_recv(s_info.file_service_fd, (char *)head + offset, recvsz - offset, NULL);
					if (ret > 0) {
						offset += ret;
					} else {
						free(head);
						head = NULL;
						recv_state = RECV_INIT;
						ret = WIDGET_ERROR_FAULT;
						write_item_to_pipe(item, ret);
						item = NULL;
						break;
					}
				}

				if (offset == sizeof(*head)) {
					void *tmp;

					recvsz += head->flen;

					tmp = realloc(head, recvsz);
					if (!tmp) {
						ErrPrint("Heap: %d\n", errno);

						free(head);
						head = NULL;
						recv_state = RECV_INIT;

						ret = WIDGET_ERROR_OUT_OF_MEMORY;
						write_item_to_pipe(item, ret);
						item = NULL;
						break;
					}

					head = tmp;
				} else if (offset == recvsz) {
					DbgPrint("Filesize: %d, name[%s]\n", head->size, head->fname);
					if (strcmp(item->filename, head->fname)) {
						ErrPrint("Invalid data sequence (%s <> %s)\n", item->filename, head->fname);

						free(head);
						head = NULL;
						recv_state = RECV_INIT;
						ret = WIDGET_ERROR_FAULT;
						write_item_to_pipe(item, ret);
						item = NULL;
						break;
					}

					file_fd = open(item->save_to, O_WRONLY|O_CREAT, 0644);
					if (file_fd < 0) {
						ErrPrint("open: %d\n", errno);
						free(head);
						head = NULL;
						recv_state = RECV_INIT;

						ret = WIDGET_ERROR_IO_ERROR;
						write_item_to_pipe(item, ret);
						item = NULL;
						break;
					}

					recv_state = RECV_DATA;
					body = NULL;

				} else {
					ErrPrint("Invalid state\n");
					free(head);
					head = NULL;
					recv_state = RECV_INIT;
					ret = WIDGET_ERROR_INVALID_PARAMETER;
					write_item_to_pipe(item, ret);
					item = NULL;
				}
				break;
			case RECV_DATA:
				if (!body) {
					body = malloc(sizeof(*body));
					if (!body) {
						free(head);
						head = NULL;
						recv_state = RECV_INIT;
						ret = WIDGET_ERROR_OUT_OF_MEMORY;
						write_item_to_pipe(item, ret);
						item = NULL;
						break;
					}

					recvsz = sizeof(*body);
					offset = 0;
				}

				ret = secure_socket_recv(s_info.file_service_fd, (char *)body + offset, recvsz - offset, NULL);
				if (ret > 0) {
					offset += ret;
				} else {
					free(head);
					head = NULL;
					free(body);
					body = NULL;
					recv_state = RECV_INIT;
					ret = WIDGET_ERROR_FAULT;
					write_item_to_pipe(item, ret);
					item = NULL;
					break;
				}

				if (offset == sizeof(*body)) {
					void *tmp;

					if (body->size < 0) {
						ErrPrint("body->size: %d\n", body->size);
						free(head);
						head = NULL;
						free(body);
						body = NULL;
						recv_state = RECV_INIT;
						ret = WIDGET_ERROR_FAULT;
						write_item_to_pipe(item, ret);
						item = NULL;
						break;
					}

					recvsz += body->size;

					tmp = realloc(body, recvsz);
					if (!tmp) {
						ErrPrint("Heap: %d\n", errno);
						free(head);
						head = NULL;

						free(body);
						body = NULL;
						recv_state = RECV_INIT;

						ret = WIDGET_ERROR_OUT_OF_MEMORY;
						write_item_to_pipe(item, ret);
						item = NULL;
						break;
					}
				} else if (offset == recvsz) {
					/* Flush this to the file */
					ret = write(file_fd, body->data, body->size);
					if (ret < 0) {
						ErrPrint("write: %d\n", errno);
						free(head);
						head = NULL;

						free(body);
						body = NULL;
						recv_state = RECV_INIT;

						ret = WIDGET_ERROR_IO_ERROR;
						write_item_to_pipe(item, ret);
						item = NULL;
						break;
					} else {
						if (body->size != ret) {
							DbgPrint("Body is not flushed correctly: %d, %d\n", ret, body->size);
							ret = body->size;
						}

						file_offset += ret;
						if (file_offset == head->size) {
							if (close(file_fd) < 0) {
								ErrPrint("close: %d\n", errno);
							}
							ret = WIDGET_ERROR_NONE;
							write_item_to_pipe(item, ret);
							item = NULL;
						}
					}

					free(body);
					body = NULL;

					free(head);
					head = NULL;

					recv_state = RECV_INIT;
				} else {
					ErrPrint("Invalid state\n");

					ret = -EFAULT;
					free(body);
					body = NULL;
					free(head);
					head = NULL;
					recv_state = RECV_INIT;

					ret = WIDGET_ERROR_FAULT;
					write_item_to_pipe(item, ret);
					item = NULL;
				}
				break;
			default:
				ErrPrint("Unknown event: %d\n", recv_state);
				ret = WIDGET_ERROR_FAULT;
				write_item_to_pipe(item, ret);
				item = NULL;
				break;
			}
		} else if (item == NULL && recv_state == RECV_INIT && FD_ISSET(s_info.ctrl_pipe[PIPE_READ], &set)) {
			int ch;
			struct dlist *l;

			/* Only if the recv state is not changed, we can get next request item */
			ch = get_event_ch(s_info.ctrl_pipe[PIPE_READ]);
			if (ch == EVT_END_CH) {
				DbgPrint("Service thread is canceled\n");
				break;
			}

			CRITICAL_SECTION_BEGIN(&s_info.file_svc_lock);
			l = dlist_nth(s_info.request_list, 0);
			item = dlist_data(l);
			s_info.request_list = dlist_remove(s_info.request_list, l);
			CRITICAL_SECTION_END(&s_info.file_svc_lock);
		}
	}

	return (void *)((long)ret);
}

/* Master */
static gboolean evt_cb(GIOChannel *src, GIOCondition cond, gpointer data)
{
	int fd;
	struct request_item *item;

	fd = g_io_channel_unix_get_fd(src);

	if (!(cond & G_IO_IN)) {
		DbgPrint("Client is disconencted\n");
		return FALSE;
	}

	if ((cond & G_IO_ERR) || (cond & G_IO_HUP) || (cond & G_IO_NVAL)) {
		DbgPrint("Client connection is lost\n");
		return FALSE;
	}

	if (read(fd, &item, sizeof(item)) != sizeof(item)) {
		ErrPrint("read: %d\n", errno);
	} else {
		if (item->result_cb) {
			item->result_cb(item->filename, item->save_to, item->ret, item->data);
		}

		free(item->filename);
		free(item->save_to);
		free(item);
	}

	return TRUE;
}

int file_service_send_request(const char *filename, const char *save_to, void (*result_cb)(const char *filename, const char *save_to, int ret, void *data), void *data)
{
	struct request_item *item;

	item = malloc(sizeof(*item));
	if (!item) {
		ErrPrint("Heap: %d\n", errno);
		return -ENOMEM;
	}

	item->filename = strdup(filename);
	if (!item->filename) {
		ErrPrint("Heap: %d\n", errno);
		free(item);
		return -ENOMEM;
	}

	item->save_to = strdup(save_to);
	if (!item->save_to) {
		ErrPrint("Heap: %d\n", errno);
		free(item->filename);
		free(item);
		return -ENOMEM;
	}

	item->result_cb = result_cb;
	item->data = data;

	CRITICAL_SECTION_BEGIN(&s_info.file_svc_lock);
	s_info.request_list = dlist_append(s_info.request_list, item);
	CRITICAL_SECTION_END(&s_info.file_svc_lock);
	return 0;
}

int file_service_init(void)
{
	int status;
	GIOChannel *gio;
	guint id;

	if (strncmp(client_addr(), COM_CORE_REMOTE_SCHEME, strlen(COM_CORE_REMOTE_SCHEME))) {
		return 0;
	}

	s_info.file_service_fd = file_service_open();
	if (s_info.file_service_fd < 0) {
		return -EFAULT;
	}

	if (pipe2(s_info.ctrl_pipe, O_NONBLOCK | O_CLOEXEC) < 0) {
		ErrPrint("file service: %d\n", errno);
		file_service_close(s_info.file_service_fd);
		s_info.file_service_fd = -1;
		return -EFAULT;
	}

	if (pipe2(s_info.evt_pipe, O_NONBLOCK | O_CLOEXEC) < 0) {
		ErrPrint("file service: %d\n", errno);
		CLOSE_PIPE(s_info.ctrl_pipe);
		file_service_close(s_info.file_service_fd);
		s_info.file_service_fd = -1;
		return -EFAULT;
	}

	status = pthread_mutex_init(&s_info.file_svc_lock, NULL);
	if (status != 0) {
		ErrPrint("Mutex: %s\n", strerror(status));
		CLOSE_PIPE(s_info.ctrl_pipe);
		CLOSE_PIPE(s_info.evt_pipe);
		file_service_close(s_info.file_service_fd);
		s_info.file_service_fd = -1;
		return -EFAULT;
	}

	gio = g_io_channel_unix_new(s_info.evt_pipe[PIPE_READ]);
	if (!gio) {
		ErrPrint("io channel new\n");
		status = pthread_mutex_destroy(&s_info.file_svc_lock);
		if (status != 0) {
			ErrPrint("destroy: %s\n", strerror(status));
		}
		CLOSE_PIPE(s_info.ctrl_pipe);
		CLOSE_PIPE(s_info.evt_pipe);
		file_service_close(s_info.file_service_fd);
		s_info.file_service_fd = -1;
		return -EFAULT;
	}

	g_io_channel_set_close_on_unref(gio, FALSE);

	id = g_io_add_watch(gio, G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL, (GIOFunc)evt_cb, NULL);
	if (id <= 0) {
		GError *err = NULL;
		ErrPrint("Failed to add IO watch\n");
		g_io_channel_shutdown(gio, TRUE, &err);
		if (err) {
			ErrPrint("Shutdown: %s\n", err->message);
			g_error_free(err);
		}
		g_io_channel_unref(gio);

		status = pthread_mutex_destroy(&s_info.file_svc_lock);
		if (status != 0) {
			ErrPrint("destroy: %s\n", strerror(status));
		}
		CLOSE_PIPE(s_info.ctrl_pipe);
		CLOSE_PIPE(s_info.evt_pipe);
		file_service_close(s_info.file_service_fd);
		s_info.file_service_fd = -1;
		return -EIO;
	}

	status = pthread_create(&s_info.file_svc_thid, NULL, file_service_main, NULL);
	if (status != 0) {
		GError *err = NULL;
		ErrPrint("Failed to add IO watch\n");
		g_io_channel_shutdown(gio, TRUE, &err);
		if (err) {
			ErrPrint("Shutdown: %s\n", err->message);
			g_error_free(err);
		}
		g_io_channel_unref(gio);

		ErrPrint("file service: %s\n", strerror(status));
		CLOSE_PIPE(s_info.ctrl_pipe);
		CLOSE_PIPE(s_info.evt_pipe);
		file_service_close(s_info.file_service_fd);
		s_info.file_service_fd = -1;

		status = pthread_mutex_destroy(&s_info.file_svc_lock);
		if (status != 0) {
			ErrPrint("destroy: %s\n", strerror(status));
		}

		return -EFAULT;
	}

	g_io_channel_unref(gio);
	return 0;
}

int file_service_fini(void)
{
	void *svc_ret;
	int ret;

	if (strncmp(client_addr(), COM_CORE_REMOTE_SCHEME, strlen(COM_CORE_REMOTE_SCHEME))) {
		return 0;
	}

	(void)put_event_ch(s_info.ctrl_pipe[PIPE_WRITE], EVT_END_CH);

	ret = pthread_join(s_info.file_svc_thid, &svc_ret);
	if (ret != 0) {
		ErrPrint("join: %s\n", strerror(ret));
	} else {
		DbgPrint("file svc returns: %p\n", svc_ret);
	}

	ret = pthread_mutex_destroy(&s_info.file_svc_lock);
	if (ret != 0) {
		ErrPrint("destroy: %s\n", strerror(ret));
	}

	CLOSE_PIPE(s_info.evt_pipe);
	CLOSE_PIPE(s_info.ctrl_pipe);

	return 0;
}

/* End of a file */
