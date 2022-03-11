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
#include <gio/gio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <dlog.h>

#include <packet.h>
#include <com-core_packet.h>
#include <widget_errno.h>
#include <widget_service.h>
#include <widget_service_internal.h>
#include <widget_buffer.h>

#include "debug.h"
#include "dlist.h"
#include "widget_viewer.h"
#include "widget_viewer_internal.h"
#include "master_rpc.h"
#include "client.h"
#include "util.h"

#define DEFAULT_TTL 10
#define REQUEST_DELAY 10

struct command {
	int ttl;
	struct packet *packet;
	widget_h handler;
	void (*ret_cb)(widget_h handler, const struct packet *result, void *data);
	void *data;
	enum {
		TYPE_ACK,
		TYPE_NOACK
	} type;
};

int errno;

static struct {
	guint cmd_timer;
	struct dlist *cmd_list;
} s_info = {
	.cmd_timer = 0,
	.cmd_list = NULL,
};

static int done_cb(pid_t pid, int handle, const struct packet *packet, void *data);

static inline struct command *pop_command(void)
{
	struct dlist *l;
	struct command *command;

	l = dlist_nth(s_info.cmd_list, 0);
	if (!l) {
		return NULL;
	}

	command = dlist_data(l);
	s_info.cmd_list = dlist_remove(s_info.cmd_list, l);
	return command;
}

static inline struct command *create_command(widget_h handler, struct packet *packet)
{
	struct command *command;

	command = malloc(sizeof(*command));
	if (!command) {
		ErrPrint("Failed to allocate mem for command\n");
		return NULL;
	}

	command->handler = _widget_ref(handler);
	command->packet = packet_ref(packet);
	return command;
}

static inline void destroy_command(struct command *command)
{
	packet_unref(command->packet);
	_widget_unref(command->handler, 1);
	free(command);
}

static gboolean cmd_consumer(gpointer user_data)
{
	struct command *command;

	command = pop_command();
	if (!command) {
		s_info.cmd_timer = 0;
		return FALSE;
	}

	/*!
	 * \NOTE:
	 * Item will be deleted in the "done_cb"
	 *
	 * item->param be release by the g_dbus_proxy_call
	 * so to use it again from the done_cb function,
	 * increate the reference counter of the item->param
	 */
	if (command->type == TYPE_NOACK) {
		if (com_core_packet_send_only(client_fd(), command->packet) < 0) {
			ErrPrint("Failed to send a packet to master\n");
		}

		destroy_command(command);
	} else {
		if (com_core_packet_async_send(client_fd(), command->packet, 0u, done_cb, command) < 0) {
			ErrPrint("Failed to send a packet to master\n");
			if (command->ret_cb) {
				command->ret_cb(command->handler, NULL, command->data);
			}
			destroy_command(command);
		}
	}
	return TRUE;
}

static inline void prepend_command(struct command *command)
{
	s_info.cmd_list = dlist_prepend(s_info.cmd_list, command);
	master_rpc_check_and_fire_consumer();
}

void master_rpc_check_and_fire_consumer(void)
{
	if (!s_info.cmd_list || s_info.cmd_timer || client_fd() < 0) {
		return;
	}

	s_info.cmd_timer = g_timeout_add(REQUEST_DELAY, cmd_consumer, NULL);
	if (!s_info.cmd_timer) {
		ErrPrint("Failed to add timer\n");
	}
}

static int done_cb(pid_t pid, int handle, const struct packet *packet, void *data)
{
	struct command *command;
	int ret;

	command = data;

	if (!packet) {
		/*! \NOTE:
		 * Release resource even if
		 * we failed to finish the method call
		 */
		command->ttl--;
		if (command->ttl > 0) {
			prepend_command(command);
			return 0;
		}

		goto out;
	}

	if (packet_get(packet, "i", &ret) != 1) {
		ErrPrint("Invalid result packet\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
	}

out:
	if (command->ret_cb) {
		command->ret_cb(command->handler, packet, command->data);
	}

	destroy_command(command);
	return 0;
}

static inline void push_command(struct command *command)
{
	s_info.cmd_list = dlist_append(s_info.cmd_list, command);
	master_rpc_check_and_fire_consumer();
}

/*!
 * \note
 * "handler" could be NULL
 */
int master_rpc_async_request(widget_h handler, struct packet *packet, int urgent, void (*ret_cb)(widget_h handler, const struct packet *result, void *data), void *data)
{
	struct command *command;

	command = create_command(handler, packet);
	if (!command) {
		ErrPrint("Failed to create a command\n");
		packet_unref(packet);
		return WIDGET_ERROR_FAULT;
	}

	command->ret_cb = ret_cb;
	command->data = data;
	command->ttl = DEFAULT_TTL;
	command->type = TYPE_ACK;

	if (urgent) {
		prepend_command(command);
	} else {
		push_command(command);
	}

	packet_unref(packet);
	return WIDGET_ERROR_NONE;
}

int master_rpc_request_only(widget_h handler, struct packet *packet)
{
	struct command *command;

	command = create_command(handler, packet);
	if (!command) {
		ErrPrint("Failed to create a command\n");
		packet_unref(packet);
		return WIDGET_ERROR_FAULT;
	}

	command->ret_cb = NULL;
	command->data = NULL;
	command->ttl = 0;
	command->type = TYPE_NOACK;

	push_command(command);
	packet_unref(packet);
	return WIDGET_ERROR_NONE;
}

int master_rpc_clear_fault_package(const char *pkgname)
{
	struct dlist *l;
	struct dlist *n;
	struct command *command;

	if (!pkgname) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	dlist_foreach_safe(s_info.cmd_list, l, n, command) {
		if (!command->handler) {
			continue;
		}

		if (!strcmp(command->handler->common->pkgname, pkgname)) {
			s_info.cmd_list = dlist_remove(s_info.cmd_list, l);
			if (command->ret_cb) {
				command->ret_cb(command->handler, NULL, command->data);
			}

			destroy_command(command);
		}
	}

	return 0;
}

int master_rpc_clear_all_request(void)
{
	struct command *command;
	struct dlist *l;
	struct dlist *n;

	dlist_foreach_safe(s_info.cmd_list, l, n, command) {
		s_info.cmd_list = dlist_remove(s_info.cmd_list, l);

		if (command->ret_cb) {
			command->ret_cb(command->handler, NULL, command->data);
		}

		destroy_command(command);
	}

	return 0;
}

int master_rpc_sync_request(struct packet *packet)
{
	struct packet *result;
	int ret;

	result = com_core_packet_oneshot_send(client_addr(), packet, 0.0f);
	if (result) {
		if (packet_get(result, "i", &ret) != 1) {
			ErrPrint("Invalid result packet\n");
			ret = WIDGET_ERROR_INVALID_PARAMETER;
		}

		packet_unref(result);
	} else {
		ErrPrint("Failed to send a sync request\n");
		ret = WIDGET_ERROR_FAULT;
	}

	packet_unref(packet);
	return ret;
}

/* End of a file */
