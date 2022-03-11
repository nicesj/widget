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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <Eina.h>

#include <com-core.h>
#include <packet.h>
#include <secure_socket.h>
#include <com-core_packet.h>
#include <widget_errno.h>
#include <widget_cmd_list.h>

#include <dlog.h>

#include "debug.h"
#include "connection.h"

static struct info {
	Eina_List *connection_list;
	Eina_List *connected_list;
	Eina_List *disconnected_list;

	enum {
		IDLE = 0x00,
		DISCONNECTION = 0x01,
		CONNECTION = 0x02,
	} process;
} s_info = {
	.connection_list = NULL,
	.connected_list = NULL,
	.disconnected_list = NULL,
	.process = IDLE,
};

int errno;

struct event_item {
	int (*event_cb)(int handle, void *data);
	void *data;
	int deleted;
};

struct connection {
	char *addr;
	int fd;
	int refcnt;
	Eina_List *data_list;
};

struct data_item {
	char *tag;
	void *data;
};

/**
 * When we get this connected callback,
 * The connection handle is not prepared yet.
 * So it is not possible to find a connection handle using socket fd.
 * In this case, just propagate this event to upper layer.
 * Make them handles this.
 */
static int connected_cb(int handle, void *data)
{
	Eina_List *l;
	Eina_List *n;
	struct event_item *item;

	EINA_LIST_FOREACH_SAFE(s_info.connected_list, l, n, item) {
		s_info.process = CONNECTION;
		if (item->deleted || item->event_cb(handle, item->data) < 0 || item->deleted) {
			s_info.connected_list = eina_list_remove(s_info.connected_list, item);
			free(item);
		}
		s_info.process = IDLE;
	}

	return 0;
}

static int disconnected_cb(int handle, void *data)
{
	Eina_List *l;
	Eina_List *n;
	struct event_item *item;

	EINA_LIST_FOREACH_SAFE(s_info.disconnected_list, l, n, item) {
		s_info.process = DISCONNECTION;
		if (item->deleted || item->event_cb(handle, item->data) < 0 || item->deleted) {
			s_info.disconnected_list = eina_list_remove(s_info.disconnected_list, item);
			free(item);
		}
		s_info.process = IDLE;
	}

	return 0;
}

int connection_init(void)
{
	if (com_core_add_event_callback(CONNECTOR_DISCONNECTED, disconnected_cb, NULL) < 0) {
		ErrPrint("Unable to register the disconnected callback\n");
	}

	if (com_core_add_event_callback(CONNECTOR_CONNECTED, connected_cb, NULL) < 0) {
		ErrPrint("Unable to register the disconnected callback\n");
	}

	return WIDGET_ERROR_NONE;
}

int connection_fini(void)
{
	(void)com_core_del_event_callback(CONNECTOR_DISCONNECTED, disconnected_cb, NULL);
	(void)com_core_del_event_callback(CONNECTOR_CONNECTED, connected_cb, NULL);
	return WIDGET_ERROR_NONE;
}

struct connection *connection_create(const char *addr, void *table)
{
	struct connection *handle;

	handle = calloc(1, sizeof(*handle));
	if (!handle) {
		ErrPrint("calloc: %d\n", errno);
		return NULL;
	}

	handle->addr = strdup(addr);
	if (!handle->addr) {
		ErrPrint("strdup: %d (%s)\n", errno, addr);
		free(handle);
		return NULL;
	}

	handle->fd = com_core_packet_client_init(handle->addr, 0, table);
	if (handle->fd < 0) {
		struct packet *packet;

		packet = packet_create(CMD_STR_DIRECT_CONNECTED, "s", addr);
		if (packet) {
			struct packet *result;

			DbgPrint("Try to get a connection through SHARED_SOCKET\n");
			result = com_core_packet_oneshot_send(SHARED_SOCKET, packet, 1.0f);
			if (result) {
				int ret;
				if (packet_get(result, "i", &ret) != 1) {
					ErrPrint("Invalid parameter\n");
				} else {
					if (ret == WIDGET_ERROR_NONE) {
						int fd;
						fd = packet_fd(result);
						if (fd >= 0) {
							handle->fd = com_core_packet_client_init_by_fd(fd, 0, table);
							DbgPrint("Handle acquired: %d (%d) - %d\n", handle->fd, ret, fd);
						} else {
							ErrPrint("Handle is not acquired: %d\n", fd);
						}
					} else {
						ErrPrint("Error: %d\n", ret);
					}
				}
				packet_destroy(result);
			} else {
				ErrPrint("Cannot retrieve the result packet\n");
			}
			packet_destroy(packet);
		} else {
			ErrPrint("Failed to create a packet\n");
		}

		if (handle->fd < 0) {
			ErrPrint("Unable to make a connection %s\n", handle->addr);
			free(handle->addr);
			free(handle);
			return NULL;
		}
	} else {
		DbgPrint("Client connection initiated: %s\n", handle->addr);
	}

	handle->refcnt = 1;

