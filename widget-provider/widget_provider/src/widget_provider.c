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
#include <sys/shm.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include <com-core.h>
#include <packet.h>
#include <com-core_packet.h>

#include <dlog.h>
#include <widget_errno.h>
#include <widget_service.h> /* WIDGET_ACCESS_STATUS_XXXX */
#include <widget_service_internal.h>
#include <widget_cmd_list.h>
#include <widget_conf.h>
#include <widget_util.h>
#include <widget_buffer.h>

#include "widget_provider.h"
#include "widget_provider_buffer.h"
#include "provider_buffer_internal.h"
#include "dlist.h"
#include "util.h"
#include "debug.h"
#include "fb.h"
#include "event.h"

#define EAPI __attribute__((visibility("default")))
#define SW_ACCEL "use-sw"

static struct info {
	int closing_fd;
	int fd;
	char *name;
	char *abi;
	char *accel;
	int secured;
	struct widget_event_table table;
	void *data;
	int prevent_overwrite;
} s_info = {
	.closing_fd = 0,
	.fd = -1,
	.name = NULL,
	.abi = NULL,
	.accel = NULL,
	.data = NULL,
	.prevent_overwrite = 0,
	.secured = 0,
};

#define EAPI __attribute__((visibility("default")))

static double current_time_get(double timestamp)
{
	double ret;

	if (WIDGET_CONF_USE_GETTIMEOFDAY) {
		struct timeval tv;
		if (gettimeofday(&tv, NULL) < 0) {
			ErrPrint("gettimeofday: %d\n", errno);
			ret = timestamp;
		} else {
			ret = (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0f);
		}
	} else {
		ret = timestamp;
	}

	return ret;
}

/* pkgname, id, signal_name, source, sx, sy, ex, ey, x, y, down, ret */
static struct packet *master_script(pid_t pid, int handle, const struct packet *packet)
{
	struct widget_event_arg arg;
	int ret;

	ret = packet_get(packet, "ssssddddddi",
			&arg.pkgname, &arg.id,
			&arg.info.content_event.signal_name, &arg.info.content_event.source,
			&(arg.info.content_event.info.part.sx), &(arg.info.content_event.info.part.sy),
			&(arg.info.content_event.info.part.ex), &(arg.info.content_event.info.part.ey),
			&(arg.info.content_event.info.pointer.x), &(arg.info.content_event.info.pointer.y),
			&(arg.info.content_event.info.pointer.down));
	if (ret != 11) {
		ErrPrint("Parameter is not valid\n");
		goto errout;
	}

	arg.type = WIDGET_EVENT_CONTENT_EVENT;

	if (s_info.table.content_event) {
		(void)s_info.table.content_event(&arg, s_info.data);
	}

errout:
	return NULL;
}

/* pkgname, id, event, timestamp, x, y, ret */
static struct packet *master_clicked(pid_t pid, int handle, const struct packet *packet)
{
	int ret;
	struct widget_event_arg arg;

	ret = packet_get(packet, "sssddd",
			&arg.pkgname, &arg.id,
			&arg.info.clicked.event,
			&arg.info.clicked.timestamp,
			&arg.info.clicked.x, &arg.info.clicked.y);
	if (ret != 6) {
		ErrPrint("Parameter is not valid\n");
		goto errout;
	}

	arg.type = WIDGET_EVENT_CLICKED;

	DbgPrint("Clicked: %s\n", arg.id);
	if (s_info.table.clicked) {
		(void)s_info.table.clicked(&arg, s_info.data);
	}

errout:
	return NULL;
}

/* pkgname, id, signal_name, source, x, y, ex, ey, ret */
static struct packet *master_text_signal(pid_t pid, int handle, const struct packet *packet)
{
	struct widget_event_arg arg;
	int ret;

	ret = packet_get(packet, "ssssdddd",
			&arg.pkgname, &arg.id,
			&arg.info.text_signal.signal_name, &arg.info.text_signal.source,
			&(arg.info.text_signal.info.part.sx), &(arg.info.text_signal.info.part.sy),
			&(arg.info.text_signal.info.part.ex), &(arg.info.text_signal.info.part.ey));

	if (ret != 8) {
		ErrPrint("Parameter is not valid\n");
		goto errout;
	}

	arg.info.text_signal.info.pointer.x = 0.0f;
	arg.info.text_signal.info.pointer.y = 0.0f;
	arg.info.text_signal.info.pointer.down = 0;
	arg.type = WIDGET_EVENT_TEXT_SIGNAL;

	if (s_info.table.text_signal) {
		(void)s_info.table.text_signal(&arg, s_info.data);
	}

errout:
	return NULL;
}

/* pkgname, id, ret */
static struct packet *master_delete(pid_t pid, int handle, const struct packet *packet)
{
	struct packet *result;
	struct widget_event_arg arg;
	int ret;
	int type;

	ret = packet_get(packet, "ssi", &arg.pkgname, &arg.id, &type);
	if (ret != 3) {
		ErrPrint("Parameter is not valid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto errout;
	}

	arg.type = WIDGET_EVENT_DELETE;
	arg.info.widget_destroy.type = type;
	DbgPrint("WIDGET Deleted, reason(%d)\n", type);

	if (s_info.table.widget_destroy) {
		ret = s_info.table.widget_destroy(&arg, s_info.data);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}

errout:
	result = packet_create_reply(packet, "i", ret);
	return result;
}

/* pkgname, id, w, h, ret */
static struct packet *master_resize(pid_t pid, int handle, const struct packet *packet)
{
	struct widget_event_arg arg;
	int ret;
	struct packet *result;

	ret = packet_get(packet, "ssii", &arg.pkgname, &arg.id, &arg.info.resize.w, &arg.info.resize.h);
	if (ret != 4) {
		ErrPrint("Parameter is not valid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto errout;
	}

	arg.type = WIDGET_EVENT_RESIZE;

	if (s_info.table.resize) {
		ret = s_info.table.resize(&arg, s_info.data);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}

errout:
	result = packet_create_reply(packet, "i", ret);
	return result;
}

/* pkgname, id, content, timeout, has_Script, period, cluster, category, pinup, width, height, abi, ret */
static struct packet *master_renew(pid_t pid, int handle, const struct packet *packet)
{
	struct widget_event_arg arg;
	int ret;
	struct packet *result;
	char *content;
	char *title;

	arg.info.widget_recreate.out_title = NULL;
	arg.info.widget_recreate.out_content = NULL;
	arg.info.widget_recreate.out_is_pinned_up = 0;

	ret = packet_get(packet, "sssiidssiisiisi", &arg.pkgname, &arg.id,
			&arg.info.widget_recreate.content,
			&arg.info.widget_recreate.timeout,
			&arg.info.widget_recreate.has_script,
			&arg.info.widget_recreate.period,
			&arg.info.widget_recreate.cluster, &arg.info.widget_recreate.category,
			&arg.info.widget_recreate.width, &arg.info.widget_recreate.height,
			&arg.info.widget_recreate.abi,
			&arg.info.widget_recreate.hold_scroll,
			&arg.info.widget_recreate.active_update,
			&arg.info.widget_recreate.direct_addr,
			&arg.info.widget_recreate.degree);
	if (ret != 15) {
		ErrPrint("Parameter is not valid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto errout;
	}

	arg.type = WIDGET_EVENT_RENEW;

	if (s_info.table.widget_recreate) {
		ret = s_info.table.widget_recreate(&arg, s_info.data);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}

errout:
	if (arg.info.widget_recreate.out_content) {
		content = arg.info.widget_recreate.out_content;
	} else {
		content = "";
	}

	if (arg.info.widget_recreate.out_title) {
		title = arg.info.widget_recreate.out_title;
	} else {
		title = "";
	}

	result = packet_create_reply(packet, "issi", ret, content, title, arg.info.widget_recreate.out_is_pinned_up);
	/*!
	 * \note
	 * Release.
	 */
	free(arg.info.widget_recreate.out_title);
	free(arg.info.widget_recreate.out_content);
	arg.info.widget_recreate.out_title = NULL;
	arg.info.widget_recreate.out_content = NULL;
	return result;
}

/* pkgname, id, content, timeout, has_script, period, cluster, category, pinup, skip_need_to_create, abi, result, width, height, priority */
static struct packet *master_new(pid_t pid, int handle, const struct packet *packet)
{
	struct widget_event_arg arg;
	int ret;
	struct packet *result;
	int width = 0;
	int height = 0;
	double priority = 0.0f;
	char *content;
	char *title;

