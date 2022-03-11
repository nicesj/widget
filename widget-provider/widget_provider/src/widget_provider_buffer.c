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
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#include <com-core.h>
#include <packet.h>
#include <com-core_packet.h>

#include <dlog.h>
#include <widget_errno.h>
#include <widget_service.h>
#include <widget_service_internal.h>
#include <widget_cmd_list.h>
#include <widget_buffer.h>
#include <widget_conf.h>

#include "dlist.h"
#include "util.h"
#include "debug.h"
#include "widget_provider.h"
#include "fb.h"
#include "widget_provider_buffer.h"
#include "provider_buffer_internal.h"

#define EAPI __attribute__((visibility("default")))

#define ACCESS_TYPE_DOWN 0
#define ACCESS_TYPE_MOVE 1
#define ACCESS_TYPE_UP 2
#define ACCESS_TYPE_CUR 0
#define ACCESS_TYPE_NEXT 1
#define ACCESS_TYPE_PREV 2
#define ACCESS_TYPE_OFF 3

static struct {
	struct dlist *buffer_list;
} s_info = {
	.buffer_list = NULL,
};

static unsigned int send_acquire_extra_request(widget_target_type_e type, const char *pkgname, const char *id, int w, int h, int size, int idx)
{
	unsigned int cmd = CMD_ACQUIRE_XBUFFER;
	struct packet *packet;
	struct packet *result;
	const char *buffer_id;
	int status;
	unsigned int pixmap;

	packet = packet_create((const char *)&cmd, "issiiii", type, pkgname, id, w, h, size, idx);
	if (!packet) {
		ErrPrint("Failed to build a packet\n");
		return 0u;
	}

	result = com_core_packet_oneshot_send(SLAVE_SOCKET, packet, 0.0f);
	packet_destroy(packet);

	if (!result) {
		ErrPrint("Failed to send a request\n");
		return 0u;
	}

	if (packet_get(result, "is", &status, &buffer_id) != 2) {
		ErrPrint("Failed to get a result packet\n");
		packet_unref(result);
		return 0u;
	}

	if (status != 0) {
		ErrPrint("Failed to acquire buffer: %d\n", status);
		packet_unref(result);
		return 0u;
	}

	if (sscanf(buffer_id, "pixmap://%u:%d", &pixmap, &size) != 2) {
		ErrPrint("Invalid buffer_id[%s]\n", buffer_id);
		packet_unref(result);
		return 0u;
	}

	DbgPrint("type: 0x%X, name: %s, pkgname[%s], id[%s], w[%d], h[%d], size[%d], buffer_id[%s], pixmap[%u]\n", type, provider_name(), pkgname, id, w, h, size, buffer_id, pixmap);

	packet_unref(result);
	return pixmap;
}

static struct fb_info *send_acquire_request(widget_target_type_e type, const char *pkgname, const char *id, int w, int h, int size)
{
	unsigned int cmd = CMD_ACQUIRE_BUFFER;
	struct packet *packet;
	struct packet *result;
	const char *buffer_id;
	struct fb_info *ret;
	int status;

	packet = packet_create((const char *)&cmd, "issiii", type, pkgname, id, w, h, size);
	if (!packet) {
		ErrPrint("Failed to build a packet\n");
		return NULL;
	}

	result = com_core_packet_oneshot_send(SLAVE_SOCKET, packet, 0.0f);
	packet_destroy(packet);

	if (!result) {
		ErrPrint("Failed to send a request\n");
		return NULL;
	}

	if (packet_get(result, "is", &status, &buffer_id) != 2) {
		ErrPrint("Failed to get a result packet\n");
		packet_unref(result);
		return NULL;
	}

	if (status != 0) {
		ErrPrint("Failed to acquire buffer: %d\n", status);
		packet_unref(result);
		return NULL;
	}

	ret = fb_create(buffer_id, w, h);
	/*!
	 * \TODO: Implements me
	 */
	DbgPrint("type: 0x%X, name: %s, pkgname[%s], id[%s], w[%d], h[%d], size[%d], buffer_id[%s], fb[%p]\n", type, provider_name(), pkgname, id, w, h, size, buffer_id, ret);
	packet_unref(result);

	return ret;
}