	s_info.connection_list = eina_list_append(s_info.connection_list, handle);
	return handle;
}

struct connection *connection_ref(struct connection *handle)
{
	if (!handle) {
		return NULL;
	}

	handle->refcnt++;
	return handle;
}

struct connection *connection_unref(struct connection *handle)
{
	struct data_item *item;

	if (!handle) {
		return NULL;
	}

	handle->refcnt--;
	if (handle->refcnt > 0) {
		return handle;
	}

	s_info.connection_list = eina_list_remove(s_info.connection_list, handle);

	if (handle->fd >= 0) {
		com_core_packet_client_fini(handle->fd);
	}

	EINA_LIST_FREE(handle->data_list, item) {
		free(item->tag);
		free(item);
	}

	free(handle->addr);
	free(handle);
	return NULL;
}

struct connection *connection_find_by_addr(const char *addr)
{
	Eina_List *l;
	struct connection *handle;

	if (!addr) {
		return NULL;
	}

	EINA_LIST_FOREACH(s_info.connection_list, l, handle) {
		if (handle->addr && !strcmp(handle->addr, addr)) {
			return handle;
		}
	}

	return NULL;
}

struct connection *connection_find_by_fd(int fd)
{
	Eina_List *l;
	struct connection *handle;

	if (fd < 0) {
		return NULL;
	}

	EINA_LIST_FOREACH(s_info.connection_list, l, handle) {
		if (handle->fd == fd) {
			return handle;
		}
	}

	return NULL;
}

int connection_add_event_handler(enum connection_event_type type, int (*event_cb)(int handle, void *data), void *data)
{
	struct event_item *item;

	item = malloc(sizeof(*item));
	if (!item) {
		ErrPrint("malloc: %d\n", errno);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	item->event_cb = event_cb;
	item->data = data;

	switch (type) {
	case CONNECTION_EVENT_TYPE_CONNECTED:
		s_info.connected_list = eina_list_append(s_info.connected_list, item);
		break;
	case CONNECTION_EVENT_TYPE_DISCONNECTED:
		s_info.disconnected_list = eina_list_append(s_info.disconnected_list, item);
		break;
	default:
		free(item);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return WIDGET_ERROR_NONE;
}

void *connection_del_event_handler(enum connection_event_type type, int (*event_cb)(int handle, void *data))
{
	Eina_List *l;
	Eina_List *n;
	struct event_item *item;
	void *ret = NULL;

	switch (type) {
	case CONNECTION_EVENT_TYPE_CONNECTED:
		EINA_LIST_FOREACH_SAFE(s_info.connected_list, l, n, item) {
			if (item->event_cb == event_cb) {
				if (s_info.process == CONNECTION) {
					item->deleted = 1;
					ret = item->data;
				} else {
					s_info.connected_list = eina_list_remove(s_info.connected_list, item);
					ret = item->data;
					free(item);
				}
				break;
			}
		}
		break;
	case CONNECTION_EVENT_TYPE_DISCONNECTED:
		EINA_LIST_FOREACH_SAFE(s_info.disconnected_list, l, n, item) {
			if (item->event_cb == event_cb) {
				if (s_info.process == DISCONNECTION) {
					item->deleted = 1;
					ret = item->data;
				} else {
					s_info.disconnected_list = eina_list_remove(s_info.disconnected_list, item);
					ret = item->data;
					free(item);
				}
				break;
			}
		}
		break;
	default:
		break;
	}

	return ret;
}

int connection_handle(struct connection *connection)
{
	return connection->fd;
}

const char *connection_addr(struct connection *connection)
{
	return connection->addr;
}

static struct data_item *connection_find_data_item(struct connection *connection, const char *key)
{
	Eina_List *l;
	struct data_item *item;

	EINA_LIST_FOREACH(connection->data_list, l, item) {
		if (!strcmp(item->tag, key)) {
			return item;
		}
	}

	return NULL;
}

int connection_set_data(struct connection *connection, const char *key, void *data)
{
	struct data_item *item;

	item = connection_find_data_item(connection, key);
	if (item) {
		return WIDGET_ERROR_ALREADY_EXIST;
	}

	item = malloc(sizeof(*item));
	if (!item) {
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	item->tag = strdup(key);
	if (!item->tag) {
		free(item);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	item->data = data;

	connection->data_list = eina_list_append(connection->data_list, item);
	return WIDGET_ERROR_NONE;
}

void *connection_del_data(struct connection *connection, const char *key)
{
	struct data_item *item;
	void *data;

	item = connection_find_data_item(connection, key);
	if (!item) {
		return NULL;
	}

	connection->data_list = eina_list_remove(connection->data_list, item);
	data = item->data;
	free(item->tag);
	free(item);
	return data;
}

void *connection_get_data(struct connection *connection, const char *key)
{
	struct data_item *item;

	item = connection_find_data_item(connection, key);
	if (!item) {
		return NULL;
	}

	return item->data;
}

/* End of a file */