	arg.info.widget_create.out_content = NULL;
	arg.info.widget_create.out_title = NULL;
	arg.info.widget_create.out_is_pinned_up = 0;

	ret = packet_get(packet, "sssiidssisiisi", &arg.pkgname, &arg.id,
			&arg.info.widget_create.content,
			&arg.info.widget_create.timeout,
			&arg.info.widget_create.has_script,
			&arg.info.widget_create.period,
			&arg.info.widget_create.cluster, &arg.info.widget_create.category,
			&arg.info.widget_create.skip_need_to_create,
			&arg.info.widget_create.abi,
			&arg.info.widget_create.width,
			&arg.info.widget_create.height,
			&arg.info.widget_create.direct_addr,
			&arg.info.widget_create.degree);
	if (ret != 14) {
		ErrPrint("Parameter is not valid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto errout;
	}

	arg.type = WIDGET_EVENT_NEW;

	if (s_info.table.widget_create) {
		ret = s_info.table.widget_create(&arg, &width, &height, &priority, s_info.data);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}

errout:
	if (arg.info.widget_create.out_content) {
		content = arg.info.widget_create.out_content;
	} else {
		content = "";
	}

	if (arg.info.widget_create.out_title) {
		title = arg.info.widget_create.out_title;
	} else {
		title = "";
	}

	result = packet_create_reply(packet, "iiidssi", ret, width, height, priority, content, title, arg.info.widget_create.out_is_pinned_up);

	/*!
	 * Note:
	 * Skip checking the address of out_content, out_title
	 */
	free(arg.info.widget_create.out_content);
	free(arg.info.widget_create.out_title);
	return result;
}

/* pkgname, id, period, ret */
static struct packet *master_set_period(pid_t pid, int handle, const struct packet *packet)
{
	struct widget_event_arg arg;
	int ret;
	struct packet *result;

	ret = packet_get(packet, "ssd", &arg.pkgname, &arg.id, &arg.info.set_period.period);
	if (ret != 3) {
		ErrPrint("Parameter is not valid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto errout;
	}

	arg.type = WIDGET_EVENT_SET_PERIOD;

	if (s_info.table.set_period) {
		ret = s_info.table.set_period(&arg, s_info.data);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}

errout:
	result = packet_create_reply(packet, "i", ret);
	return result;
}

/* pkgname, id, cluster, category, ret */
static struct packet *master_change_group(pid_t pid, int handle, const struct packet *packet)
{
	struct widget_event_arg arg;
	int ret;
	struct packet *result;

	ret = packet_get(packet, "ssss", &arg.pkgname, &arg.id,
			&arg.info.change_group.cluster, &arg.info.change_group.category);
	if (ret != 4) {
		ErrPrint("Parameter is not valid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto errout;
	}

	arg.type = WIDGET_EVENT_CHANGE_GROUP;

	if (s_info.table.change_group) {
		ret = s_info.table.change_group(&arg, s_info.data);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}

errout:
	result = packet_create_reply(packet, "i", ret);
	return result;
}

/* pkgname, id, pinup, ret */
static struct packet *master_pinup(pid_t pid, int handle, const struct packet *packet)
{
	struct widget_event_arg arg;
	int ret;
	struct packet *result;
	const char *content;

	ret = packet_get(packet, "ssi", &arg.pkgname, &arg.id, &arg.info.pinup.state);
	if (ret != 3) {
		ErrPrint("Parameter is not valid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto errout;
	}

	arg.type = WIDGET_EVENT_PINUP;

	if (s_info.table.pinup) {
		ret = s_info.table.pinup(&arg, s_info.data);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}

errout:
	content = "default";
	if (ret == 0 && arg.info.pinup.content_info) {
		content = arg.info.pinup.content_info;
	}

	result = packet_create_reply(packet, "is", ret, content);
	if (ret == 0) {
		free(arg.info.pinup.content_info);
	}
	return result;
}

/* pkgname, id, cluster, category, content, ret */
static struct packet *master_update_content(pid_t pid, int handle, const struct packet *packet)
{
	struct widget_event_arg arg;
	int ret;

	ret = packet_get(packet, "sssssi", &arg.pkgname, &arg.id, &arg.info.update_content.cluster, &arg.info.update_content.category, &arg.info.update_content.content, &arg.info.update_content.force);
	if (ret != 6) {
		ErrPrint("Parameter is not valid\n");
		goto errout;
	}

	arg.type = WIDGET_EVENT_UPDATE_CONTENT;

	if (s_info.table.update_content) {
		(void)s_info.table.update_content(&arg, s_info.data);
	}

errout:
	return NULL;
}

static struct packet *master_widget_pause(pid_t pid, int handle, const struct packet *packet)
{
	struct widget_event_arg arg;
	int ret;

	ret = packet_get(packet, "ss", &arg.pkgname, &arg.id);
	if (ret != 2) {
		ErrPrint("Invalid parameter\n");
		return NULL;
	}

	arg.type = WIDGET_EVENT_WIDGET_PAUSE;

	if (s_info.table.widget_pause) {
		(void)s_info.table.widget_pause(&arg, s_info.data);
	}

	return NULL;
}

static struct packet *master_widget_resume(pid_t pid, int handle, const struct packet *packet)
{
	struct widget_event_arg arg;
	int ret;

	ret = packet_get(packet, "ss", &arg.pkgname, &arg.id);
	if (ret != 2) {
		ErrPrint("Invalid parameter\n");
		return NULL;
	}

	arg.type = WIDGET_EVENT_WIDGET_RESUME;

	if (s_info.table.widget_resume) {
		(void)s_info.table.widget_resume(&arg, s_info.data);
	}

	return NULL;
}

/* timestamp, ret */
static struct packet *master_pause(pid_t pid, int handle, const struct packet *packet)
{
	struct widget_event_arg arg;
	struct packet *result;
	int ret;

	ret = packet_get(packet, "d", &arg.info.pause.timestamp);
	if (ret != 1) {
		ErrPrint("Parameter is not valid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto errout;
	}
	arg.pkgname = NULL;
	arg.id = NULL;
	arg.type = WIDGET_EVENT_PAUSE;

	if (s_info.table.pause) {
		ret = s_info.table.pause(&arg, s_info.data);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}

errout:
	result = packet_create_reply(packet, "i", ret);
	return result;
}

static struct packet *master_update_mode(pid_t pid, int handle, const struct packet *packet)
{
	struct packet *result;
	struct widget_event_arg arg;
	int ret;