static inline int send_release_extra_request(widget_target_type_e type, const char *pkgname, const char *id, int idx)
{
	unsigned int cmd = CMD_RELEASE_XBUFFER;
	struct packet *packet;
	struct packet *result;
	int ret;

	packet = packet_create((const char *)&cmd, "issi", type, pkgname, id, idx);
	if (!packet) {
		ErrPrint("Failed to build a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	result = com_core_packet_oneshot_send(SLAVE_SOCKET, packet, 0.0f);
	packet_destroy(packet);
	if (!result) {
		ErrPrint("Failed to send a request\n");
		return WIDGET_ERROR_FAULT;
	}

	if (packet_get(result, "i", &ret) != 1) {
		ErrPrint("Invalid result packet\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet_unref(result);
	return ret;
}

static inline int send_release_request(widget_target_type_e type, const char *pkgname, const char *id)
{
	unsigned int cmd = CMD_RELEASE_BUFFER;
	struct packet *packet;
	struct packet *result;
	int ret;

	packet = packet_create((const char *)&cmd, "iss", type, pkgname, id);
	if (!packet) {
		ErrPrint("Failed to build a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	result = com_core_packet_oneshot_send(SLAVE_SOCKET, packet, 0.0f);
	packet_destroy(packet);
	if (!result) {
		ErrPrint("Failed to send a request\n");
		return WIDGET_ERROR_FAULT;
	}

	if (packet_get(result, "i", &ret) != 1) {
		ErrPrint("Invalid result packet\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet_unref(result);
	return ret;
}

static inline struct fb_info *send_resize_request(widget_target_type_e type, const char *pkgname, const char *id, int w, int h)
{
	unsigned int cmd = CMD_RESIZE_BUFFER;
	struct packet *packet;
	struct packet *result;
	const char *buffer_id;
	struct fb_info *fb;
	int ret;

	packet = packet_create((const char *)&cmd, "issii", type, pkgname, id, w, h);
	if (!packet) {
		ErrPrint("Faield to build a packet\n");
		return NULL;
	}

	result = com_core_packet_oneshot_send(SLAVE_SOCKET, packet, 0.0f);
	packet_destroy(packet);
	if (!result) {
		ErrPrint("Failed to send a request\n");
		return NULL;
	}

	if (packet_get(result, "is", &ret, &buffer_id) != 2) {
		ErrPrint("Invalid result packet\n");
		packet_unref(result);
		return NULL;
	}

	fb = (ret == 0) ? fb_create(buffer_id, w, h) : NULL;
	packet_unref(result);
	return fb;
}

EAPI int widget_provider_buffer_init(void *display)
{
	return fb_init(display);
}

EAPI int widget_provider_buffer_fini(void)
{
	return fb_fini();
}

int provider_buffer_direct_key_down(widget_buffer_h info, widget_buffer_event_data_t data)
{
	if (!info || !data) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	data->type = WIDGET_BUFFER_EVENT_KEY_DOWN;

	if (info->handler) {
		(void)info->handler(info, data, info->data);
	}

	return WIDGET_ERROR_NONE;
}

int provider_buffer_direct_key_up(widget_buffer_h info, widget_buffer_event_data_t data)
{
	if (!info || !data) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	data->type = WIDGET_BUFFER_EVENT_KEY_UP;

	if (info->handler) {
		(void)info->handler(info, data, info->data);
	}

	return WIDGET_ERROR_NONE;
}

int provider_buffer_direct_key_focus_in(widget_buffer_h info, widget_buffer_event_data_t data)
{
	if (!info || !data) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	data->type = WIDGET_BUFFER_EVENT_KEY_FOCUS_IN;

	if (info->handler) {
		(void)info->handler(info, data, info->data);
	}

	return WIDGET_ERROR_NONE;
}

int provider_buffer_direct_key_focus_out(widget_buffer_h info, widget_buffer_event_data_t data)
{
	if (!info || !data) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	data->type = WIDGET_BUFFER_EVENT_KEY_FOCUS_OUT;

	if (info->handler) {
		(void)info->handler(info, data, info->data);
	}

	return WIDGET_ERROR_NONE;
}

int provider_buffer_direct_mouse_enter(widget_buffer_h info, widget_buffer_event_data_t data)
{
	if (!info || !data) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	data->type = WIDGET_BUFFER_EVENT_ENTER;

	if (info->handler) {
		(void)info->handler(info, data, info->data);
	}

	return WIDGET_ERROR_NONE;
}

int provider_buffer_direct_mouse_leave(widget_buffer_h info, widget_buffer_event_data_t data)
{
	if (!info || !data) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	data->type = WIDGET_BUFFER_EVENT_LEAVE;

	if (info->handler) {
		(void)info->handler(info, data, info->data);
	}

	return WIDGET_ERROR_NONE;
}

int provider_buffer_direct_mouse_down(widget_buffer_h info, widget_buffer_event_data_t data)
{
	if (!info || !data) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	DbgPrint("DOWN: %dx%d (%lf)\n", data->info.pointer.x, data->info.pointer.y, data->timestamp);
	data->type = WIDGET_BUFFER_EVENT_DOWN;

	if (info->handler) {
		(void)info->handler(info, data, info->data);
	}

	return WIDGET_ERROR_NONE;
}

int provider_buffer_direct_mouse_up(widget_buffer_h info, widget_buffer_event_data_t data)
{
	if (!info || !data) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	DbgPrint("UP: %dx%d (%lf)\n", data->info.pointer.x, data->info.pointer.y, data->timestamp);
	data->type = WIDGET_BUFFER_EVENT_UP;

	if (info->handler) {
		(void)info->handler(info, data, info->data);
	}

	return WIDGET_ERROR_NONE;
}

int provider_buffer_direct_mouse_move(widget_buffer_h info, widget_buffer_event_data_t data)
{
	if (!info || !data) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	data->type = WIDGET_BUFFER_EVENT_MOVE;

	if (info->handler) {
		(void)info->handler(info, data, info->data);
	}

	return WIDGET_ERROR_NONE;
}

struct packet *provider_buffer_widget_key_down(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_KEY_DOWN,
	};

	if (packet_get(packet, "ssdii", &pkgname, &id, &data.timestamp, &data.info.key.code, &data.info.key.device) != 5) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	} else {
		(void)widget_provider_send_key_status(pkgname, id, WIDGET_KEY_STATUS_ERROR);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_key_up(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_KEY_UP,
	};

	if (packet_get(packet, "ssdii", &pkgname, &id, &data.timestamp, &data.info.key.code, &data.info.key.device) != 5) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	} else {
		(void)widget_provider_send_key_status(pkgname, id, WIDGET_KEY_STATUS_ERROR);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_key_focus_in(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_KEY_FOCUS_IN,
	};

	if (packet_get(packet, "ssdii", &pkgname, &id, &data.timestamp, &data.info.key.code, &data.info.key.device) != 5) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	} else {
		(void)widget_provider_send_key_status(pkgname, id, WIDGET_KEY_STATUS_ERROR);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_key_focus_out(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_KEY_FOCUS_OUT,
	};

	if (packet_get(packet, "ssdii", &pkgname, &id, &data.timestamp, &data.info.key.code, &data.info.key.device) != 5) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	} else {
		(void)widget_provider_send_key_status(pkgname, id, WIDGET_KEY_STATUS_ERROR);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_mouse_enter(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	double ratio_w;
	double ratio_h;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_ENTER,
	};

	if (packet_get(packet, "ssdiiiddi", &pkgname, &id, &data.timestamp, &data.info.pointer.x, &data.info.pointer.y, (int *)&data.info.pointer.source, &ratio_w, &ratio_h, &data.info.pointer.device) != 9) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		if (ratio_w != 1.0f || ratio_h != 1.0f) {
			data.info.pointer.x *= ratio_w;
			data.info.pointer.y *= ratio_h;
		}

		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_mouse_leave(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	double ratio_w;
	double ratio_h;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_LEAVE,
	};

	if (packet_get(packet, "ssdiiiddi", &pkgname, &id, &data.timestamp, &data.info.pointer.x, &data.info.pointer.y, (int *)&data.info.pointer.source, &ratio_w, &ratio_h, &data.info.pointer.device) != 9) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		if (ratio_w != 1.0f || ratio_h != 1.0f) {
			data.info.pointer.x *= ratio_w;
			data.info.pointer.y *= ratio_h;
		}
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_mouse_down(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	double ratio_w;
	double ratio_h;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_DOWN,
	};

	if (packet_get(packet, "ssdiiiddi", &pkgname, &id, &data.timestamp, &data.info.pointer.x, &data.info.pointer.y, (int *)&data.info.pointer.source, &ratio_w, &ratio_h, &data.info.pointer.device) != 9) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	DbgPrint("Down: %s\n", id);
	if (info->handler) {
		if (ratio_w != 1.0f || ratio_h != 1.0f) {
			data.info.pointer.x *= ratio_w;
			data.info.pointer.y *= ratio_h;
		}

		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_mouse_up(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	double ratio_w;
	double ratio_h;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_UP,
	};

	if (packet_get(packet, "ssdiiiddi", &pkgname, &id, &data.timestamp, &data.info.pointer.x, &data.info.pointer.y, (int *)&data.info.pointer.source, &ratio_w, &ratio_h, &data.info.pointer.device) != 9) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	DbgPrint("Up: %s\n", id);
	if (info->handler) {
		if (ratio_w != 1.0f || ratio_h != 1.0f) {
			data.info.pointer.x *= ratio_w;
			data.info.pointer.y *= ratio_h;
		}

		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_mouse_move(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	double ratio_w;
	double ratio_h;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_MOVE,
	};

	if (packet_get(packet, "ssdiiiddi", &pkgname, &id, &data.timestamp, &data.info.pointer.x, &data.info.pointer.y, (int *)&data.info.pointer.source, &ratio_w, &ratio_h, &data.info.pointer.device) != 9) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		if (ratio_w != 1.0f || ratio_h != 1.0f) {
			data.info.pointer.x *= ratio_w;
			data.info.pointer.y *= ratio_h;
		}

		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_key_down(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_KEY_DOWN,
	};

	if (packet_get(packet, "ssdii", &pkgname, &id, &data.timestamp, &data.info.key.code, &data.info.key.device) != 5) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	} else {
		(void)widget_provider_send_key_status(pkgname, id, WIDGET_KEY_STATUS_ERROR);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_key_up(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_KEY_UP,
	};

	if (packet_get(packet, "ssdii", &pkgname, &id, &data.timestamp, &data.info.key.code, &data.info.key.device) != 5) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	} else {
		(void)widget_provider_send_key_status(pkgname, id, WIDGET_KEY_STATUS_ERROR);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_key_focus_in(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_KEY_FOCUS_IN,
	};

	if (packet_get(packet, "ssdii", &pkgname, &id, &data.timestamp, &data.info.key.code, &data.info.key.device) != 5) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	} else {
		(void)widget_provider_send_key_status(pkgname, id, WIDGET_KEY_STATUS_ERROR);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_key_focus_out(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_KEY_FOCUS_OUT,
	};

	if (packet_get(packet, "ssdii", &pkgname, &id, &data.timestamp, &data.info.key.code, &data.info.key.device) != 5) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	} else {
		(void)widget_provider_send_key_status(pkgname, id, WIDGET_KEY_STATUS_ERROR);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_mouse_enter(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	double ratio_w;
	double ratio_h;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_ENTER,
	};

	if (packet_get(packet, "ssdiiiddi", &pkgname, &id, &data.timestamp, &data.info.pointer.x, &data.info.pointer.y, (int *)&data.info.pointer.source, &ratio_w, &ratio_h, &data.info.pointer.device) != 9) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		if (ratio_w != 1.0f || ratio_h != 1.0f) {
			data.info.pointer.x *= ratio_w;
			data.info.pointer.y *= ratio_h;
		}

		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_mouse_leave(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	double ratio_w;
	double ratio_h;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_LEAVE,
	};

	if (packet_get(packet, "ssdiiiddi", &pkgname, &id, &data.timestamp, &data.info.pointer.x, &data.info.pointer.y, (int *)&data.info.pointer.source, &ratio_w, &ratio_h, &data.info.pointer.device) != 9) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		if (ratio_w != 1.0f || ratio_h != 1.0f) {
			data.info.pointer.x *= ratio_w;
			data.info.pointer.y *= ratio_h;
		}

		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_mouse_down(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	double ratio_w;
	double ratio_h;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_DOWN,
	};

	if (packet_get(packet, "ssdiiiddi", &pkgname, &id, &data.timestamp, &data.info.pointer.x, &data.info.pointer.y, (int *)&data.info.pointer.source, &ratio_w, &ratio_h, &data.info.pointer.device) != 9) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		if (ratio_w != 1.0f || ratio_h != 1.0f) {
			data.info.pointer.x *= ratio_w;
			data.info.pointer.y *= ratio_h;
		}

		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_mouse_up(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	double ratio_w;
	double ratio_h;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_UP,
	};

	if (packet_get(packet, "ssdiiiddi", &pkgname, &id, &data.timestamp, &data.info.pointer.x, &data.info.pointer.y, (int *)&data.info.pointer.source, &ratio_w, &ratio_h, &data.info.pointer.device) != 9) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		if (ratio_w != 1.0f || ratio_h != 1.0f) {
			data.info.pointer.x *= ratio_w;
			data.info.pointer.y *= ratio_h;
		}

		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_mouse_move(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_MOVE,
	};
	double ratio_w;
	double ratio_h;

	if (packet_get(packet, "ssdiiiddi", &pkgname, &id, &data.timestamp, &data.info.pointer.x, &data.info.pointer.y, (int *)&data.info.pointer.source, &ratio_w, &ratio_h, &data.info.pointer.device) != 9) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		if (ratio_w != 1.0f || ratio_h != 1.0f) {
			data.info.pointer.x *= ratio_w;
			data.info.pointer.y *= ratio_h;
		}

		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_access_action(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	int ret;
	widget_buffer_h info;
	struct widget_buffer_event_data data;

	ret = packet_get(packet, "ssdiii", &pkgname, &id, &data.timestamp, &data.info.access.x, &data.info.access.y, &data.info.access.mouse_type);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	switch (data.info.access.mouse_type) {
	case ACCESS_TYPE_UP:
		data.type = WIDGET_BUFFER_EVENT_ACCESS_ACTION_UP;
		break;
	case ACCESS_TYPE_DOWN:
		data.type = WIDGET_BUFFER_EVENT_ACCESS_ACTION_DOWN;
		break;
	default:
		ErrPrint("Invalid type\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	} else {
		/* Event handler is not ready */
		(void)widget_provider_send_access_status(pkgname, id, WIDGET_ACCESS_STATUS_ERROR);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_access_scroll(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	int ret;
	widget_buffer_h info;
	struct widget_buffer_event_data data;

	ret = packet_get(packet, "ssdiii", &pkgname, &id, &data.timestamp, &data.info.access.x, &data.info.access.y, &data.info.access.mouse_type);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	switch (data.info.access.mouse_type) {
	case ACCESS_TYPE_DOWN:
		data.type = WIDGET_BUFFER_EVENT_ACCESS_SCROLL_DOWN;
		break;
	case ACCESS_TYPE_MOVE:
		data.type = WIDGET_BUFFER_EVENT_ACCESS_SCROLL_MOVE;
		break;
	case ACCESS_TYPE_UP:
		data.type = WIDGET_BUFFER_EVENT_ACCESS_SCROLL_UP;
		break;
	default:
		ErrPrint("Invalid type\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	} else {
		/* Event handler is not ready */
		(void)widget_provider_send_access_status(pkgname, id, WIDGET_ACCESS_STATUS_ERROR);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_access_hl(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	int ret;
	widget_buffer_h info;
	struct widget_buffer_event_data data;

	ret = packet_get(packet, "ssdiii", &pkgname, &id, &data.timestamp, &data.info.access.x, &data.info.access.y, &data.info.access.mouse_type);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	switch (data.info.access.mouse_type) {
	case ACCESS_TYPE_CUR:
		data.type = WIDGET_BUFFER_EVENT_ACCESS_HIGHLIGHT;
		break;
	case ACCESS_TYPE_NEXT:
		data.type = WIDGET_BUFFER_EVENT_ACCESS_HIGHLIGHT_NEXT;
		break;
	case ACCESS_TYPE_PREV:
		data.type = WIDGET_BUFFER_EVENT_ACCESS_HIGHLIGHT_PREV;
		break;
	case ACCESS_TYPE_OFF:
		data.type = WIDGET_BUFFER_EVENT_ACCESS_UNHIGHLIGHT;
		break;
	default:
		ErrPrint("Invalid type\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	} else {
		/* Event handler is not ready */
		(void)widget_provider_send_access_status(pkgname, id, WIDGET_ACCESS_STATUS_ERROR);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_access_activate(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	int ret;
	widget_buffer_h info;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_ACCESS_ACTIVATE,
	};

	ret = packet_get(packet, "ssdiii", &pkgname, &id, &data.timestamp, &data.info.access.x, &data.info.access.y, &data.info.access.mouse_type);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	} else {
		/* Event handler is not ready */
		(void)widget_provider_send_access_status(pkgname, id, WIDGET_ACCESS_STATUS_ERROR);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_access_hl(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	int ret;
	widget_buffer_h info;
	struct widget_buffer_event_data data;

	ret = packet_get(packet, "ssdiii", &pkgname, &id, &data.timestamp, &data.info.access.x, &data.info.access.y, &data.info.access.mouse_type);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	switch (data.info.access.mouse_type) {
	case ACCESS_TYPE_CUR:
		data.type = WIDGET_BUFFER_EVENT_ACCESS_HIGHLIGHT;
		break;
	case ACCESS_TYPE_NEXT:
		data.type = WIDGET_BUFFER_EVENT_ACCESS_HIGHLIGHT_NEXT;
		break;
	case ACCESS_TYPE_PREV:
		data.type = WIDGET_BUFFER_EVENT_ACCESS_HIGHLIGHT_PREV;
		break;
	case ACCESS_TYPE_OFF:
		data.type = WIDGET_BUFFER_EVENT_ACCESS_UNHIGHLIGHT;
		break;
	default:
		ErrPrint("Invalid type\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	} else {
		/* Event handler is not ready */
		(void)widget_provider_send_access_status(pkgname, id, WIDGET_ACCESS_STATUS_ERROR);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_access_action(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	int ret;
	widget_buffer_h info;
	struct widget_buffer_event_data data;

	ret = packet_get(packet, "ssdiii", &pkgname, &id, &data.timestamp, &data.info.access.x, &data.info.access.y, &data.info.access.mouse_type);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	switch (data.info.access.mouse_type) {
	case ACCESS_TYPE_UP:
		data.type = WIDGET_BUFFER_EVENT_ACCESS_ACTION_UP;
		break;
	case ACCESS_TYPE_DOWN:
		data.type = WIDGET_BUFFER_EVENT_ACCESS_ACTION_DOWN;
		break;
	default:
		ErrPrint("Invalid type\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	} else {
		/* Event handler is not ready */
		(void)widget_provider_send_access_status(pkgname, id, WIDGET_ACCESS_STATUS_ERROR);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_access_scroll(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	int ret;
	widget_buffer_h info;
	struct widget_buffer_event_data data;

	ret = packet_get(packet, "ssdiii", &pkgname, &id, &data.timestamp, &data.info.access.x, &data.info.access.y, &data.info.access.mouse_type);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	switch (data.info.access.mouse_type) {
	case ACCESS_TYPE_DOWN:
		data.type = WIDGET_BUFFER_EVENT_ACCESS_SCROLL_DOWN;
		break;
	case ACCESS_TYPE_MOVE:
		data.type = WIDGET_BUFFER_EVENT_ACCESS_SCROLL_MOVE;
		break;
	case ACCESS_TYPE_UP:
		data.type = WIDGET_BUFFER_EVENT_ACCESS_SCROLL_UP;
		break;
	default:
		ErrPrint("Invalid type\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	} else {
		/* Event handler is not ready */
		(void)widget_provider_send_access_status(pkgname, id, WIDGET_ACCESS_STATUS_ERROR);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_access_activate(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	int ret;
	widget_buffer_h info;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_ACCESS_ACTIVATE,
	};

	ret = packet_get(packet, "ssdiii", &pkgname, &id, &data.timestamp, &data.info.access.x, &data.info.access.y, &data.info.access.mouse_type);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	} else {
		/* Event handler is not ready */
		(void)widget_provider_send_access_status(pkgname, id, WIDGET_ACCESS_STATUS_ERROR);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_mouse_on_hold(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	double ratio_w;
	double ratio_h;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_ON_HOLD,
	};

	if (packet_get(packet, "ssdiiiddi", &pkgname, &id, &data.timestamp, &data.info.pointer.x, &data.info.pointer.y, (int *)&data.info.pointer.source, &ratio_w, &ratio_h, &data.info.pointer.device) != 9) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		if (ratio_w != 1.0f || ratio_h != 1.0f) {
			data.info.pointer.x *= ratio_w;
			data.info.pointer.y *= ratio_h;
		}
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_mouse_off_hold(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	double ratio_w;
	double ratio_h;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_OFF_HOLD,
	};

	if (packet_get(packet, "ssdiiiddi", &pkgname, &id, &data.timestamp, &data.info.pointer.x, &data.info.pointer.y, &data.info.pointer.source, &ratio_w, &ratio_h, &data.info.pointer.device) != 9) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		if (ratio_w != 1.0f || ratio_h != 1.0f) {
			data.info.pointer.x *= ratio_w;
			data.info.pointer.y *= ratio_h;
		}
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_mouse_on_scroll(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	double ratio_w;
	double ratio_h;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_ON_SCROLL,
	};

	if (packet_get(packet, "ssdiiiddi", &pkgname, &id, &data.timestamp, &data.info.pointer.x, &data.info.pointer.y, &data.info.pointer.source, &ratio_w, &ratio_h, &data.info.pointer.device) != 9) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		if (ratio_w != 1.0f || ratio_h != 1.0f) {
			data.info.pointer.x *= ratio_w;
			data.info.pointer.y *= ratio_h;
		}
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_mouse_off_scroll(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	double ratio_w;
	double ratio_h;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_OFF_SCROLL,
	};

	if (packet_get(packet, "ssdiiiddi", &pkgname, &id, &data.timestamp, &data.info.pointer.x, &data.info.pointer.y, &data.info.pointer.source, &ratio_w, &ratio_h, &data.info.pointer.device) != 9) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		if (ratio_w != 1.0f || ratio_h != 1.0f) {
			data.info.pointer.x *= ratio_w;
			data.info.pointer.y *= ratio_h;
		}
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_mouse_on_hold(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	double ratio_w;
	double ratio_h;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_ON_HOLD,
	};

	if (packet_get(packet, "ssdiiiddi", &pkgname, &id, &data.timestamp, &data.info.pointer.x, &data.info.pointer.y, &data.info.pointer.source, &ratio_w, &ratio_h, &data.info.pointer.device) != 9) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		if (ratio_w != 1.0f || ratio_h != 1.0f) {
			data.info.pointer.x *= ratio_w;
			data.info.pointer.y *= ratio_h;
		}
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_mouse_off_hold(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	double ratio_w;
	double ratio_h;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_OFF_HOLD,
	};

	if (packet_get(packet, "ssdiiiddi", &pkgname, &id, &data.timestamp, &data.info.pointer.x, &data.info.pointer.y, &data.info.pointer.source, &ratio_w, &ratio_h, &data.info.pointer.device) != 9) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		if (ratio_w != 1.0f || ratio_h != 1.0f) {
			data.info.pointer.x *= ratio_w;
			data.info.pointer.y *= ratio_h;
		}
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_mouse_on_scroll(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	double ratio_w;	
	double ratio_h;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_ON_SCROLL,
	};

	if (packet_get(packet, "ssdiiiddi", &pkgname, &id, &data.timestamp, &data.info.pointer.x, &data.info.pointer.y, (int *)&data.info.pointer.source, &ratio_w, &ratio_h, &data.info.pointer.device) != 9) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		if (ratio_w != 1.0f || ratio_h != 1.0f) {
			data.info.pointer.x *= ratio_w;
			data.info.pointer.y *= ratio_h;
		}
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_mouse_off_scroll(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	double ratio_w;
	double ratio_h;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_OFF_SCROLL,
	};

	if (packet_get(packet, "ssdiiiddi", &pkgname, &id, &data.timestamp, &data.info.pointer.x, &data.info.pointer.y, (int *)&data.info.pointer.source, &ratio_w, &ratio_h, &data.info.pointer.device) != 9) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		if (ratio_w != 1.0f || ratio_h != 1.0f) {
			data.info.pointer.x *= ratio_w;
			data.info.pointer.y *= ratio_h;
		}
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_access_value_change(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	int ret;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_ACCESS_VALUE_CHANGE,
	};

	ret = packet_get(packet, "ssdiii", &pkgname, &id, &data.timestamp, &data.info.access.x, &data.info.access.y, &data.info.access.mouse_type);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_access_mouse(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	int ret;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_ACCESS_MOUSE,
	};

	ret = packet_get(packet, "ssdiii", &pkgname, &id, &data.timestamp, &data.info.access.x, &data.info.access.y, &data.info.access.mouse_type);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_access_back(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	int ret;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_ACCESS_BACK,
	};

	ret = packet_get(packet, "ssdiii", &pkgname, &id, &data.timestamp, &data.info.access.x, &data.info.access.y, &data.info.access.mouse_type);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_access_over(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	int ret;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_ACCESS_OVER,
	};

	ret = packet_get(packet, "ssdiii", &pkgname, &id, &data.timestamp, &data.info.access.x, &data.info.access.y, &data.info.access.mouse_type);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_access_read(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	int ret;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_ACCESS_READ,
	};

	ret = packet_get(packet, "ssdiii", &pkgname, &id, &data.timestamp, &data.info.access.x, &data.info.access.y, &data.info.access.mouse_type);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_widget_access_enable(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	int ret;
	struct widget_buffer_event_data data;

	ret = packet_get(packet, "ssdiii", &pkgname, &id, &data.timestamp, &data.info.access.x, &data.info.access.y, &data.info.access.mouse_type);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	data.type = (data.info.access.mouse_type == 0) ? WIDGET_BUFFER_EVENT_ACCESS_DISABLE : WIDGET_BUFFER_EVENT_ACCESS_ENABLE;

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_access_value_change(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	int ret;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_ACCESS_VALUE_CHANGE,
	};

	ret = packet_get(packet, "ssdiii", &pkgname, &id, &data.timestamp, &data.info.access.x, &data.info.access.y, &data.info.access.mouse_type);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_access_mouse(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	int ret;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_ACCESS_MOUSE,
	};

	ret = packet_get(packet, "ssdiii", &pkgname, &id, &data.timestamp, &data.info.access.x, &data.info.access.y, &data.info.access.mouse_type);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_access_back(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	int ret;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_ACCESS_BACK,
	};

	ret = packet_get(packet, "ssdiii", &pkgname, &id, &data.timestamp, &data.info.access.x, &data.info.access.y, &data.info.access.mouse_type);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_access_over(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	int ret;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_ACCESS_OVER,
	};

	ret = packet_get(packet, "ssdiii", &pkgname, &id, &data.timestamp, &data.info.access.x, &data.info.access.y, &data.info.access.mouse_type);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_access_read(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	int ret;
	struct widget_buffer_event_data data = {
		.type = WIDGET_BUFFER_EVENT_ACCESS_READ,
	};

	ret = packet_get(packet, "ssdiii", &pkgname, &id, &data.timestamp, &data.info.access.x, &data.info.access.y, &data.info.access.mouse_type);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

struct packet *provider_buffer_gbar_access_enable(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_buffer_h info;
	int ret;
	struct widget_buffer_event_data data;

	ret = packet_get(packet, "ssdiii", &pkgname, &id, &data.timestamp, &data.info.access.x, &data.info.access.y, &data.info.access.mouse_type);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	data.type = (data.info.access.mouse_type == 0) ? WIDGET_BUFFER_EVENT_ACCESS_DISABLE : WIDGET_BUFFER_EVENT_ACCESS_ENABLE;

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (!info) {
		ErrPrint("Failed to find a buffer [%s:%s]\n", pkgname, id);
		goto out;
	}

	if (info->handler) {
		(void)info->handler(info, &data, info->data);
	}

out:
	return NULL;
}

EAPI widget_buffer_h widget_provider_buffer_find_buffer(widget_target_type_e type, const char *pkgname, const char *id)
{
	struct dlist *l;
	struct dlist *n;
	widget_buffer_h info;

	dlist_foreach_safe(s_info.buffer_list, l, n, info) {
		if (info->type != type) {
			continue;
		}

		if (!strcmp(info->pkgname, pkgname) && !strcmp(info->id, id)) {
			return info;
		}
	}

	return NULL;
}

EAPI int widget_provider_buffer_set_user_data(widget_buffer_h handle, void *data)
{
	if (!handle || (handle->state != BUFFER_INITIALIZED && handle->state != BUFFER_CREATED)) {
		DbgPrint("Buffer handler is not prepared yet: %p\n", handle);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	handle->user_data = data;
	return WIDGET_ERROR_NONE;
}

EAPI void *widget_provider_buffer_user_data(widget_buffer_h handle)
{
	if (!handle || (handle->state != BUFFER_INITIALIZED && handle->state != BUFFER_CREATED)) {
		DbgPrint("Buffer handler is not prepared yet: %p\n", handle);
		return NULL;
	}

	return handle->user_data;
}

EAPI widget_buffer_h widget_provider_buffer_create(widget_target_type_e type, const char *pkgname, const char *id, int auto_align, int (*widget_handler)(widget_buffer_h , widget_buffer_event_data_t, void *), void *data)
{
	widget_buffer_h info;

	if (!pkgname) {
		ErrPrint("Invalid parameter: pkgname is NIL\n");
		return NULL;
	}

	if (!id) {
		ErrPrint("Invalid ID: id is NIL\n");
		return NULL;
	}

	if (!widget_handler) {
		DbgPrint("Event handler is not speicified\n");
	}

	DbgPrint("acquire_buffer: [%s] %s, handler: %p\n", type == WIDGET_TYPE_WIDGET ? "WIDGET" : "GBAR", id, widget_handler);

	info = calloc(1, sizeof(*info));
	if (!info) {
		ErrPrint("calloc: %d\n", errno);
		return NULL;
	}

	info->pkgname = strdup(pkgname);
	if (!info->pkgname) {
		ErrPrint("strdup: %d\n", errno);
		free(info);
		return NULL;
	}

	info->id = strdup(id);
	if (!info->id) {
		ErrPrint("strdup: %d\n", errno);
		free(info->pkgname);
		free(info);
		return NULL;
	}

	info->handler = widget_handler;
	info->type = type;
	info->data = data;
	info->state = BUFFER_INITIALIZED;
	info->auto_align = auto_align;
	info->frame_skip = WIDGET_CONF_FRAME_SKIP;

	s_info.buffer_list = dlist_prepend(s_info.buffer_list, info);
	return info;
}

EAPI int widget_provider_buffer_extra_acquire(widget_buffer_h info, int idx, int width, int height, int pixel_size)
{
	if (!widget_conf_is_loaded()) {
		ErrPrint("Configuration is not loaded\n");
		return WIDGET_ERROR_DISABLED;
	}

	if (idx < 0 || width <= 0 || height <= 0 || pixel_size <= 0 || idx >= WIDGET_CONF_EXTRA_BUFFER_COUNT) {
		ErrPrint("Invalid size: %dx%d, %d\n", width, height, pixel_size);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!info->extra_buffer && WIDGET_CONF_EXTRA_BUFFER_COUNT) {
		info->extra_buffer = calloc(WIDGET_CONF_EXTRA_BUFFER_COUNT, sizeof(*info->extra_buffer));
		if (!info->extra_buffer) {
			ErrPrint("calloc: %d\n", errno);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}
	}

	info->extra_buffer[idx] = send_acquire_extra_request(info->type, info->pkgname, info->id, width, height, pixel_size, idx);
	if (!info->extra_buffer[idx]) {
		ErrPrint("Failed to acquire an extra info\n");
		return WIDGET_ERROR_FAULT;
	}

	return WIDGET_ERROR_NONE;
}

EAPI int widget_provider_buffer_acquire(widget_buffer_h info, int width, int height, int pixel_size)
{
	if (width <= 0 || height <= 0 || pixel_size <= 0) {
		ErrPrint("Invalid size: %dx%d, %d\n", width, height, pixel_size);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	DbgPrint("acquire_buffer: [%s] %s, %dx%d, size: %d, handler: %p\n",
			info->type == WIDGET_TYPE_WIDGET ? "WIDGET" : "GBAR", info->id,
			width, height, pixel_size, info->handler);

	info->fb = send_acquire_request(info->type, info->pkgname, info->id, width, height, pixel_size);
	if (!info->fb) {
		ErrPrint("Failed to acquire an info\n");
		return WIDGET_ERROR_FAULT;
	}

	info->width = width;
	info->height = height;
	info->pixel_size = pixel_size;
	info->state = BUFFER_CREATED;

	switch (fb_type(info->fb)) {
	case WIDGET_FB_TYPE_FILE:
	case WIDGET_FB_TYPE_SHM:
		info->lock_info = widget_service_create_lock(info->id, info->type, WIDGET_LOCK_WRITE);
		break;
	case WIDGET_FB_TYPE_PIXMAP:
	default:
		/* Do nothing */
		break;
	}

	return WIDGET_ERROR_NONE;
}

EAPI int widget_provider_buffer_resize(widget_buffer_h info, int w, int h)
{
	struct fb_info *fb;

	if (!info || info->state != BUFFER_CREATED) {
		DbgPrint("Buffer handler is not prepared yet: %p\n", info);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	fb = send_resize_request(info->type, info->pkgname, info->id, w, h);
	if (!fb) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	/*!
	 * \note
	 * Even if we destroy the buffer object,
	 * if the user references it, it will not be destroyed,
	 * it only can be destroyed when there is no reference exists.
	 */
	info->fb = fb;
	return WIDGET_ERROR_NONE;
}

EAPI void *widget_provider_buffer_ref(widget_buffer_h info)
{
	if (!info || info->state != BUFFER_CREATED) {
		DbgPrint("Buffer handler is not prepared yet: %p\n", info);
		return NULL;
	}

	return fb_acquire_buffer(info->fb);
}

EAPI int widget_provider_buffer_unref(void *ptr)
{
	if (!ptr) {
		ErrPrint("PTR is not valid\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return fb_release_buffer(ptr);
}

EAPI int widget_provider_buffer_extra_release(widget_buffer_h info, int idx)
{
	int ret;

	if (!widget_conf_is_loaded()) {
		ErrPrint("Configuration is not loaded\n");
		return WIDGET_ERROR_DISABLED;
	}

	if (!info || info->state != BUFFER_CREATED) {
		DbgPrint("Buffer handler is not prepared yet: %p\n", info);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (idx < 0 || idx >= WIDGET_CONF_EXTRA_BUFFER_COUNT || !info->extra_buffer) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	ret = send_release_extra_request(info->type, info->pkgname, info->id, idx);
	if (ret == WIDGET_ERROR_NONE) {
		info->extra_buffer[idx] = 0u;
	}

	return ret;
}

EAPI int widget_provider_buffer_release(widget_buffer_h info)
{
	int ret;

	if (!info || info->state != BUFFER_CREATED) {
		DbgPrint("Buffer handler is not prepared yet: %p\n", info);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	widget_service_destroy_lock(info->lock_info, 0);
	info->lock_info = NULL;

	ret = send_release_request(info->type, info->pkgname, info->id);
	if (ret < 0) {
		ErrPrint("Failed to send a release request [%d]\n", ret);
		/**
		 * @note
		 * But go ahead to destroy this buffer object
		 */
	}

	info->fb = NULL;
	info->width = 0;
	info->height = 0;
	info->pixel_size = 0;
	info->state = BUFFER_INITIALIZED;

	return WIDGET_ERROR_NONE;
}

EAPI int widget_provider_buffer_destroy(widget_buffer_h info)
{
	if (!info || info->state != BUFFER_INITIALIZED) {
		ErrPrint("Buffer handler is %p (%x)\n", info, info ? info->state : 0x0);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	dlist_remove_data(s_info.buffer_list, info);

	info->state = BUFFER_DESTROYED;
	free(info->pkgname);
	free(info->id);
	free(info);

	return WIDGET_ERROR_NONE;
}

EAPI int widget_provider_buffer_sync(widget_buffer_h info)
{
	int ret;
	if (!info || info->state != BUFFER_CREATED) {
		DbgPrint("Buffer handler is not prepared yet: %p\n", info);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (fb_type(info->fb) == WIDGET_FB_TYPE_FILE) {
		(void)widget_service_acquire_lock(info->lock_info);
		ret = fb_sync(info->fb);
		(void)widget_service_release_lock(info->lock_info);
	} else {
		ret = fb_sync(info->fb);
	}
	return ret;
}

EAPI widget_target_type_e widget_provider_buffer_type(widget_buffer_h info)
{
	if (!info || (info->state != BUFFER_CREATED && info->state != BUFFER_INITIALIZED)) {
		ErrPrint("Buffer handler is not prepared yet (%p)\n", info);
		return WIDGET_TYPE_ERROR;
	}

	return info->type;
}

EAPI const char *widget_provider_buffer_pkgname(widget_buffer_h info)
{
	if (!info || (info->state != BUFFER_INITIALIZED && info->state != BUFFER_CREATED)) {
		ErrPrint("Buffer handler is not prepared yet: %p\n", info);
		return NULL;
	}

	return info->pkgname;
}

EAPI const char *widget_provider_buffer_id(widget_buffer_h info)
{
	if (!info || (info->state != BUFFER_CREATED && info->state != BUFFER_INITIALIZED)) {
		ErrPrint("Buffer handler is not prepared yet: %p\n", info);
		return NULL;
	}

	return info->id;
}

EAPI int widget_provider_buffer_get_size(widget_buffer_h info, int *w, int *h, int *pixel_size)
{
	if (!info || info->state != BUFFER_CREATED) {
		ErrPrint("Buffer handler is not prepared yet: %p\n", info);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (w) {
		*w = info->width;
	}

	if (h) {
		*h = info->height;
	}

	if (pixel_size) {
		*pixel_size = info->pixel_size;
	}

	return WIDGET_ERROR_NONE;
}

EAPI const char *widget_provider_buffer_uri(widget_buffer_h info)
{
	if (!info || info->state != BUFFER_CREATED) {
		ErrPrint("Buffer handler is not prepared yet: %p\n", info);
		return NULL;
	}

	return fb_id(info->fb);
}

EAPI unsigned int widget_provider_buffer_extra_resource_id(widget_buffer_h info, int idx)
{
	if (!info || info->state != BUFFER_CREATED) {
		ErrPrint("Buffer handler is not prepared yet: %p\n", info);
		return 0u;
	}

	if (!widget_conf_is_loaded()) {
		ErrPrint("Configuration is not loaded\n");
		return 0u;
	}

	if (!info->extra_buffer || idx < 0 || idx >= WIDGET_CONF_EXTRA_BUFFER_COUNT) {
		ErrPrint("Invalid parameter\n");
		return 0u;
	}

	return info->extra_buffer[idx];
}

EAPI unsigned int widget_provider_buffer_resource_id(widget_buffer_h info)
{
	const char *id;
	unsigned int pixmap;
	int size;

	if (!info || info->state != BUFFER_CREATED) {
		ErrPrint("Buffer handler is not prepared yet: %p\n", info);
		return 0u;
	}

	id = fb_id(info->fb);
	if (!id) {
		return 0u;
	}

	if (sscanf(id, SCHEMA_PIXMAP "%u:%d", &pixmap, &size) != 2) {
		ErrPrint("Invalid ID: %s\n", id);
		return 0u;
	}

	if (info->pixel_size != size) {
		DbgPrint("Pixel size is mismatched: %d <> %d\n", info->pixel_size, size);
	}

	return pixmap;
}

/**
 * @note
 * This API only used for PIXMAP.
 * Because the address of canvas can be get by Evas layer easily except PIXMAP based one.
 * So if a developer wants get the address of canvas which is created by pixmap,
 * This API will help the developer to get the address of pixmap.
 *
 * If there is already created gem buffer, this will copy its contents
 * or will try to create gem buffer if possible.
 * If the system doesn't support GEM buffer, this will try to get buffer
 * using XShm API.
 */
EAPI void *widget_provider_buffer_dump_frame(widget_buffer_h info, int idx)
{
	unsigned int pixmap;
	void *buffer = NULL;

	if (!info) {
		return NULL;
	}

	if (idx == WIDGET_PRIMARY_BUFFER) {
		if (fb_has_gem(info->fb)) {
			void *addr;

			addr = fb_acquire_gem(info->fb);
			if (addr) {
				buffer = malloc(info->width * info->height * info->pixel_size);
				if (buffer) {
					memcpy(buffer, addr, info->width * info->height * info->pixel_size);
				}
				fb_release_gem(info->fb);
			}
		} else {
			pixmap = widget_provider_buffer_resource_id(info);
			if (pixmap) {
				buffer = fb_dump_frame(pixmap, info->width, info->height, info->pixel_size);
			}
		}
	} else {
		/**
		 * @note
		 * In this case, the provider buffer doesn't create the gem handler for this.
		 * So we have to use fb_dump_frame.
		 */
		pixmap = widget_provider_buffer_extra_resource_id(info, idx);
		if (pixmap) {
			buffer = fb_dump_frame(pixmap, info->width, info->height, info->pixel_size);
		}
	}

	return buffer;
}

EAPI int widget_provider_buffer_is_support_hw(widget_buffer_h info)
{
	if (!info || info->state != BUFFER_CREATED) {
		DbgPrint("Buffer handler is not prepared yet: %p\n", info);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return fb_support_gem(info->fb);
}

EAPI int widget_provider_buffer_create_hw(widget_buffer_h info)
{
	if (!info || info->state != BUFFER_CREATED) {
		DbgPrint("Buffer handler is not prepared yet: %p\n", info);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!fb_support_gem(info->fb)) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return fb_create_gem(info->fb, info->auto_align);
}

EAPI int widget_provider_buffer_destroy_hw(widget_buffer_h info)
{
	if (!info || info->state != BUFFER_CREATED || !fb_has_gem(info->fb)) {
		DbgPrint("Buffer handler is not prepared yet: %p\n", info);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return fb_destroy_gem(info->fb);
}

EAPI void *widget_provider_buffer_hw_addr(widget_buffer_h info)
{
	void *addr;

	if (!info || info->state != BUFFER_CREATED || !fb_has_gem(info->fb)) {
		DbgPrint("Buffer handler is not prepared yet: %p\n", info);
		return NULL;
	}

	addr = fb_acquire_gem(info->fb);
	fb_release_gem(info->fb);

	return addr;
}

EAPI int widget_provider_buffer_pre_render(widget_buffer_h info)
{
	int ret = WIDGET_ERROR_NONE;

	if (!info) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (fb_has_gem(info->fb)) {
		ret = fb_acquire_gem(info->fb) ? WIDGET_ERROR_NONE : WIDGET_ERROR_FAULT;
	} else if (fb_type(info->fb) == WIDGET_FB_TYPE_SHM) {
		ret = widget_service_acquire_lock(info->lock_info);
	} else {
		ErrPrint("Unable to acquire gem (%s)\n", info ? (info->fb ? info->fb->id : "info->fb==null") : "info==null");
	}

	return ret;
}

static inline void feed_frame_skip_cleared_event(widget_buffer_h info)
{
	struct widget_buffer_event_data ev_info;

	memset(&ev_info, 0, sizeof(ev_info));

	ev_info.type = WIDGET_BUFFER_EVENT_FRAME_SKIP_CLEARED;

	if (info->handler) {
		(void)info->handler(info, &ev_info, info->data);
	}
}

EAPI int widget_provider_buffer_post_render(widget_buffer_h info)
{
	int ret = WIDGET_ERROR_NONE;

	if (!info) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (fb_has_gem(info->fb)) {
		ret = fb_release_gem(info->fb);
	} else if (fb_type(info->fb) == WIDGET_FB_TYPE_SHM) {
		ret = widget_service_release_lock(info->lock_info);
	} else {
		ErrPrint("Unable to release gem (%s)\n", info ? (info->fb ? info->fb->id : "info->fb==null") : "info==null");
	}

	if (info->frame_skip > 0) {
		info->frame_skip--;
		if (info->frame_skip == 0) {
			feed_frame_skip_cleared_event(info);
		}
	}

	return ret;
}

EAPI int widget_provider_buffer_stride(widget_buffer_h info)
{
	if (!info) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return fb_stride(info->fb);
}

EAPI int widget_provider_buffer_frame_skip(widget_buffer_h info)
{
	if (!info) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return info->frame_skip;
}

EAPI int widget_provider_buffer_clear_frame_skip(widget_buffer_h info)
{
	int old;

	if (!info) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	old = info->frame_skip;
	DbgPrint("Clear frame skip: %d\n", info->frame_skip);
	info->frame_skip = 0;

	if (old > 0) {
		feed_frame_skip_cleared_event(info);
	}

	return WIDGET_ERROR_NONE;
}

/* End of a file */
