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

#include <Eina.h>

#include <dlog.h>
#if defined(HAVE_LIVEBOX)
#include <widget_errno.h>
#else
#include "lite-errno.h"
#endif
#include <packet.h>

#include <Eina.h>
#include <sys/smack.h>

#include <security-server.h>
#include <shortcut.h>

#include "service_common.h"
#include "debug.h"
#include "util.h"
#include "conf.h"

static struct info {
	Eina_List *context_list;
	struct service_context *svc_ctx;
} s_info = {
	.context_list = NULL, /*!< \WARN: This is only used for SERVICE THREAD */
	.svc_ctx = NULL, /*!< \WARN: This is only used for MAIN THREAD */
};

struct context {
	struct tcb *tcb;
	double seq;
};

/*!
 * SERVICE THREAD
 */
static inline int put_reply_context(struct tcb *tcb, double seq)
{
	struct context *ctx;

	ctx = malloc(sizeof(*ctx));
	if (!ctx) {
		ErrPrint("malloc: %d\n", errno);
		return -ENOMEM;
	}

	ctx->tcb = tcb;
	ctx->seq = seq; /* Could we this sequence value is uniq? */

	s_info.context_list = eina_list_append(s_info.context_list, ctx);
	return 0;
}

/*!
 * SERVICE THREAD
 */
static inline struct tcb *get_reply_context(double seq)
{
	Eina_List *l;
	Eina_List *n;
	struct context *ctx;
	struct tcb *tcb;

	tcb = NULL;
	EINA_LIST_FOREACH_SAFE(s_info.context_list, l, n, ctx) {
		if (ctx->seq != seq) {
			continue;
		}

		s_info.context_list = eina_list_remove(s_info.context_list, ctx);
		tcb = ctx->tcb;
		DbgFree(ctx);
		break;
	}

	return tcb;
}

static void send_reply_packet(struct tcb *tcb, struct packet *packet, int ret)
{
	struct packet *reply_packet;

	reply_packet = packet_create_reply(packet, "i", ret);
	if (!reply_packet) {
		ErrPrint("Failed to create a packet\n");
		return;
	}

	if (service_common_unicast_packet(tcb, reply_packet) < 0) {
		ErrPrint("Unable to send reply packet\n");
	}

	packet_destroy(reply_packet);
}

/*!
 * SERVICE THREAD
 */
static int service_thread_main(struct tcb *tcb, struct packet *packet, void *data)
{
	const char *command;

	if (!packet) {
		DbgPrint("TCB: %p is terminated (NIL packet)\n", tcb);
		return 0;
	}

	command = packet_command(packet);
	if (!command) {
		ErrPrint("Invalid command\n");
		return -EINVAL;
	}

	switch (packet_type(packet)) {
	case PACKET_REQ:

		/* Need to send reply packet */
		DbgPrint("%p REQ: Command: [%s]\n", tcb, command);
		if (!strcmp(command, "add_shortcut_widget") || !strcmp(command, "rm_shortcut_widget")) {
			int ret;
			ret = security_server_check_privilege_by_sockfd(tcb_fd(tcb), "data-provider-master::shortcut.livebox", "w");
			if (ret == SECURITY_SERVER_API_ERROR_ACCESS_DENIED) {
				ErrPrint("SMACK:Access denied\n");
				send_reply_packet(tcb, packet, SHORTCUT_ERROR_PERMISSION_DENIED);
				break;
			}
		} else if (!strcmp(command, "add_shortcut") || !strcmp(command, "rm_shortcut")) {
			int ret;
			ret = security_server_check_privilege_by_sockfd(tcb_fd(tcb), "data-provider-master::shortcut.shortcut", "w");
			if (ret == SECURITY_SERVER_API_ERROR_ACCESS_DENIED) {
				ErrPrint("SMACK:Access denied\n");
				send_reply_packet(tcb, packet, SHORTCUT_ERROR_PERMISSION_DENIED);
				break;
			}
		}

		if (service_common_multicast_packet(tcb, packet, TCB_CLIENT_TYPE_SERVICE) < 0) {
			ErrPrint("Unable to send service request packet\n");
		} else {
			(void)put_reply_context(tcb, packet_seq(packet));
		}
		break;
	case PACKET_REQ_NOACK:
		/* Doesn't need to send reply packet */
		DbgPrint("%p REQ_NOACK: Command: [%s]\n", tcb, command);
		if (!strcmp(command, "service_register")) {
			tcb_client_type_set(tcb, TCB_CLIENT_TYPE_SERVICE);
			break;
		}

		if (service_common_multicast_packet(tcb, packet, TCB_CLIENT_TYPE_SERVICE) < 0) {
			ErrPrint("Unable to send service reuqest packet\n");
		}
		break;
	case PACKET_ACK:
		/* Okay, client(or app) send a reply packet to us. */
		DbgPrint("%p ACK: Command: [%s]\n", tcb, command);
		tcb = get_reply_context(packet_seq(packet));
		if (!tcb) {
			ErrPrint("There is no proper context\n");
			break;
		}

		if (tcb_is_valid(s_info.svc_ctx, tcb) < 0) {
			ErrPrint("TCB is not valid (already disconnected?)\n");
			break;
		}

		if (service_common_unicast_packet(tcb, packet) < 0) {
			ErrPrint("Unable to send reply packet\n");
		}
		break;
	default:
		ErrPrint("Packet type is not valid[%s]\n", command);
		return -EINVAL;
	}

	/*!
	 * return value has no meanning,
	 * it will be printed by dlogutil.
	 */
	return 0;
}


/*!
 * MAIN THREAD
 * Do not try to do anyother operation in these functions
 */

HAPI int shortcut_service_init(void)
{
	if (s_info.svc_ctx) {
		ErrPrint("Already initialized\n");
		return WIDGET_ERROR_ALREADY_STARTED;
	}

	s_info.svc_ctx = service_common_create("sdlocal://"SHORTCUT_SOCKET, SHORTCUT_SMACK_LABEL, service_thread_main, NULL);
	if (!s_info.svc_ctx) {
		ErrPrint("Unable to activate service thread\n");
		return WIDGET_ERROR_FAULT;
	}

	DbgPrint("Successfully initiated\n");
	return WIDGET_ERROR_NONE;
}

HAPI int shortcut_service_fini(void)
{
	if (!s_info.svc_ctx) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	service_common_destroy(s_info.svc_ctx);
	s_info.svc_ctx = NULL;
	DbgPrint("Successfully Finalized\n");
	return WIDGET_ERROR_NONE;
}

/* End of a file */