	ret = packet_get(packet, "ssi", &arg.pkgname, &arg.id, &arg.info.update_mode.active_update);
	if (ret != 3) {
		ErrPrint("Invalid parameter\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto errout;
	}

	if (s_info.table.update_mode) {
		ret = s_info.table.update_mode(&arg, s_info.data);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}

errout:
	result = packet_create_reply(packet, "i", ret);
	return result;
}

static struct packet *master_widget_mouse_set(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	double timestamp;
	const char *id;
	int ret;
	int x;
	int y;
	int fd;

	ret = packet_get(packet, "ssdii", &pkgname, &id, &timestamp, &x, &y);
	if (ret != 5) {
		ErrPrint("Parameter is not matched\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto out;
	}

	fd = packet_fd(packet);
	DbgPrint("FD: %d\n", fd);
	if (fd >= 0 || event_input_fd() >= 0) {
		widget_buffer_h buffer_handler;

		buffer_handler = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
		if (!buffer_handler) {
			if (close(fd) < 0) {
				ErrPrint("close: %d\n", errno);
			}

			ErrPrint("Unable to find a buffer handler: %s\n", id);
			return NULL;
		}

		ret = event_add_object(fd, buffer_handler, current_time_get(timestamp), x, y);
	}

out:
	return NULL;
}

static struct packet *master_widget_mouse_unset(pid_t pid, int handle, const struct packet *packet)
{
	widget_buffer_h buffer_handler;
	const char *pkgname;
	double timestamp;
	const char *id;
	int ret;
	int x;
	int y;

	ret = packet_get(packet, "ssdii", &pkgname, &id, &timestamp, &x, &y);
	if (ret != 5) {
		ErrPrint("Parameter is not matched\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto out;
	}

	buffer_handler = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (buffer_handler) {
		ret = event_remove_object(buffer_handler);
	}

out:
	return NULL;
}

static struct packet *master_gbar_mouse_set(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	double timestamp;
	const char *id;
	int ret;
	int x;
	int y;
	int fd;

	ret = packet_get(packet, "ssdii", &pkgname, &id, &timestamp, &x, &y);
	if (ret != 5) {
		ErrPrint("Parameter is not matched\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto out;
	}

	fd = packet_fd(packet);
	DbgPrint("FD: %d\n", fd);
	if (fd >= 0 || event_input_fd() >= 0) {
		widget_buffer_h buffer_handler;

		buffer_handler = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
		if (!buffer_handler) {
			if (close(fd) < 0) {
				ErrPrint("close: %d\n", errno);
			}

			ErrPrint("Unable to find a buffer handler: %s\n", id);
			return NULL;
		}

		ret = event_add_object(fd, buffer_handler, current_time_get(timestamp), x, y);
	}

out:
	return NULL;
}

static struct packet *master_gbar_mouse_unset(pid_t pid, int handle, const struct packet *packet)
{
	widget_buffer_h buffer_handler;
	const char *pkgname;
	double timestamp;
	const char *id;
	int ret;
	int x;
	int y;

	ret = packet_get(packet, "ssdii", &pkgname, &id, &timestamp, &x, &y);
	if (ret != 5) {
		ErrPrint("Parameter is not matched\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto out;
	}

	buffer_handler = widget_provider_buffer_find_buffer(WIDGET_TYPE_GBAR, pkgname, id);
	if (buffer_handler) {
		ret = event_remove_object(buffer_handler);
	}

out:
	return NULL;
}

static struct packet *master_orientation(pid_t pid, int handle, const struct packet *packet)
{
	struct widget_event_arg arg;
	int ret;

	ret = packet_get(packet, "ssi", &arg.pkgname, &arg.id, &arg.info.orientation.degree);
	if (ret != 3) {
		ErrPrint("Parameter is not valid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto errout;
	}

	arg.type = WIDGET_EVENT_ORIENTATION;

	if (s_info.table.orientation) {
		ret = s_info.table.orientation(&arg, s_info.data);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}

errout:
	return NULL;
}

/* timestamp, ret */
static struct packet *master_resume(pid_t pid, int handle, const struct packet *packet)
{
	struct packet *result;
	struct widget_event_arg arg;
	int ret;

	ret = packet_get(packet, "d", &arg.info.resume.timestamp);
	if (ret != 1) {
		ErrPrint("Parameter is not valid\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto errout;
	}

	arg.pkgname = NULL;
	arg.id = NULL;
	arg.type = WIDGET_EVENT_RESUME;

	if (s_info.table.resume) {
		ret = s_info.table.resume(&arg, s_info.data);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}

errout:
	result = packet_create_reply(packet, "i", ret);
	if (!result) {
		ErrPrint("Failed to create result packet\n");
	}
	return result;
}

struct packet *master_gbar_create(pid_t pid, int handle, const struct packet *packet)
{
	struct widget_event_arg arg;
	int ret;

	ret = packet_get(packet, "ssiidd", &arg.pkgname, &arg.id, &arg.info.gbar_create.w, &arg.info.gbar_create.h, &arg.info.gbar_create.x, &arg.info.gbar_create.y);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	arg.type = WIDGET_EVENT_GBAR_CREATE;

	DbgPrint("PERF_WIDGET\n");
	if (s_info.table.gbar_create) {
		(void)s_info.table.gbar_create(&arg, s_info.data);
	}

out:
	return NULL;
}

struct packet *master_disconnect(pid_t pid, int handle, const struct packet *packet)
{
	double timestamp;
	int ret;

	ret = packet_get(packet, "d", &timestamp);
	if (ret != 1) {
		ErrPrint("Invalid packet\n");
		goto errout;
	}

	if (s_info.fd >= 0 && s_info.closing_fd == 0) {
		s_info.closing_fd = 1;
		com_core_packet_client_fini(s_info.fd);
		fb_master_disconnected();
		s_info.fd = -1;
		s_info.closing_fd = 0;
	}

errout:
	return NULL;
}

struct packet *master_viewer_connected(pid_t pid, int handle, const struct packet *packet)
{
	int ret;
	struct widget_event_arg arg;

	ret = packet_get(packet, "sss", &arg.pkgname, &arg.id, &arg.info.viewer_connected.direct_addr);
	if (ret != 3) {
		ErrPrint("Invalid packet\n");
		goto errout;
	}

	/**
	 * Create a new connection if the direct_path is valid
	 */
	arg.type = WIDGET_EVENT_VIEWER_CONNECTED;

	if (s_info.table.viewer_connected) {
		ret = s_info.table.viewer_connected(&arg, s_info.data);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}

errout:
	return NULL;
}

struct packet *master_viewer_disconnected(pid_t pid, int handle, const struct packet *packet)
{
	int ret;
	struct widget_event_arg arg;

	ret = packet_get(packet, "sss", &arg.pkgname, &arg.id, &arg.info.viewer_disconnected.direct_addr);
	if (ret != 3) {
		ErrPrint("Invalid packet\n");
		goto errout;
	}

	/**
	 * Destroy a connection, maybe it is already destroyed. ;)
	 */
	arg.type = WIDGET_EVENT_VIEWER_DISCONNECTED;

	if (s_info.table.viewer_disconnected) {
		ret = s_info.table.viewer_disconnected(&arg, s_info.data);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}

errout:
	return NULL;
}

struct packet *master_ctrl_mode(pid_t pid, int handle, const struct packet *packet)
{
	int ret;
	struct widget_event_arg arg;

	ret = packet_get(packet, "ssii", &arg.pkgname, &arg.id, &arg.info.ctrl_mode.cmd, &arg.info.ctrl_mode.value);
	if (ret != 4) {
		ErrPrint("Invalid packet\n");
		goto errout;
	}

	arg.type = WIDGET_EVENT_CTRL_MODE;

	if (s_info.table.ctrl_mode) {
		(void)s_info.table.ctrl_mode(&arg, s_info.data);
	}

errout:
	return NULL;
}

struct packet *master_gbar_move(pid_t pid, int handle, const struct packet *packet)
{
	struct widget_event_arg arg;
	int ret;

	ret = packet_get(packet, "ssiidd", &arg.pkgname, &arg.id, &arg.info.gbar_move.w, &arg.info.gbar_move.h, &arg.info.gbar_move.x, &arg.info.gbar_move.y);
	if (ret != 6) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	arg.type = WIDGET_EVENT_GBAR_MOVE;

	if (s_info.table.gbar_move) {
		(void)s_info.table.gbar_move(&arg, s_info.data);
	}

out:
	return NULL;
}

struct packet *master_gbar_destroy(pid_t pid, int handle, const struct packet *packet)
{
	struct widget_event_arg arg;
	int ret;

	ret = packet_get(packet, "ssi", &arg.pkgname, &arg.id, &arg.info.gbar_destroy.reason);
	if (ret != 3) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	arg.type = WIDGET_EVENT_GBAR_DESTROY;
	if (s_info.table.gbar_destroy) {
		(void)s_info.table.gbar_destroy(&arg, s_info.data);
	}

out:
	return NULL;
}

static struct method s_table[] = {
	/*!< For the buffer type */
	{
		.cmd = CMD_STR_GBAR_MOUSE_MOVE,
		.handler = provider_buffer_gbar_mouse_move,
	},
	{
		.cmd = CMD_STR_WIDGET_MOUSE_MOVE,
		.handler = provider_buffer_widget_mouse_move,
	},
	{
		.cmd = CMD_STR_GBAR_MOUSE_DOWN,
		.handler = provider_buffer_gbar_mouse_down,
	},
	{
		.cmd = CMD_STR_GBAR_MOUSE_UP,
		.handler = provider_buffer_gbar_mouse_up,
	},
	{
		.cmd = CMD_STR_WIDGET_MOUSE_DOWN,
		.handler = provider_buffer_widget_mouse_down,
	},
	{
		.cmd = CMD_STR_WIDGET_MOUSE_UP,
		.handler = provider_buffer_widget_mouse_up,
	},
	{
		.cmd = CMD_STR_GBAR_MOUSE_ENTER,
		.handler = provider_buffer_gbar_mouse_enter,
	},
	{
		.cmd = CMD_STR_GBAR_MOUSE_LEAVE,
		.handler = provider_buffer_gbar_mouse_leave,
	},
	{
		.cmd = CMD_STR_WIDGET_MOUSE_ENTER,
		.handler = provider_buffer_widget_mouse_enter,
	},
	{
		.cmd = CMD_STR_WIDGET_MOUSE_LEAVE,
		.handler = provider_buffer_widget_mouse_leave,
	},
	{
		.cmd = CMD_STR_WIDGET_MOUSE_ON_SCROLL,
		.handler = provider_buffer_widget_mouse_on_scroll,
	},
	{
		.cmd = CMD_STR_WIDGET_MOUSE_OFF_SCROLL,
		.handler = provider_buffer_widget_mouse_off_scroll,
	},
	{
		.cmd = CMD_STR_GBAR_MOUSE_ON_SCROLL,
		.handler = provider_buffer_gbar_mouse_on_scroll,
	},
	{
		.cmd = CMD_STR_GBAR_MOUSE_OFF_SCROLL,
		.handler = provider_buffer_gbar_mouse_off_scroll,
	},
	{
		.cmd = CMD_STR_WIDGET_MOUSE_ON_HOLD,
		.handler = provider_buffer_widget_mouse_on_hold,
	},
	{
		.cmd = CMD_STR_WIDGET_MOUSE_OFF_HOLD,
		.handler = provider_buffer_widget_mouse_off_hold,
	},
	{
		.cmd = CMD_STR_GBAR_MOUSE_ON_HOLD,
		.handler = provider_buffer_gbar_mouse_on_hold,
	},
	{
		.cmd = CMD_STR_GBAR_MOUSE_OFF_HOLD,
		.handler = provider_buffer_gbar_mouse_off_hold,
	},
	{
		.cmd = CMD_STR_CLICKED,
		.handler = master_clicked, /* pkgname, id, event, timestamp, x, y, ret */
	},
	{
		.cmd = CMD_STR_TEXT_SIGNAL,
		.handler = master_text_signal, /* pkgname, id, signal_name, source, x, y, ex, ey, ret */
	},
	{
		.cmd = CMD_STR_DELETE,
		.handler = master_delete, /* pkgname, id, ret */
	},
	{
		.cmd = CMD_STR_RESIZE,
		.handler = master_resize, /* pkgname, id, w, h, ret */
	},
	{
		.cmd = CMD_STR_NEW,
		.handler = master_new, /* pkgname, id, content, timeout, has_script, period, cluster, category, pinup, skip_need_to_create, abi, result, width, height, priority */
	},
	{
		.cmd = CMD_STR_SET_PERIOD,
		.handler = master_set_period, /* pkgname, id, period, ret */
	},
	{
		.cmd = CMD_STR_CHANGE_GROUP,
		.handler = master_change_group, /* pkgname, id, cluster, category, ret */
	},
	{
		.cmd = CMD_STR_GBAR_MOVE,
		.handler = master_gbar_move,
	},
	{
		.cmd = CMD_STR_GBAR_ACCESS_HL,
		.handler = provider_buffer_gbar_access_hl,
	},
	{
		.cmd = CMD_STR_GBAR_ACCESS_ACTIVATE,
		.handler = provider_buffer_gbar_access_activate,
	},
	{
		.cmd = CMD_STR_GBAR_ACCESS_ACTION,
		.handler = provider_buffer_gbar_access_action,
	},
	{
		.cmd = CMD_STR_GBAR_ACCESS_SCROLL,
		.handler = provider_buffer_gbar_access_scroll,
	},
	{
		.cmd = CMD_STR_GBAR_ACCESS_VALUE_CHANGE,
		.handler = provider_buffer_gbar_access_value_change,
	},
	{
		.cmd = CMD_STR_GBAR_ACCESS_MOUSE,
		.handler = provider_buffer_gbar_access_mouse,
	},
	{
		.cmd = CMD_STR_GBAR_ACCESS_BACK,
		.handler = provider_buffer_gbar_access_back,
	},
	{
		.cmd = CMD_STR_GBAR_ACCESS_OVER,
		.handler = provider_buffer_gbar_access_over,
	},
	{
		.cmd = CMD_STR_GBAR_ACCESS_READ,
		.handler = provider_buffer_gbar_access_read,
	},
	{
		.cmd = CMD_STR_GBAR_ACCESS_ENABLE,
		.handler = provider_buffer_gbar_access_enable,
	},
	{
		.cmd = CMD_STR_WIDGET_ACCESS_HL,
		.handler = provider_buffer_widget_access_hl,
	},
	{
		.cmd = CMD_STR_WIDGET_ACCESS_ACTIVATE,
		.handler = provider_buffer_widget_access_activate,
	},
	{
		.cmd = CMD_STR_WIDGET_ACCESS_ACTION,
		.handler = provider_buffer_widget_access_action,
	},
	{
		.cmd = CMD_STR_WIDGET_ACCESS_SCROLL,
		.handler = provider_buffer_widget_access_scroll,
	},
	{
		.cmd = CMD_STR_WIDGET_ACCESS_VALUE_CHANGE,
		.handler = provider_buffer_widget_access_value_change,
	},
	{
		.cmd = CMD_STR_WIDGET_ACCESS_MOUSE,
		.handler = provider_buffer_widget_access_mouse,
	},
	{
		.cmd = CMD_STR_WIDGET_ACCESS_BACK,
		.handler = provider_buffer_widget_access_back,
	},
	{
		.cmd = CMD_STR_WIDGET_ACCESS_OVER,
		.handler = provider_buffer_widget_access_over,
	},
	{
		.cmd = CMD_STR_WIDGET_ACCESS_READ,
		.handler = provider_buffer_widget_access_read,
	},
	{
		.cmd = CMD_STR_WIDGET_ACCESS_ENABLE,
		.handler = provider_buffer_widget_access_enable,
	},
	{
		.cmd = CMD_STR_WIDGET_KEY_DOWN,
		.handler = provider_buffer_widget_key_down,
	},
	{
		.cmd = CMD_STR_WIDGET_KEY_UP,
		.handler = provider_buffer_widget_key_up,
	},
	{
		.cmd = CMD_STR_WIDGET_KEY_FOCUS_IN,
		.handler = provider_buffer_widget_key_focus_in,
	},
	{
		.cmd = CMD_STR_WIDGET_KEY_FOCUS_OUT,
		.handler = provider_buffer_widget_key_focus_out,
	},
	{
		.cmd = CMD_STR_GBAR_KEY_DOWN,
		.handler = provider_buffer_gbar_key_down,
	},
	{
		.cmd = CMD_STR_GBAR_KEY_UP,
		.handler = provider_buffer_gbar_key_up,
	},
	{
		.cmd = CMD_STR_GBAR_KEY_FOCUS_IN,
		.handler = provider_buffer_widget_key_focus_in,
	},
	{
		.cmd = CMD_STR_GBAR_KEY_FOCUS_OUT,
		.handler = provider_buffer_widget_key_focus_out,
	},
	{
		.cmd = CMD_STR_UPDATE_MODE,
		.handler = master_update_mode,
	},
	{
		.cmd = CMD_STR_WIDGET_MOUSE_SET,
		.handler = master_widget_mouse_set,
	},
	{
		.cmd = CMD_STR_WIDGET_MOUSE_UNSET,
		.handler = master_widget_mouse_unset,
	},
	{
		.cmd = CMD_STR_GBAR_MOUSE_SET,
		.handler = master_gbar_mouse_set,
	},
	{
		.cmd = CMD_STR_GBAR_MOUSE_UNSET,
		.handler = master_gbar_mouse_unset,
	},
	{
		.cmd = CMD_STR_ORIENTATION,
		.handler = master_orientation,
	},
	{
		.cmd = CMD_STR_GBAR_SHOW,
		.handler = master_gbar_create,
	},
	{
		.cmd = CMD_STR_GBAR_HIDE,
		.handler = master_gbar_destroy,
	},
	{
		.cmd = CMD_STR_WIDGET_PAUSE,
		.handler = master_widget_pause,
	},
	{
		.cmd = CMD_STR_WIDGET_RESUME,
		.handler = master_widget_resume,
	},
	{
		.cmd = CMD_STR_SCRIPT,
		.handler = master_script, /* pkgname, id, signal_name, source, sx, sy, ex, ey, x, y, down, ret */
	},
	{
		.cmd = CMD_STR_RENEW,
		.handler = master_renew, /* pkgname, id, content, timeout, has_script, period, cluster, category, pinup, width, height, abi, ret */
	},
	{
		.cmd = CMD_STR_PINUP,
		.handler = master_pinup, /* pkgname, id, pinup, ret */
	},
	{
		.cmd = CMD_STR_UPDATE_CONTENT,
		.handler = master_update_content, /* pkgname, cluster, category, ret */
	},
	{
		.cmd = CMD_STR_PAUSE,
		.handler = master_pause, /* timestamp, ret */
	},
	{
		.cmd = CMD_STR_RESUME,
		.handler = master_resume, /* timestamp, ret */
	},
	{
		.cmd = CMD_STR_DISCONNECT,
		.handler = master_disconnect,
	},
	{
		.cmd = CMD_STR_VIEWER_CONNECTED,
		.handler = master_viewer_connected,
	},
	{
		.cmd = CMD_STR_VIEWER_DISCONNECTED,
		.handler = master_viewer_disconnected,
	},
	{
		.cmd = CMD_STR_CTRL_MODE,
		.handler = master_ctrl_mode,
	},
	{
		.cmd = NULL,
		.handler = NULL,
	},
};

static int connected_cb(int handle, void *data)
{
	int ret = WIDGET_ERROR_NONE;

	DbgPrint("Connected (%p) %d\n", s_info.table.connected, handle);

	if (s_info.fd >= 0) {
		ErrPrint("Already connected. Ignore this (%d)?\n", handle);
		return WIDGET_ERROR_NONE;
	}

	s_info.fd = handle;

	if (s_info.table.connected) {
		ret = s_info.table.connected(NULL, s_info.data);
	}

	return ret;
}

static int disconnected_cb(int handle, void *data)
{
	if (s_info.fd != handle) {
		DbgPrint("%d is not my favor (%d)\n", handle, s_info.fd);
		return 0;
	}

	DbgPrint("Disconnected (%d)\n", handle);
	if (s_info.table.disconnected) {
		s_info.table.disconnected(NULL, s_info.data);
	}

	/* Reset the FD */
	s_info.fd = -1;
	return 0;
}

static int initialize_provider(void *disp, const char *name, widget_event_table_h table, void *data)
{
	int ret;

	s_info.name = strdup(name);
	if (!s_info.name) {
		ErrPrint("Heap: %d\n", errno);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	memcpy(&s_info.table, table, sizeof(*table));
	s_info.data = data;

	com_core_add_event_callback(CONNECTOR_DISCONNECTED, disconnected_cb, NULL);

	ret = com_core_packet_client_init(SLAVE_SOCKET, 0, s_table);
	if (ret < 0) {
		ErrPrint("Failed to establish the connection with the master\n");
		s_info.data = NULL;
		free(s_info.name);
		s_info.name = NULL;
		return (ret == -EACCES) ? WIDGET_ERROR_PERMISSION_DENIED : WIDGET_ERROR_FAULT;
	}

	widget_provider_buffer_init(disp);
	ret = connected_cb(ret, NULL);

	if (ret == WIDGET_ERROR_NONE) {
		DbgPrint("Slave is initialized\n");
	} else {
		DbgPrint("Slave is not initialized: %d\n", ret);
	}

	return ret;
}

static char *keep_file_in_safe(const char *id, int uri)
{
	const char *path;
	int len;
	int base_idx;
	char *new_path;

	path = uri ? widget_util_uri_to_path(id) : id;
	if (!path) {
		ErrPrint("Invalid path\n");
		return NULL;
	}

	if (s_info.prevent_overwrite) {
		char *ret;

		ret = strdup(path);
		if (!ret) {
			ErrPrint("Heap: %d\n", errno);
		}

		return ret;
	}

	if (access(path, R_OK | F_OK) != 0) {
		ErrPrint("[%s] %d\n", path, errno);
		return NULL;
	}

	len = strlen(path);
	base_idx = len - 1;

	while (base_idx > 0 && path[base_idx] != '/') base_idx--;
	base_idx += (path[base_idx] == '/');

	new_path = malloc(len + 10 + 30); /* for "tmp" tv_sec, tv_usec */
	if (!new_path) {
		ErrPrint("Heap: %d\n", errno);
		return NULL;
	}

	strncpy(new_path, path, base_idx);

#if defined(_USE_ECORE_TIME_GET)
	double tval;

	tval = util_timestamp();

	snprintf(new_path + base_idx, len + 10 - base_idx + 30, "reader/%lf.%s", tval, path + base_idx);
#else
	struct timeval tv;
	if (gettimeofday(&tv, NULL) < 0) {
		ErrPrint("gettimeofday: %d\n", errno);
		tv.tv_sec = rand();
		tv.tv_usec = rand();
	}

	snprintf(new_path + base_idx, len + 10 - base_idx + 30, "reader/%lu.%lu.%s", tv.tv_sec, tv.tv_usec, path + base_idx);
#endif

	/*!
	 * To prevent from failing of rename a content file
	 */
	(void)unlink(new_path);

	if (rename(path, new_path) < 0) {
		ErrPrint("Failed to keep content in safe: %d (%s -> %s)\n", errno, path, new_path);
	}

	return new_path;
}

const char *provider_name(void)
{
	return s_info.name;
}

EAPI int widget_provider_prepare_init(const char *abi, const char *accel, int secured)
{
	if (s_info.abi) {
		free(s_info.abi);
		s_info.abi = NULL;
	}

	if (s_info.accel) {
		free(s_info.accel);
		s_info.accel = NULL;
	}

	if (abi) {
		s_info.abi = strdup(abi);
		if (!s_info.abi) {
			ErrPrint("strdup: %d\n", errno);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}
	}

	if (accel) {
		s_info.accel = strdup(accel);
		if (!s_info.accel) {
			ErrPrint("strdup: %d\n", errno);
			free(s_info.abi);
			s_info.abi = NULL;
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}
	}

	s_info.secured = secured;

	return WIDGET_ERROR_NONE;
}

EAPI int widget_provider_init(void *disp, const char *name, widget_event_table_h table, void *data, int prevent_overwrite, int com_core_use_thread)
{
	if (!name || !table) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (s_info.name) {
		ErrPrint("Provider is already initialized\n");
		return WIDGET_ERROR_ALREADY_STARTED;
	}

	s_info.prevent_overwrite = prevent_overwrite;
	com_core_packet_use_thread(com_core_use_thread);

	return initialize_provider(disp, name, table, data);
}

EAPI void *widget_provider_fini(void)
{
	void *ret;
	static int provider_fini_called = 0;

	if (provider_fini_called) {
		ErrPrint("Provider finalize is already called\n");
		return NULL;
	}

	if (!s_info.name) {
		ErrPrint("Connection is not established (or cleared already)\n");
		return NULL;
	}

	provider_fini_called = 1;

	if (s_info.fd >= 0 && s_info.closing_fd == 0) {
		s_info.closing_fd = 1;
		com_core_packet_client_fini(s_info.fd);
		fb_master_disconnected();
		s_info.fd = -1;
		s_info.closing_fd = 0;
	}

	provider_fini_called = 0;

	com_core_del_event_callback(CONNECTOR_DISCONNECTED, disconnected_cb, NULL);

	widget_provider_buffer_fini();

	free(s_info.name);
	s_info.name = NULL;

	free(s_info.accel);
	s_info.accel = NULL;

	free(s_info.abi);
	s_info.abi = NULL;

	ret = s_info.data;
	s_info.data = NULL;

	return ret;
}

EAPI int widget_provider_send_call(const char *pkgname, const char *id, const char *funcname)
{
	struct packet *packet;
	unsigned int cmd = CMD_CALL;
	int ret;

	if (!pkgname || !id || !funcname) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "sss", pkgname, id, funcname);
	if (!packet) {
		ErrPrint("Failed to create a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = com_core_packet_send_only(s_info.fd, packet);
	packet_destroy(packet);
	return ret < 0 ? WIDGET_ERROR_FAULT : WIDGET_ERROR_NONE;
}

EAPI int widget_provider_send_ret(const char *pkgname, const char *id, const char *funcname)
{
	struct packet *packet;
	unsigned int cmd = CMD_RET;
	int ret;

	if (!pkgname || !id || !funcname) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "sss", pkgname, id, funcname);
	if (!packet) {
		ErrPrint("Failed to create a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = com_core_packet_send_only(s_info.fd, packet);
	packet_destroy(packet);
	return ret < 0 ? WIDGET_ERROR_FAULT : WIDGET_ERROR_NONE;
}

EAPI int widget_provider_send_faulted(const char *pkgname, const char *id, const char *funcname)
{
	struct packet *packet;
	unsigned int cmd = CMD_FAULTED;
	int ret;

	if (!pkgname || !id || !funcname) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "sss", pkgname, id, funcname);
	if (!packet) {
		ErrPrint("Failed to create a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = com_core_packet_send_only(s_info.fd, packet);
	packet_destroy(packet);
	return ret < 0 ? WIDGET_ERROR_FAULT : WIDGET_ERROR_NONE;
}

EAPI int widget_provider_send_hello_sync(const char *pkgname)
{
	struct packet *packet;
	struct packet *result;
	unsigned int cmd;
	int ret;
	const char *accel;
	const char *abi;
	struct widget_event_arg arg;
	int width = 0;
	int height = 0;
	double priority = 0.0f;
	double sync_ctx;

	DbgPrint("name[%s]\n", s_info.name);

	if (!s_info.name) {
		ErrPrint("Provider is not initialized\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (s_info.fd < 0) {
		ErrPrint("Connection is not established\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	accel = s_info.accel ? s_info.accel : SW_ACCEL;
	abi = s_info.abi ? s_info.abi : WIDGET_CONF_DEFAULT_ABI;
	DbgPrint("Accel[%s], abi[%s]\n", accel, abi);

	cmd = CMD_HELLO_SYNC_PREPARE;
	sync_ctx = util_timestamp();
	packet = packet_create_noack((const char *)&cmd, "d", sync_ctx);
	if (!packet) {
		ErrPrint("Failed to create a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = com_core_packet_send_only(s_info.fd, packet);
	packet_destroy(packet);
	if (ret < 0) {
		return WIDGET_ERROR_FAULT;
	}

	cmd = CMD_HELLO_SYNC;
	packet = packet_create((const char *)&cmd, "disssss", sync_ctx, s_info.secured, s_info.name, pkgname, accel, abi, pkgname);
	if (!packet) {
		ErrPrint("Failed to create a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	result = com_core_packet_oneshot_send(SLAVE_SOCKET, packet, 0.0f);
	packet_destroy(packet);

	if (s_info.table.widget_create) {
		arg.info.widget_create.out_content = NULL;
		arg.info.widget_create.out_title = NULL;
		arg.info.widget_create.out_is_pinned_up = 0;

		ret = packet_get(result, "sssiidssisiis", &arg.pkgname, &arg.id,
				&arg.info.widget_create.content,
				&arg.info.widget_create.timeout,
				&arg.info.widget_create.has_script,
				&arg.info.widget_create.period,
				&arg.info.widget_create.cluster, &arg.info.widget_create.category,
				&arg.info.widget_create.skip_need_to_create,
				&arg.info.widget_create.abi,
				&arg.info.widget_create.width,
				&arg.info.widget_create.height,
				&arg.info.widget_create.direct_addr);
		if (ret != 13) {
			int sync_status;

			ret = packet_get(result, "i", &sync_status);
			if (ret != 1) {
				ErrPrint("Parameter is not valid\n");
				packet_destroy(result);
				return WIDGET_ERROR_INVALID_PARAMETER;
			}

			return sync_status;
		}

		arg.type = WIDGET_EVENT_NEW;

		ret = s_info.table.widget_create(&arg, &width, &height, &priority, s_info.data);
	} else {
		ret = WIDGET_ERROR_NOT_SUPPORTED;
	}

	ErrPrint("#widget_create return [%d]\n", ret);
	packet_destroy(result);
	return WIDGET_ERROR_NONE;
}

EAPI int widget_provider_send_hello(void)
{
	struct packet *packet;
	unsigned int cmd = CMD_HELLO;
	int ret;
	const char *accel;
	const char *abi;

	DbgPrint("name[%s]\n", s_info.name);

	if (!s_info.name) {
		ErrPrint("Provider is not initialized\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (s_info.fd < 0) {
		ErrPrint("Connection is not established\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	accel = s_info.accel ? s_info.accel : SW_ACCEL;
	abi = s_info.abi ? s_info.abi : WIDGET_CONF_DEFAULT_ABI;
	DbgPrint("Accel[%s], abi[%s]\n", accel, abi);

	packet = packet_create_noack((const char *)&cmd, "isss", s_info.secured, s_info.name, accel, abi);
	if (!packet) {
		ErrPrint("Failed to create a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = com_core_packet_send_only(s_info.fd, packet);
	packet_destroy(packet);
	return ret < 0 ? WIDGET_ERROR_FAULT : WIDGET_ERROR_NONE;
}

EAPI int widget_provider_send_ping(void)
{
	struct packet *packet;
	unsigned int cmd = CMD_PING;
	int ret;

	DbgPrint("name[%s]\n", s_info.name);
	if (!s_info.name) {
		ErrPrint("Provider is not initialized\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (s_info.fd < 0) {
		ErrPrint("Connection is not established\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "s", s_info.name);
	if (!packet) {
		ErrPrint("Failed to create a a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = com_core_packet_send_only(s_info.fd, packet);
	packet_destroy(packet);
	return ret < 0 ? WIDGET_ERROR_FAULT : WIDGET_ERROR_NONE;
}

EAPI int widget_provider_send_widget_update_begin(const char *pkgname, const char *id, double priority, const char *content_info, const char *title)
{
	struct packet *packet;
	unsigned int cmd = CMD_WIDGET_UPDATE_BEGIN;
	int ret;

	if (!pkgname || !id) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!content_info) {
		content_info = "";
	}

	if (!title) {
		title = "";
	}

	if (s_info.fd < 0) {
		ErrPrint("Connection is not established\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "ssdss",
			pkgname, id, priority, content_info, title);
	if (!packet) {
		ErrPrint("Failed to build a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = com_core_packet_send_only(s_info.fd, packet);
	packet_destroy(packet);

	DbgPrint("[ACTIVE] WIDGET BEGIN: %s (%d)\n", id, ret);
	return ret < 0 ? WIDGET_ERROR_FAULT : WIDGET_ERROR_NONE;
}

EAPI int widget_provider_send_widget_update_end(const char *pkgname, const char *id)
{
	struct packet *packet;
	unsigned int cmd = CMD_WIDGET_UPDATE_END;
	int ret;

	if (!pkgname || !id) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (s_info.fd < 0) {
		ErrPrint("Connection is not established\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "ss", pkgname, id);
	if (!packet) {
		ErrPrint("Failed to build a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = com_core_packet_send_only(s_info.fd, packet);
	packet_destroy(packet);

	DbgPrint("[ACTIVE] WIDGET END: %s (%d)\n", id, ret);
	return ret < 0 ? WIDGET_ERROR_FAULT : WIDGET_ERROR_NONE;
}

EAPI int widget_provider_send_gbar_update_begin(const char *pkgname, const char *id)
{
	struct packet *packet;
	unsigned int cmd = CMD_GBAR_UPDATE_BEGIN;
	int ret;

	if (!pkgname || !id) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (s_info.fd < 0) {
		ErrPrint("Connection is not established\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "ss", pkgname, id);
	if (!packet) {
		ErrPrint("Failed to build a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = com_core_packet_send_only(s_info.fd, packet);
	packet_destroy(packet);

	DbgPrint("[ACTIVE] GBAR BEGIN: %s (%d)\n", id, ret);
	return ret < 0 ? WIDGET_ERROR_FAULT : WIDGET_ERROR_NONE;
}

EAPI int widget_provider_send_gbar_update_end(const char *pkgname, const char *id)
{
	struct packet *packet;
	unsigned int cmd = CMD_GBAR_UPDATE_END;
	int ret;

	if (!pkgname || !id) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (s_info.fd < 0) {
		ErrPrint("Connection is not established\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "ss", pkgname, id);
	if (!packet) {
		ErrPrint("Failed to build a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = com_core_packet_send_only(s_info.fd, packet);
	packet_destroy(packet);

	DbgPrint("[ACTIVE] GBAR END: %s (%d)\n", id, ret);
	return ret < 0 ? WIDGET_ERROR_FAULT : WIDGET_ERROR_NONE;
}

EAPI int widget_provider_send_extra_info(const char *pkgname, const char *id, double priority, const char *content_info, const char *title, const char *icon, const char *name)
{
	struct packet *packet;
	unsigned int cmd = CMD_EXTRA_INFO;
	int ret;

	if (!pkgname || !id) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (s_info.fd < 0) {
		ErrPrint("Connection is not established\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "ssssssd", pkgname, id, content_info, title, icon, name, priority);
	if (!packet) {
		ErrPrint("failed to build a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = com_core_packet_send_only(s_info.fd, packet);
	packet_destroy(packet);
	return ret < 0 ? WIDGET_ERROR_FAULT : WIDGET_ERROR_NONE;
}

__attribute__((always_inline)) static inline int send_extra_buffer_updated(int fd, const char *pkgname, const char *id, widget_buffer_h info, int idx, widget_damage_region_s *region, int is_gbar)
{
	int ret;
	struct packet *packet;
	unsigned int cmd;

	if (info->extra_buffer[idx] == 0u) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	fb_sync_xdamage(info->fb, region);

	cmd = CMD_EXTRA_UPDATED;

	packet = packet_create_noack((const char *)&cmd, "ssiiiiii", pkgname, id, is_gbar, idx, region->x, region->y, region->w, region->h);

	if (!packet) {
		ErrPrint("failed to build a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = com_core_packet_send_only(fd, packet);
	packet_destroy(packet);

	return ret;
}

__attribute__((always_inline)) static inline int send_extra_updated(int fd, const char *pkgname, const char *id, int idx, widget_damage_region_s *region, int is_gbar)
{
	widget_buffer_h info;
	widget_damage_region_s _region = {
		.x = 0,
		.y = 0,
		.w = 0,
		.h = 0,
	};
	int ret;

	if (!pkgname || !id || is_gbar < 0 || idx < 0 || idx > WIDGET_CONF_EXTRA_BUFFER_COUNT) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (fd < 0) {
		ErrPrint("Connection is not established\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	info = widget_provider_buffer_find_buffer(WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (!region) {
		int bpp = 0;
		(void)widget_provider_buffer_get_size(info, &_region.w, &_region.h, &bpp);
		region = &_region;
	}

	ret = send_extra_buffer_updated(fd, pkgname, id, info, idx, region, is_gbar);
	return ret < 0 ? WIDGET_ERROR_FAULT : WIDGET_ERROR_NONE;
}

__attribute__((always_inline)) static inline int send_buffer_updated(int fd, const char *pkgname, const char *id, widget_buffer_h info, widget_damage_region_s *region, int direct, int for_gbar, const char *safe_filename)
{
	struct packet *packet;
	unsigned int cmd = for_gbar ? CMD_DESC_UPDATED : CMD_UPDATED;
	int ret;

	if (direct && info && widget_provider_buffer_uri(info)) {
		fb_sync_xdamage(info->fb, region);
		packet = packet_create_noack((const char *)&cmd, "ssssiiii", pkgname, id, widget_provider_buffer_uri(info), safe_filename, region->x, region->y, region->w, region->h);
	} else {
		if (info) {
			fb_sync_xdamage(info->fb, region);
		}
		packet = packet_create_noack((const char *)&cmd, "sssiiii", pkgname, id, safe_filename, region->x, region->y, region->w, region->h);
	}

	if (!packet) {
		ErrPrint("failed to build a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = com_core_packet_send_only(fd, packet);
	packet_destroy(packet);

	return ret;
}

__attribute__((always_inline)) static inline int send_updated(int fd, const char *pkgname, const char *id, widget_damage_region_s *region, int direct, int for_gbar, const char *descfile)
{
	widget_buffer_h info;
	char *safe_filename = NULL;
	widget_damage_region_s _region = {
		.x = 0,
		.y = 0,
		.w = 0,
		.h = 0,
	};
	int ret;

	if (!pkgname || !id) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (fd < 0) {
		ErrPrint("Connection is not established\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (for_gbar && !descfile) {
		descfile = widget_util_uri_to_path(id); /* In case of the NULL descfilename, use the ID */
		if (!descfile) {
			ErrPrint("Invalid descfile: %s\n", id);
			return WIDGET_ERROR_INVALID_PARAMETER;
		}
	}

	info = widget_provider_buffer_find_buffer(for_gbar ? WIDGET_TYPE_GBAR : WIDGET_TYPE_WIDGET, pkgname, id);
	if (!info) {
		safe_filename = keep_file_in_safe(for_gbar ? descfile : id, 1);
		if (!safe_filename) {
			return WIDGET_ERROR_INVALID_PARAMETER;
		}
	} else {
		if (for_gbar) {
			safe_filename = strdup(descfile);
			if (!safe_filename) {
				ErrPrint("strdup: %d\n", errno);
				return WIDGET_ERROR_OUT_OF_MEMORY;
			}
		}
	}

	if (!region) {
		if (info) {
			int bpp = 0;
			(void)widget_provider_buffer_get_size(info, &_region.w, &_region.h, &bpp);
		}
		region = &_region;
	}

	ret = send_buffer_updated(fd, pkgname, id, info, region, direct, for_gbar, safe_filename);
	free(safe_filename);
	return ret < 0 ? WIDGET_ERROR_FAULT : WIDGET_ERROR_NONE;
}

EAPI int widget_provider_send_direct_updated(int fd, const char *pkgname, const char *id, int idx, widget_damage_region_s *region, int for_gbar, const char *descfile)
{
	int ret;

	if (idx == WIDGET_PRIMARY_BUFFER) {
		ret = send_updated(fd, pkgname, id, region, 1, for_gbar, descfile);
	} else {
		ret = send_extra_updated(fd, pkgname, id, idx, region, for_gbar);
	}

	return ret;
}

EAPI int widget_provider_send_updated(const char *pkgname, const char *id, int idx, widget_damage_region_s *region, int for_gbar, const char *descfile)
{
	int ret;

	if (idx == WIDGET_PRIMARY_BUFFER) {
		ret = send_updated(s_info.fd, pkgname, id, region, 0, for_gbar, descfile);
	} else {
		ret = send_extra_updated(s_info.fd, pkgname, id, idx, region, for_gbar);
	}

	return ret;
}

EAPI int widget_provider_send_direct_buffer_updated(int fd, widget_buffer_h handle, int idx, widget_damage_region_s *region, int for_gbar, const char *descfile)
{
	int ret;
	const char *pkgname;
	const char *id;

	pkgname = widget_provider_buffer_pkgname(handle);
	id = widget_provider_buffer_id(handle);

	if (idx == WIDGET_PRIMARY_BUFFER) {
		ret = send_buffer_updated(fd, pkgname, id, handle, region, 1, for_gbar, descfile);
	} else {
		ret = send_extra_buffer_updated(fd, pkgname, id, handle, idx, region, for_gbar);
	}

	return ret;
}

EAPI int widget_provider_send_buffer_updated(widget_buffer_h handle, int idx, widget_damage_region_s *region, int for_gbar, const char *descfile)
{
	int ret;
	const char *pkgname;
	const char *id;

	pkgname = widget_provider_buffer_pkgname(handle);
	id = widget_provider_buffer_id(handle);

	if (idx == WIDGET_PRIMARY_BUFFER) {
		ret = send_buffer_updated(s_info.fd, pkgname, id, handle, region, 0, for_gbar, descfile);
	} else {
		ret = send_extra_buffer_updated(s_info.fd, pkgname, id, handle, idx, region, for_gbar);
	}

	return ret;
}

EAPI int widget_provider_send_deleted(const char *pkgname, const char *id)
{
	struct packet *packet;
	unsigned int cmd = CMD_DELETED;
	int ret;

	if (!pkgname || !id) {
		ErrPrint("Invalid arguement\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (s_info.fd < 0) {
		ErrPrint("Connection is not established\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "ss", pkgname, id);
	if (!packet) {
		ErrPrint("Failed to build a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = com_core_packet_send_only(s_info.fd, packet);
	packet_destroy(packet);
	return ret < 0 ? WIDGET_ERROR_FAULT : WIDGET_ERROR_NONE;
}

EAPI int widget_provider_send_hold_scroll(const char *pkgname, const char *id, int hold)
{
	struct packet *packet;
	unsigned int cmd = CMD_SCROLL;
	int ret;

	if (!pkgname || !id) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (s_info.fd < 0) {
		ErrPrint("Connection is not established\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "ssi", pkgname, id, !!hold);
	if (!packet) {
		ErrPrint("Failed to build a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = com_core_packet_send_only(s_info.fd, packet);
	packet_destroy(packet);
	DbgPrint("[HOLD] Send hold: %d (%s) ret(%d)\n", hold, id, ret);
	return ret < 0 ? WIDGET_ERROR_FAULT : WIDGET_ERROR_NONE;
}

EAPI int widget_provider_send_access_status(const char *pkgname, const char *id, int status)
{
	struct packet *packet;
	unsigned int cmd = CMD_ACCESS_STATUS;
	int ret;

	if (!pkgname || !id) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (s_info.fd < 0) {
		ErrPrint("Connection is not established\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "ssi", pkgname, id, status);
	if (!packet) {
		ErrPrint("Failed to build a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = com_core_packet_send_only(s_info.fd, packet);
	packet_destroy(packet);

	DbgPrint("[ACCESS] Send status: %d (%s) (%d)\n", status, id, ret);
	return ret < 0 ? WIDGET_ERROR_FAULT : WIDGET_ERROR_NONE;
}

EAPI int widget_provider_send_key_status(const char *pkgname, const char *id, int status)
{
	struct packet *packet;
	unsigned int cmd = CMD_KEY_STATUS;
	int ret;

	if (!pkgname || !id) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (s_info.fd < 0) {
		ErrPrint("Connection is not established\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "ssi", pkgname, id, status);
	if (!packet) {
		ErrPrint("Failed to build a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = com_core_packet_send_only(s_info.fd, packet);
	packet_destroy(packet);

	DbgPrint("[KEY] Send status: %d (%s) (%d)\n", status, id, ret);
	return ret < 0 ? WIDGET_ERROR_FAULT : WIDGET_ERROR_NONE;
}

EAPI int widget_provider_send_request_close_gbar(const char *pkgname, const char *id, int reason)
{
	struct packet *packet;
	unsigned int cmd = CMD_CLOSE_GBAR;
	int ret;

	if (!pkgname || !id) {
		ErrPrint("Invalid argument\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (s_info.fd < 0) {
		ErrPrint("Connection is not established\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "ssi", pkgname, id, reason);
	if (!packet) {
		ErrPrint("Failed to build a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = com_core_packet_send_only(s_info.fd, packet);
	packet_destroy(packet);

	DbgPrint("[GBAR] Close GBAR: %d (%s) (%d)\n", reason, id, ret);
	return ret < 0 ? WIDGET_ERROR_FAULT : WIDGET_ERROR_NONE;
}

EAPI int widget_provider_control(int ctrl)
{
	struct packet *packet;
	unsigned int cmd = CMD_CTRL;
	int ret;

	if (s_info.fd < 0) {
		ErrPrint("Connection is not established\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	packet = packet_create_noack((const char *)&cmd, "i", ctrl);
	if (!packet) {
		ErrPrint("Failed to build a packet\n");
		return WIDGET_ERROR_FAULT;
	}

	ret = com_core_packet_send_only(s_info.fd, packet);
	packet_destroy(packet);

	DbgPrint("[CTRL] Request: 0x%x\n", (unsigned int)ctrl);
	return ret < 0 ? WIDGET_ERROR_FAULT : WIDGET_ERROR_NONE;
}

EAPI void *widget_provider_callback_data(void)
{
	return s_info.data;
}

/* End of a file */
