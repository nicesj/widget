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
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>

#include <dlog.h>
#include <glib.h>

#include <vconf.h>
#include <vconf-keys.h>

#include <packet.h>
#include <com-core.h>
#include <com-core_packet.h>
#include <widget_errno.h>
#include <widget_service.h>
#include <widget_service_internal.h>
#include <widget_cmd_list.h>
#include <widget_buffer.h>
#include <widget_conf.h>
#include <widget_util.h>
#include <secure_socket.h>

#include "debug.h"
#include "client.h"
#include "widget_viewer.h"
#include "widget_viewer_internal.h"
#include "desc_parser.h"
#include "fb.h"
#include "util.h"
#include "master_rpc.h"
#include "conf.h"
#include "file_service.h"
#include "dlist.h"

int errno;

#define MAX_DIRECT_ADDR 256

static struct info {
	int fd;
	int direct_fd;
	int master_direct_fd;
	guint timer_id;
	char *client_addr;
	char *direct_addr;
} s_info = {
	.fd = -1,
	.direct_fd = -1,
	.master_direct_fd = -1,
	.timer_id = 0,
	.client_addr = NULL,
	.direct_addr = NULL,
};

static struct packet *master_fault_package(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	const char *function;

	if (packet_get(packet, "sss", &pkgname, &id, &function) != 3) {
		ErrPrint("Invalid arguments\n");
		return NULL;
	}

	DbgPrint("[%s]\n", pkgname);
	master_rpc_clear_fault_package(pkgname);
	_widget_invoke_fault_handler(WIDGET_FAULT_DEACTIVATED, pkgname, id, function);
	return NULL;
}

static struct packet *master_hold_scroll(pid_t pid, int handle, const struct packet *packet)
{
	struct widget_common *common;
	widget_h widget;
	const char *pkgname;
	const char *id;
	int seize;
	int ret;
	struct dlist *l;

	ret = packet_get(packet, "ssi", &pkgname, &id, &seize);
	if (ret != 3) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("Instance(%s) is not exists\n", id);
		goto out;
	}

	DbgPrint("HOLD: [%s] seize(%d)\n", id, seize);
	seize = seize ? WIDGET_EVENT_HOLD_SCROLL : WIDGET_EVENT_RELEASE_SCROLL;
	dlist_foreach(common->widget_list, l, widget) {
		_widget_invoke_event_handler(widget, seize);
	}

out:
	return NULL;
}

static struct packet *master_pinup(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	const char *content;
	widget_h handler;
	struct dlist *l;
	struct dlist *n;
	struct widget_common *common;
	char *new_content;
	int ret;
	int status;
	int pinup;

	ret = packet_get(packet, "iisss", &status, &pinup, &pkgname, &id, &content);
	if (ret != 5) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("Instance (%s) is not exists\n", id);
		goto out;
	}

	if (status == (int)WIDGET_ERROR_NONE) {
		new_content = strdup(content);
		if (new_content) {
			free(common->content);
			common->content = new_content;
			common->is_pinned_up = pinup;
		} else {
			ErrPrint("Heap: %d\n", errno);
			status = WIDGET_ERROR_OUT_OF_MEMORY;
		}
	}

	common->request.pinup = 0;

	dlist_foreach_safe(common->widget_list, l, n, handler) {
		if (handler->cbs.pinup.cb) {
			widget_ret_cb cb;
			void *cbdata;

			/* Make sure that user can call pinup API in its result callback */
			cb = handler->cbs.pinup.cb;
			cbdata = handler->cbs.pinup.data;

			handler->cbs.pinup.cb = NULL;
			handler->cbs.pinup.data = NULL;

			cb(handler, status, cbdata);
		} else if (status == (int)WIDGET_ERROR_NONE) {
			_widget_invoke_event_handler(handler, WIDGET_EVENT_PINUP_CHANGED);
		}
	}

out:
	return NULL;
}

static struct packet *master_deleted(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	double timestamp;
	widget_h handler;
	struct widget_common *common;
	struct dlist *l;
	struct dlist *n;
	int reason;

	if (packet_get(packet, "ssdi", &pkgname, &id, &timestamp, &reason) != 4) {
		ErrPrint("Invalid arguemnt\n");
		goto out;
	}

	DbgPrint("[%s]\n", pkgname);
	common = _widget_find_common_handle_by_timestamp(timestamp);
	if (!common) {
		/*!
		 * \note
		 * This can be happens only if the user delete a widget
		 * right after create it before receive created event.
		 */
		goto out;
	}

	/*!< Check validity of this "handler" */
	if (common->state != WIDGET_STATE_CREATE) {
		if (common->state != WIDGET_STATE_DELETE) {
			/*!
			 * \note
			 * This is not possible
			 */
			ErrPrint("Already deleted handler (%s - %s)\n", pkgname, id);
			return NULL;
		}
	}

	common->request.deleted = 0;
	/*!
	 * We should change the state of "common handler' before handling the callbacks.
	 * Because if user tries to create a new handle in the callbacks,
	 * find_sharable_common_handle will returns destroying object.
	 * Then we will get panic.
	 * To prevent it, we should change its state first.
	 */
	common->state = WIDGET_STATE_DELETE;

	dlist_foreach_safe(common->widget_list, l, n, handler) {
		if (handler->cbs.created.cb) {
			widget_ret_cb cb;
			void *cbdata;
			/*!
			 * \note
			 *
			 * "if (handler->id == NULL) {"
			 *
			 * The instance is not created yet.
			 * But the master forcely destroy it and send destroyed event to this
			 * without the created event.
			 *
			 * It could be destroyed when a slave has critical error(fault)
			 * before creating an instance successfully.
			 */
			if (handler->cbs.created.cb == handler->cbs.deleted.cb) {
				if (handler->cbs.created.data != handler->cbs.deleted.data) {
					DbgPrint("cb is same but cbdata is different (%s - %s)\n", pkgname, id);
				}

				handler->cbs.deleted.cb = NULL;
				handler->cbs.deleted.data = NULL;
			}

			cb = handler->cbs.created.cb;
			cbdata = handler->cbs.created.data;

			handler->cbs.created.cb = NULL;
			handler->cbs.created.data = NULL;

			if (reason == (int)WIDGET_ERROR_NONE) {
				reason = WIDGET_ERROR_CANCELED;
			}

			cb(handler, reason, cbdata);
		} else if (common->id) {
			if (handler->cbs.deleted.cb) {
				widget_ret_cb cb;
				void *cbdata;

				cb = handler->cbs.deleted.cb;
				cbdata = handler->cbs.deleted.data;

				handler->cbs.deleted.cb = NULL;
				handler->cbs.deleted.data = NULL;

				cb(handler, reason, cbdata);
			} else {
				_widget_invoke_event_handler(handler, WIDGET_EVENT_DELETED);
			}
		}

		/* Just try to delete it, if a user didn't remove it from the live box list */
		_widget_unref(handler, 1);
	}

out:
	return NULL;
}

static struct packet *master_widget_update_begin(pid_t pid, int handle, const struct packet *packet)
{
	widget_h handler;
	struct widget_common *common;
	const char *pkgname;
	const char *id;
	const char *content;
	const char *title;
	const char *fbfile;
	double priority;
	int ret;

	ret = packet_get(packet, "ssdsss", &pkgname, &id, &priority, &content, &title, &fbfile);
	if (ret != 6) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("Instance[%s] is not exists\n", id);
		goto out;
	}

	if (common->state != WIDGET_STATE_CREATE) {
		ErrPrint("(%s) is not created\n", id);
		goto out;
	}

	_widget_set_priority(common, priority);
	_widget_set_content(common, content);
	_widget_set_title(common, title);

	/*!
	 * \NOTE
	 * Width & Height is not changed in this case.
	 * If the active update is began, the size should not be changed,
	 * And if the size is changed, the provider should finish the updating first.
	 * And then begin updating again after change its size.
	 */
	if (_widget_get_widget_fb(common)) {
		(void)_widget_set_widget_fb(common, fbfile);

		ret = _widget_sync_widget_fb(common);

		if (ret != (int)WIDGET_ERROR_NONE) {
			ErrPrint("Failed to do sync FB (%s - %s) (%d)\n", pkgname, fbfile, ret);
		} else {
			struct dlist *l;
			dlist_foreach(common->widget_list, l, handler) {
				_widget_invoke_event_handler(handler, WIDGET_EVENT_WIDGET_UPDATE_BEGIN);
			}
		}
	} else {
		ErrPrint("Invalid request[%s], %s\n", id, fbfile);
	}

out:
	return NULL;
}

static struct packet *master_gbar_update_begin(pid_t pid, int handle, const struct packet *packet)
{
	widget_h handler;
	struct widget_common *common;
	const char *pkgname;
	const char *id;
	const char *fbfile;
	int ret;

	ret = packet_get(packet, "sss", &pkgname, &id, &fbfile);
	if (ret != 2) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("Instance[%s] is not exists\n", id);
		goto out;
	}

	if (common->state != WIDGET_STATE_CREATE) {
		ErrPrint("[%s] is not created\n", id);
		goto out;
	}

	if (_widget_get_gbar_fb(common)) {
		(void)_widget_set_gbar_fb(common, fbfile);

		ret = _widget_sync_gbar_fb(common);
		if (ret != (int)WIDGET_ERROR_NONE) {
			ErrPrint("Failed to do sync FB (%s - %s) (%d)\n", pkgname, fbfile, ret);
		} else {
			struct dlist *l;
			dlist_foreach(common->widget_list, l, handler) {
				_widget_invoke_event_handler(handler, WIDGET_EVENT_GBAR_UPDATE_BEGIN);
			}
		}
	} else {
		ErrPrint("Invalid request[%s], %s\n", id, fbfile);
	}

out:
	return NULL;
}

static struct packet *master_widget_update_end(pid_t pid, int handle, const struct packet *packet)
{
	widget_h handler;
	struct widget_common *common;
	const char *pkgname;
	const char *id;
	int ret;

	ret = packet_get(packet, "ss", &pkgname, &id);
	if (ret != 2) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("Instance[%s] is not exists\n", id);
		goto out;
	}

	if (common->state != WIDGET_STATE_CREATE) {
		ErrPrint("[%s] is not created\n", id);
		goto out;
	}

	if (_widget_get_widget_fb(common)) {
		struct dlist *l;
		dlist_foreach(common->widget_list, l, handler) {
			_widget_invoke_event_handler(handler, WIDGET_EVENT_WIDGET_UPDATE_END);
		}
	} else {
		ErrPrint("Invalid request[%s]\n", id);
	}

out:
	return NULL;
}

static struct packet *master_key_status(pid_t pid, int handle, const struct packet *packet)
{
	widget_h handler;
	struct widget_common *common;
	struct dlist *l;
	const char *pkgname;
	const char *id;
	int ret;
	int status;

	ret = packet_get(packet, "ssi", &pkgname, &id, &status);
	if (ret != 3) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("Instance[%s] is not exists\n", id);
		goto out;
	}

	if (common->state != WIDGET_STATE_CREATE) {
		ErrPrint("[%s] is not created\n", id);
		goto out;
	}

	common->request.key_event = 0;
	dlist_foreach(common->widget_list, l, handler) {
		if (handler->cbs.key_event.cb) {
			widget_ret_cb cb;
			void *cbdata;

			cb = handler->cbs.key_event.cb;
			cbdata = handler->cbs.key_event.data;

			handler->cbs.key_event.cb = NULL;
			handler->cbs.key_event.data = NULL;

			cb(handler, status, cbdata);
		} else {
			ErrPrint("Invalid event[%s]\n", id);
		}
	}

out:
	return NULL;
}

static struct packet *master_request_close_gbar(pid_t pid, int handle, const struct packet *packet)
{
	widget_h handler;
	struct widget_common *common;
	struct dlist *l;
	const char *pkgname;
	const char *id;
	int ret;
	int reason;

	ret = packet_get(packet, "ssi", &pkgname, &id, &reason);
	if (ret != 3) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("Instance[%s] is not exists\n", id);
		goto out;
	}

	if (common->state != WIDGET_STATE_CREATE) {
		ErrPrint("[%s] is not created\n", id);
		goto out;
	}

	if (!common->is_gbar_created) {
		DbgPrint("GBAR is not created, closing what?(%s)\n", id);
		goto out;
	}

	DbgPrint("Reason: %d\n", reason);

	dlist_foreach(common->widget_list, l, handler) {
		_widget_invoke_event_handler(handler, WIDGET_EVENT_REQUEST_CLOSE_GBAR);
	}
out:
	return NULL;
}

static struct packet *master_access_status(pid_t pid, int handle, const struct packet *packet)
{
	widget_h handler;
	struct widget_common *common;
	struct dlist *l;
	const char *pkgname;
	const char *id;
	int ret;
	int status;

	ret = packet_get(packet, "ssi", &pkgname, &id, &status);
	if (ret != 3) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("Instance[%s] is not exists\n", id);
		goto out;
	}

	if (common->state != WIDGET_STATE_CREATE) {
		ErrPrint("[%s] is not created\n", id);
		goto out;
	}

	common->request.access_event = 0;
	dlist_foreach(common->widget_list, l, handler) {
		if (handler->cbs.access_event.cb) {
			widget_ret_cb cb;
			void *cbdata;

			cb = handler->cbs.access_event.cb;
			cbdata = handler->cbs.access_event.data;

			handler->cbs.access_event.cb = NULL;
			handler->cbs.access_event.data = NULL;

			cb(handler, status, cbdata);
		}
	}
out:
	return NULL;
}

static struct packet *master_gbar_update_end(pid_t pid, int handle, const struct packet *packet)
{
	widget_h handler;
	struct widget_common *common;
	const char *pkgname;
	const char *id;
	int ret;

	ret = packet_get(packet, "ss", &pkgname, &id);
	if (ret != 2) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("Instance[%s] is not exists\n", id);
		goto out;
	}

	if (common->state != WIDGET_STATE_CREATE) {
		ErrPrint("[%s] is not created\n", id);
		goto out;
	}

	if (_widget_get_widget_fb(common)) {
		struct dlist *l;

		dlist_foreach(common->widget_list, l, handler) {
			_widget_invoke_event_handler(handler, WIDGET_EVENT_GBAR_UPDATE_END);
		}
	} else {
		ErrPrint("Invalid request[%s]", id);
	}

out:
	return NULL;
}

static struct packet *master_extra_info(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	const char *content;
	const char *title;
	const char *icon;
	const char *name;
	double priority;
	int ret;
	widget_h handler;
	struct widget_common *common;
	struct dlist *l;
	struct dlist *n;

	ret = packet_get(packet, "ssssssd", &pkgname, &id,
			&content, &title,
			&icon, &name,
			&priority);
	if (ret != 7) {
		ErrPrint("Invalid parameters\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("instance(%s) is not exists\n", id);
		goto out;
	}

	if (common->state != WIDGET_STATE_CREATE) {
		/*!
		 * \note
		 * Already deleted by the user.
		 * Don't try to notice anything with this, Just ignore all events
		 * Beacuse the user doesn't wants know about this anymore
		 */
		ErrPrint("(%s) is not exists, but updated\n", id);
		goto out;
	}

	_widget_set_priority(common, priority);
	_widget_set_content(common, content);
	_widget_set_title(common, title);
	_widget_set_alt_icon(common, icon);
	_widget_set_alt_name(common, name);

	dlist_foreach_safe(common->widget_list, l, n, handler) {
		_widget_invoke_event_handler(handler, WIDGET_EVENT_EXTRA_INFO_UPDATED);
	}
out:
	return NULL;
}

static struct packet *master_extra_updated(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	widget_h handler;
	struct widget_common *common;
	int ret;
	int x;
	int y;
	int w;
	int h;
	int is_gbar;
	int event_type;
	int idx;

	ret = packet_get(packet, "ssiiiiii", &pkgname, &id, &is_gbar, &idx, &x, &y, &w, &h);
	if (ret != 8) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("instance(%s) is not exists\n", id);
		goto out;
	}

	if (common->state != WIDGET_STATE_CREATE) {
		/*!
		 * \note
		 * Already deleted by the user.
		 * Don't try to notice anything with this, Just ignore all events
		 * Beacuse the user doesn't wants know about this anymore
		 */
		ErrPrint("(%s) is not exists, but updated\n", id);
		goto out;
	}

	if (is_gbar) {
		common->gbar.last_damage.x = x;
		common->gbar.last_damage.y = y;
		common->gbar.last_damage.w = w;
		common->gbar.last_damage.h = h;
		common->gbar.last_extra_buffer_idx = idx;

		event_type = WIDGET_EVENT_GBAR_EXTRA_UPDATED;
	} else {
		common->widget.last_damage.x = x;
		common->widget.last_damage.y = y;
		common->widget.last_damage.w = w;
		common->widget.last_damage.h = h;
		common->widget.last_extra_buffer_idx = idx;

		event_type = WIDGET_EVENT_WIDGET_EXTRA_UPDATED;

		if (conf_frame_drop_for_resizing() && common->request.size_changed) {
			/* Just for skipping the update event callback call, After request to resize buffer, update event will be discarded */
			DbgPrint("Discards obsoloted update event\n");
			ret = WIDGET_ERROR_RESOURCE_BUSY;
		} else {
			if (!conf_manual_sync()) {
				ret = _widget_sync_widget_fb(common);
				if (ret != (int)WIDGET_ERROR_NONE) {
					ErrPrint("Failed to do sync FB (%s - %s) (%d)\n", pkgname, widget_util_basename(util_uri_to_path(id)), ret);
				}
			} else {
				ret = WIDGET_ERROR_NONE;
			}
		}
	}

	if (!common->request.created && ret == (int)WIDGET_ERROR_NONE) {
		struct dlist *l;
		struct dlist *n;

		dlist_foreach_safe(common->widget_list, l, n, handler) {
			_widget_invoke_event_handler(handler, event_type);
		}
	}

out:
	return NULL;
}

static struct packet *master_widget_updated(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	const char *fbfile;
	const char *safe_file;
	widget_h handler;
	struct widget_common *common;
	int ret;
	int x;
	int y;
	int w;
	int h;

	ret = packet_get(packet, "ssssiiii", &pkgname, &id, &fbfile, &safe_file, &x, &y, &w, &h);
	if (ret != 8) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("instance(%s) is not exists\n", id);
		goto out;
	}

	if (common->state != WIDGET_STATE_CREATE) {
		/*!
		 * \note
		 * Already deleted by the user.
		 * Don't try to notice anything with this, Just ignore all events
		 * Beacuse the user doesn't wants know about this anymore
		 */
		ErrPrint("(%s) is not exists, but updated\n", id);
		goto out;
	}

	common->widget.last_damage.x = x;
	common->widget.last_damage.y = y;
	common->widget.last_damage.w = w;
	common->widget.last_damage.h = h;
	common->widget.last_extra_buffer_idx = WIDGET_PRIMARY_BUFFER;
	_widget_set_filename(common, safe_file);

	if (_widget_text_widget(common)) {
		const char *common_filename;

		common_filename = common->filename ? common->filename : util_uri_to_path(common->id);

		(void)parse_desc(common, common_filename, 0);
		/*!
		 * \note
		 * DESC parser will call the "text event callback".
		 * Don't need to call global event callback in this case.
		 */
		_widget_unlink_filename(common);
		goto out;
	} else if (_widget_get_widget_fb(common)) {
		/*!
		 * \todo
		 * replace this with "flag" instead of "callback address"
		 */
		if (conf_frame_drop_for_resizing() && common->request.size_changed) {
			/* Just for skipping the update event callback call, After request to resize buffer, update event will be discarded */
			DbgPrint("Discards obsoloted update event\n");
			ret = WIDGET_ERROR_RESOURCE_BUSY;
		} else {
			(void)_widget_set_widget_fb(common, fbfile);

			if (!conf_manual_sync()) {
				ret = _widget_sync_widget_fb(common);
				if (ret != (int)WIDGET_ERROR_NONE) {
					ErrPrint("Failed to do sync FB (%s - %s) (%d)\n", pkgname, widget_util_basename(util_uri_to_path(id)), ret);
				}
			} else {
				ret = WIDGET_ERROR_NONE;
			}
		}
	} else {
		ret = WIDGET_ERROR_NONE;
	}

	if (ret == (int)WIDGET_ERROR_NONE && !common->request.created) {
		struct dlist *l;
		struct dlist *n;

		dlist_foreach_safe(common->widget_list, l, n, handler) {
			_widget_invoke_event_handler(handler, WIDGET_EVENT_WIDGET_UPDATED);
		}
	}
	_widget_unlink_filename(common);

out:
	return NULL;
}

static struct packet *master_gbar_created(pid_t pid, int handle, const struct packet *packet)
{
	widget_h handler;
	struct widget_common *common;
	const char *pkgname;
	const char *id;
	const char *buf_id;
	struct dlist *l;
	struct dlist *n;
	int width;
	int height;
	int ret;
	int status;

	ret = packet_get(packet, "sssiii", &pkgname, &id, &buf_id, &width, &height, &status);
	if (ret != 6) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	DbgPrint("[%s]\n", pkgname);
	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("Instance(%s) is not exists\n", id);
		goto out;
	}

	if (common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Instance(%s) is not created\n", id);
		goto out;
	}

	if (!common->request.gbar_created) {
		ErrPrint("GBAR create request is canceled\n");
		goto out;
	}

	common->is_gbar_created = (status == (int)WIDGET_ERROR_NONE);
	common->request.gbar_created = 0;

	if (common->is_gbar_created) {
		_widget_set_gbarsize(common, width, height);
		if (_widget_text_gbar(common)) {
			DbgPrint("Text TYPE does not need to handle this\n");
		} else {
			(void)_widget_set_gbar_fb(common, buf_id);

			switch (common->gbar.type) {
			case GBAR_TYPE_SCRIPT:
			case GBAR_TYPE_BUFFER:
				switch (fb_type(_widget_get_gbar_fb(common))) {
				case WIDGET_FB_TYPE_FILE:
				case WIDGET_FB_TYPE_SHM:
					common->gbar.lock = widget_service_create_lock(common->id, WIDGET_TYPE_GBAR, WIDGET_LOCK_READ);
					break;
				case WIDGET_FB_TYPE_PIXMAP:
				case WIDGET_FB_TYPE_ERROR:
				default:
					break;
				}
				break;
			case GBAR_TYPE_UIFW:
			case GBAR_TYPE_TEXT:
			default:
				break;
			}

			ret = _widget_sync_gbar_fb(common);
			if (ret < 0) {
				ErrPrint("Failed to do sync FB (%s - %s)\n", pkgname, widget_util_basename(util_uri_to_path(id)));
			}
		}
	}

	DbgPrint("PERF_WIDGET\n");
	dlist_foreach_safe(common->widget_list, l, n, handler) {
		if (handler->cbs.gbar_created.cb) {
			widget_ret_cb cb;
			void *cbdata;

			cb = handler->cbs.gbar_created.cb;
			cbdata = handler->cbs.gbar_created.data;

			handler->cbs.gbar_created.cb = NULL;
			handler->cbs.gbar_created.data = NULL;

			/*!
			 * Before call the Callback function,
			 * gbar_create_cb must be reset.
			 * Because, in the create callback, user can call create_gbar function again.
			 */
			cb(handler, status, cbdata);
		} else if (status == (int)WIDGET_ERROR_NONE) {
			_widget_invoke_event_handler(handler, WIDGET_EVENT_GBAR_CREATED);
		}
	}

out:
	return NULL;
}

static struct packet *master_gbar_destroyed(pid_t pid, int handle, const struct packet *packet)
{
	widget_h handler;
	struct dlist *l;
	struct widget_common *common;
	const char *pkgname;
	const char *id;
	int ret;
	int status;

	ret = packet_get(packet, "ssi", &pkgname, &id, &status);
	if (ret != 3) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	DbgPrint("[%s]\n", pkgname);
	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("Instance(%s) is not exists\n", id);
		goto out;
	}

	if (common->state != WIDGET_STATE_CREATE) {
		ErrPrint("Instance(%s) is not created\n", id);
		goto out;
	}

	if (common->is_gbar_created == 0) {
		ErrPrint("GBAR is not created, event is ignored\n");
		goto out;
	}

	common->is_gbar_created = 0;
	common->request.gbar_destroyed = 0;

	dlist_foreach(common->widget_list, l, handler) {
		if (handler->cbs.gbar_destroyed.cb) {
			widget_ret_cb cb;
			void *cbdata;

			cb = handler->cbs.gbar_destroyed.cb;
			cbdata = handler->cbs.gbar_destroyed.data;

			handler->cbs.gbar_destroyed.cb = NULL;
			handler->cbs.gbar_destroyed.data = NULL;

			/*!
			 * Before call the Callback function,
			 * gbar_destroyed_cb must be reset.
			 * Because, in the create callback, user can call destroy_gbar function again.
			 */
			cb(handler, status, cbdata);
		} else if (status == (int)WIDGET_ERROR_NONE) {
			_widget_invoke_event_handler(handler, WIDGET_EVENT_GBAR_DESTROYED);
		}
	}

	/*!
	 * \note
	 * Lock file should be deleted after all callbacks are processed.
	 */
	switch (common->gbar.type) {
	case GBAR_TYPE_SCRIPT:
	case GBAR_TYPE_BUFFER:
		switch (fb_type(_widget_get_gbar_fb(common))) {
		case WIDGET_FB_TYPE_FILE:
		case WIDGET_FB_TYPE_SHM:
			widget_service_destroy_lock(common->gbar.lock, 0);
			common->gbar.lock = NULL;
			break;
		case WIDGET_FB_TYPE_PIXMAP:
		case WIDGET_FB_TYPE_ERROR:
		default:
			break;
		}
		break;
	case GBAR_TYPE_UIFW:
	case GBAR_TYPE_TEXT:
	default:
		break;
	}

out:
	return NULL;
}

static struct packet *master_gbar_updated(pid_t pid, int handle, const struct packet *packet)
{
	const char *pkgname;
	const char *id;
	const char *descfile;
	const char *fbfile;
	int ret;
	widget_h handler;
	struct widget_common *common;
	struct dlist *l;
	int x;
	int y;
	int w;
	int h;

	ret = packet_get(packet, "ssssiiii", &pkgname, &id, &fbfile, &descfile, &x, &y, &w, &h);
	if (ret != 8) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("Instance(%s) is not exists\n", id);
		goto out;
	}

	common->gbar.last_damage.x = x;
	common->gbar.last_damage.y = y;
	common->gbar.last_damage.w = w;
	common->gbar.last_damage.h = h;

	if (common->state != WIDGET_STATE_CREATE) {
		/*!
		 * \note
		 * This handler is already deleted by the user.
		 * So don't try to notice anything about this anymore.
		 * Just ignore all events.
		 */
		ErrPrint("Instance(%s) is not created\n", id);
		goto out;
	}

	if (_widget_text_gbar(common)) {
		(void)parse_desc(common, descfile, 1);
	} else {
		if (conf_frame_drop_for_resizing() && common->request.size_changed) {
			/* Just for skipping the update event callback call, After request to resize buffer, update event will be discarded */
			DbgPrint("Discards obsoloted update event\n");
		} else {
			(void)_widget_set_gbar_fb(common, fbfile);

			if (!conf_manual_sync()) {
				ret = _widget_sync_gbar_fb(common);
				if (ret < 0) {
					ErrPrint("Failed to do sync FB (%s - %s), %d\n", pkgname, widget_util_basename(util_uri_to_path(id)), ret);
				} else {
					dlist_foreach(common->widget_list, l, handler) {
						_widget_invoke_event_handler(handler, WIDGET_EVENT_GBAR_UPDATED);
					}
				}
			} else {
				dlist_foreach(common->widget_list, l, handler) {
					_widget_invoke_event_handler(handler, WIDGET_EVENT_GBAR_UPDATED);
				}
			}
		}
	}

out:
	return NULL;
}

static struct packet *master_gbar_extra_buffer_destroyed(pid_t pid, int handle, const struct packet *packet)
{
	widget_h handler;
	struct widget_common *common;
	const char *pkgname;
	struct dlist *l;
	struct dlist *n;
	const char *id;
	int pixmap;
	int idx;
	int ret;

	if (!packet) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	ret = packet_get(packet, "ssii", &pkgname, &id, &pixmap, &idx);
	if (ret != 4) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	if (idx < 0 || idx >= conf_extra_buffer_count()) {
		ErrPrint("Extra buffer count is not matched\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("WIDGET(%s) is not found\n", id);
		goto out;
	}

	if (common->state != WIDGET_STATE_CREATE) {
		ErrPrint("WIDGET(%s) is not created yet\n", id);
		goto out;
	}

	if (!common->gbar.extra_buffer && conf_extra_buffer_count()) {
		common->gbar.extra_buffer = calloc(conf_extra_buffer_count(), sizeof(*common->gbar.extra_buffer));
		if (!common->gbar.extra_buffer) {
			ErrPrint("WIDGET(%s) calloc: %d\n", id, errno);
			goto out;
		}
	}

	common->gbar.last_extra_buffer_idx = idx;
	if (common->gbar.extra_buffer[idx] != pixmap) {
		DbgPrint("Extra buffer Pixmap is not matched %u <> %u\n", common->widget.extra_buffer[idx], pixmap);
	}

	dlist_foreach_safe(common->widget_list, l, n, handler) {
		_widget_invoke_event_handler(handler, WIDGET_EVENT_GBAR_EXTRA_BUFFER_DESTROYED);
	}
out:
	return NULL;
}

static struct packet *master_widget_extra_buffer_destroyed(pid_t pid, int handle, const struct packet *packet)
{
	widget_h handler;
	struct widget_common *common;
	const char *pkgname;
	struct dlist *l;
	struct dlist *n;
	const char *id;
	int idx;
	int pixmap;
	int ret;

	if (!packet) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	ret = packet_get(packet, "ssii", &pkgname, &id, &pixmap, &idx);
	if (ret != 4) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	if (idx < 0 || idx >= conf_extra_buffer_count()) {
		ErrPrint("Extra buffer count is not matched\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("WIDGET(%s) is not found\n", id);
		goto out;
	}

	if (common->state != WIDGET_STATE_CREATE) {
		ErrPrint("WIDGET(%s) is not created yet\n", id);
		goto out;
	}

	if (!common->widget.extra_buffer && conf_extra_buffer_count()) {
		common->widget.extra_buffer = calloc(conf_extra_buffer_count(), sizeof(*common->widget.extra_buffer));
		if (!common->widget.extra_buffer) {
			ErrPrint("WIDGET(%s) calloc: %d\n", id, errno);
			goto out;
		}
	}

	common->widget.last_extra_buffer_idx = idx;
	if (common->widget.extra_buffer[idx] != pixmap) {
		DbgPrint("Extra buffer Pixmap is not matched %u <> %u\n", common->widget.extra_buffer[idx], pixmap);
	}

	dlist_foreach_safe(common->widget_list, l, n, handler) {
		_widget_invoke_event_handler(handler, WIDGET_EVENT_WIDGET_EXTRA_BUFFER_DESTROYED);
	}
out:
	return NULL;
}

static struct packet *master_widget_extra_buffer_created(pid_t pid, int handle, const struct packet *packet)
{
	widget_h handler;
	struct widget_common *common;
	const char *pkgname;
	struct dlist *l;
	struct dlist *n;
	const char *id;
	int idx;
	int pixmap;
	int ret;

	if (!packet) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	ret = packet_get(packet, "ssii", &pkgname, &id, &pixmap, &idx);
	if (ret != 4) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	if (idx < 0 || idx >= conf_extra_buffer_count()) {
		ErrPrint("Extra buffer count is not matched\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("WIDGET(%s) is not found\n", id);
		goto out;
	}

	if (common->state != WIDGET_STATE_CREATE) {
		ErrPrint("WIDGET(%s) is not created yet\n", id);
		goto out;
	}

	if (!common->widget.extra_buffer && conf_extra_buffer_count()) {
		common->widget.extra_buffer = calloc(conf_extra_buffer_count(), sizeof(*common->widget.extra_buffer));
		if (!common->widget.extra_buffer) {
			ErrPrint("WIDGET(%s) calloc: %d\n", id, errno);
			goto out;
		}
	}

	common->widget.last_extra_buffer_idx = idx;
	common->widget.extra_buffer[idx] = pixmap;

	dlist_foreach_safe(common->widget_list, l, n, handler) {
		_widget_invoke_event_handler(handler, WIDGET_EVENT_WIDGET_EXTRA_BUFFER_CREATED);
	}
out:
	return NULL;
}

static struct packet *master_gbar_extra_buffer_created(pid_t pid, int handle, const struct packet *packet)
{
	widget_h handler;
	struct widget_common *common;
	const char *pkgname;
	struct dlist *l;
	struct dlist *n;
	const char *id;
	int pixmap;
	int idx;
	int ret;

	if (!packet) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	ret = packet_get(packet, "ssii", &pkgname, &id, &pixmap, &idx);
	if (ret != 4) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	if (idx < 0 || idx >= conf_extra_buffer_count()) {
		ErrPrint("Extra buffer count is not matched\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("WIDGET(%s) is not found\n", id);
		goto out;
	}

	if (common->state != WIDGET_STATE_CREATE) {
		ErrPrint("WIDGET(%s) is not created yet\n", id);
		goto out;
	}

	if (!common->gbar.extra_buffer && conf_extra_buffer_count()) {
		common->gbar.extra_buffer = calloc(conf_extra_buffer_count(), sizeof(*common->gbar.extra_buffer));
		if (!common->gbar.extra_buffer) {
			ErrPrint("WIDGET(%s) calloc: %d\n", id, errno);
			goto out;
		}
	}

	common->gbar.last_extra_buffer_idx = idx;
	common->gbar.extra_buffer[idx] = pixmap;

	dlist_foreach_safe(common->widget_list, l, n, handler) {
		_widget_invoke_event_handler(handler, WIDGET_EVENT_GBAR_EXTRA_BUFFER_CREATED);
	}
out:
	return NULL;
}

static struct packet *master_update_mode(pid_t pid, int handle, const struct packet *packet)
{
	widget_h handler;
	struct widget_common *common;
	struct dlist *l;
	struct dlist *n;
	const char *pkgname;
	const char *id;
	int active_mode;
	int status;
	int ret;

	if (!packet) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	ret = packet_get(packet, "ssii", &pkgname, &id, &status, &active_mode);
	if (ret != 4) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("WIDGET(%s) is not found\n", id);
		goto out;
	}

	if (common->state != WIDGET_STATE_CREATE) {
		ErrPrint("WIDGET(%s) is not created yet\n", id);
		goto out;
	}

	if (status == (int)WIDGET_ERROR_NONE) {
		_widget_set_update_mode(common, active_mode);
	}

	common->request.update_mode = 0;
	dlist_foreach_safe(common->widget_list, l, n, handler) {
		if (handler->cbs.update_mode.cb) {
			widget_ret_cb cb;
			void *cbdata;

			cb = handler->cbs.update_mode.cb;
			cbdata = handler->cbs.update_mode.data;

			handler->cbs.update_mode.cb = NULL;
			handler->cbs.update_mode.data = NULL;

			cb(handler, status, cbdata);
		} else if (status == (int)WIDGET_ERROR_NONE) {
			_widget_invoke_event_handler(handler, WIDGET_EVENT_UPDATE_MODE_CHANGED);
		}
	}

out:
	return NULL;
}

static struct packet *master_size_changed(pid_t pid, int handle, const struct packet *packet)
{
	widget_h handler;
	struct widget_common *common;
	const char *pkgname;
	const char *id;
	const char *fbfile;
	int status;
	int ret;
	int w;
	int h;
	int is_gbar;

	if (!packet) {
		ErrPrint("Invalid packet\n");
		goto out;
	}

	ret = packet_get(packet, "sssiiii", &pkgname, &id, &fbfile, &is_gbar, &w, &h, &status);
	if (ret != 7) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("WIDGET(%s) is not found\n", id);
		goto out;
	}

	if (common->state != WIDGET_STATE_CREATE) {
		ErrPrint("WIDGET(%s) is not created yet\n", id);
		goto out;
	}

	common->request.size_changed = 0;
	if (is_gbar) {
		/*!
		 * \NOTE
		 * GBAR is not able to resized by the client.
		 * GBAR is only can be managed by the provider.
		 * So the GBAR has no private resized event handler.
		 * Notify it via global event handler only.
		 */
		if (status == (int)WIDGET_ERROR_NONE) {
			struct dlist *l;

			_widget_set_gbarsize(common, w, h);
			dlist_foreach(common->widget_list, l, handler) {
				_widget_invoke_event_handler(handler, WIDGET_EVENT_GBAR_SIZE_CHANGED);
			}
		} else {
			ErrPrint("This is not possible. GBAR Size is changed but the return value is not ZERO (%d)\n", status);
		}
	} else {
		struct dlist *l;
		struct dlist *n;

		if (status == (int)WIDGET_ERROR_NONE) {
			_widget_set_size(common, w, h);

			/*!
			 * \NOTE
			 * If there is a created WIDGET FB,
			 * Update it too.
			 */
			if (_widget_get_widget_fb(common)) {
				(void)_widget_set_widget_fb(common, fbfile);

				ret = _widget_sync_widget_fb(common);
				if (ret < 0) {
					ErrPrint("Failed to do sync FB (%s - %s)\n", pkgname, widget_util_basename(util_uri_to_path(id)));
				}

				/* Just update the size info only. */
			}
		}

		/*!
		 * \NOTE
		 * I cannot believe client.
		 * So I added some log before & after call the user callback.
		 */
		dlist_foreach_safe(common->widget_list, l, n, handler) {
			if (handler->cbs.size_changed.cb) {
				widget_ret_cb cb;
				void *cbdata;

				cb = handler->cbs.size_changed.cb;
				cbdata = handler->cbs.size_changed.data;

				handler->cbs.size_changed.cb = NULL;
				handler->cbs.size_changed.data = NULL;

				cb(handler, status, cbdata);
			} else if (status == (int)WIDGET_ERROR_NONE) {
				_widget_invoke_event_handler(handler, WIDGET_EVENT_WIDGET_SIZE_CHANGED);
			}
		}
	}

out:
	return NULL;
}

static struct packet *master_period_changed(pid_t pid, int handle, const struct packet *packet)
{
	widget_h handler;
	struct widget_common *common;
	struct dlist *l;
	struct dlist *n;
	const char *pkgname;
	const char *id;
	int ret;
	double period;
	int status;

	ret = packet_get(packet, "idss", &status, &period, &pkgname, &id);
	if (ret != 4) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("WIDGET(%s) is not found\n", id);
		goto out;
	}

	if (common->state != WIDGET_STATE_CREATE) {
		ErrPrint("WIDGET(%s) is not created\n", id);
		goto out;
	}

	if (status == (int)WIDGET_ERROR_NONE) {
		_widget_set_period(common, period);
	}

	common->request.period_changed = 0;

	dlist_foreach_safe(common->widget_list, l, n, handler) {
		if (handler->cbs.period_changed.cb) {
			widget_ret_cb cb;
			void *cbdata;

			cb = handler->cbs.period_changed.cb;
			cbdata = handler->cbs.period_changed.data;

			handler->cbs.period_changed.cb = NULL;
			handler->cbs.period_changed.data = NULL;

			cb(handler, status, cbdata);
		} else if (status == (int)WIDGET_ERROR_NONE) {
			_widget_invoke_event_handler(handler, WIDGET_EVENT_PERIOD_CHANGED);
		}
	}

out:
	return NULL;
}

static struct packet *master_group_changed(pid_t pid, int handle, const struct packet *packet)
{
	widget_h handler;
	struct widget_common *common;
	struct dlist *l;
	struct dlist *n;
	const char *pkgname;
	const char *id;
	int ret;
	const char *cluster;
	const char *category;
	int status;

	ret = packet_get(packet, "ssiss", &pkgname, &id, &status, &cluster, &category);
	if (ret != 5) {
		ErrPrint("Invalid argument\n");
		goto out;
	}

	common = _widget_find_common_handle(pkgname, id);
	if (!common) {
		ErrPrint("WIDGET(%s) is not exists\n", id);
		goto out;
	}

	if (common->state != WIDGET_STATE_CREATE) {
		/*!
		 * \note
		 * Do no access this handler,
		 * You cannot believe this handler anymore.
		 */
		ErrPrint("WIDGET(%s) is not created\n", id);
		goto out;
	}

	if (status == (int)WIDGET_ERROR_NONE) {
		(void)_widget_set_group(common, cluster, category);
	}

	common->request.group_changed = 0;

	dlist_foreach_safe(common->widget_list, l, n, handler) {
		if (handler->cbs.group_changed.cb) {
			widget_ret_cb cb;
			void *cbdata;

			cb = handler->cbs.group_changed.cb;
			cbdata = handler->cbs.group_changed.data;

			handler->cbs.group_changed.cb = NULL;
			handler->cbs.group_changed.data = NULL;

			cb(handler, status, cbdata);
		} else if (status == (int)WIDGET_ERROR_NONE) {
			_widget_invoke_event_handler(handler, WIDGET_EVENT_GROUP_CHANGED);
		}
	}

out:
	return NULL;
}

static struct packet *master_update_id(pid_t pid, int handle, const struct packet *packet)
{
	int ret;
	double timestamp;
	const char *id;
	struct widget_common *common;

	if (!packet) {
		ErrPrint("Invalid packet\n");
		return NULL;
	}

	ret = packet_get(packet, "ds", &timestamp, &id);
	if (ret != 2) {
		ErrPrint("Invalid paramter\n");
		return NULL;
	}

	common = _widget_find_common_handle_by_timestamp(timestamp);
	if (!common) {
		ErrPrint("Handle is not found for %d\n", timestamp);
		return NULL;
	}

	_widget_set_id(common, id);
	DbgPrint("Update ID(%s) for %lf\n", id, timestamp);
	return NULL;
}

static struct packet *master_created(pid_t pid, int handle, const struct packet *packet)
{
	widget_h handler;
	struct widget_common *common;
	struct dlist *l;

	int widget_w;
	int widget_h;
	int gbar_w;
	int gbar_h;
	const char *pkgname;
	const char *id;

	const char *content;
	const char *cluster;
	const char *category;
	const char *widget_fname;
	const char *gbar_fname;
	const char *title;

	double timestamp;
	const char *auto_launch;
	double priority;
	int size_list;
	int user;
	int pinup_supported;
	widget_widget_type_e widget_type;
	widget_gbar_type_e gbar_type;
	double period;
	int is_pinned_up;

	int old_state = WIDGET_STATE_DESTROYED;

	int ret;

	ret = packet_get(packet, "dsssiiiisssssdiiiiidsi",
			&timestamp,
			&pkgname, &id, &content,
			&widget_w, &widget_h, &gbar_w, &gbar_h,
			&cluster, &category, &widget_fname, &gbar_fname,
			&auto_launch, &priority, &size_list, &user, &pinup_supported,
			&widget_type, &gbar_type, &period, &title, &is_pinned_up);
	if (ret != 22) {
		ErrPrint("Invalid argument\n");
		ret = WIDGET_ERROR_INVALID_PARAMETER;
		goto out;
	}

	ErrPrint("[%lf] pkgname: %s, id: %s, content: %s, "
			"gbar_w: %d, gbar_h: %d, widget_w: %d, widget_h: %d, "
			"cluster: %s, category: %s, widget_fname: \"%s\", gbar_fname: \"%s\", "
			"auto_launch: %s, priority: %lf, size_list: %d, user: %d, pinup: %d, "
			"widget_type: %d, gbar_type: %d, period: %lf, title: [%s], is_pinned_up: %d\n",
			timestamp, pkgname, id, content,
			gbar_w, gbar_h, widget_w, widget_h,
			cluster, category, widget_fname, gbar_fname,
			auto_launch, priority, size_list, user, pinup_supported,
			widget_type, gbar_type, period, title, is_pinned_up);

	common = _widget_find_common_handle_by_timestamp(timestamp);
	if (!common) {
		handler = _widget_new_widget(pkgname, id, timestamp, cluster, category);
		if (!handler) {
			ErrPrint("Failed to create a new widget\n");
			ret = WIDGET_ERROR_FAULT;
			goto out;
		}
		common = handler->common;
		old_state = common->state;
	} else {
		if (common->state != WIDGET_STATE_CREATE) {
			if (common->state != WIDGET_STATE_DELETE) {
				/*!
				 * \note
				 * This is not possible!!!
				 */
				ErrPrint("Invalid handler\n");
				ret = WIDGET_ERROR_INVALID_PARAMETER;
				goto out;
			}

			/*!
			 * \note
			 * After get the delete states,
			 * call the create callback with deleted result.
			 */
		}

		old_state = common->state;

		if (common->id && common->request.created == 0) {
			ErrPrint("Already created: timestamp[%lf] "
					"pkgname[%s], id[%s] content[%s] "
					"cluster[%s] category[%s] widget_fname[%s] gbar_fname[%s]\n",
					timestamp, pkgname, id,
					content, cluster, category,
					widget_fname, gbar_fname);

			ret = WIDGET_ERROR_ALREADY_EXIST;
			goto out;
		}

		_widget_set_id(common, id);
	}

	common->request.created = 0;
	_widget_set_size(common, widget_w, widget_h);
	common->widget.type = widget_type;
	common->is_pinned_up = is_pinned_up;

	switch (widget_type) {
	case WIDGET_TYPE_UIFW:
	case WIDGET_TYPE_FILE:
		break;
	case WIDGET_TYPE_SCRIPT:
	case WIDGET_TYPE_BUFFER:
		if (!strlen(widget_fname)) {
			break;
		}
		(void)_widget_set_widget_fb(common, widget_fname);

		/*!
		 * \note
		 * WIDGET should create the lock file from here.
		 * Even if the old_state == WIDGET_STATE_DELETE,
		 * the lock file will be deleted from deleted event callback.
		 */
		switch (fb_type(_widget_get_widget_fb(common))) {
		case WIDGET_FB_TYPE_FILE:
		case WIDGET_FB_TYPE_SHM:
			common->widget.lock = widget_service_create_lock(common->id, WIDGET_TYPE_WIDGET, WIDGET_LOCK_READ);
			break;
		case WIDGET_FB_TYPE_PIXMAP:
		case WIDGET_FB_TYPE_ERROR:
		default:
			break;
		}

		ret = _widget_sync_widget_fb(common);
		if (ret < 0) {
			ErrPrint("Failed to do sync FB (%s - %s)\n", pkgname, widget_util_basename(util_uri_to_path(id)));
		}
		break;
	case WIDGET_TYPE_TEXT:
		_widget_set_text_widget(common);
		break;
	default:
		break;
	}

	common->gbar.type = gbar_type;
	_widget_set_gbarsize(common, gbar_w, gbar_h);
	_widget_set_default_gbarsize(common, gbar_w, gbar_h);
	switch (gbar_type) {
	case GBAR_TYPE_SCRIPT:
	case GBAR_TYPE_BUFFER:
		if (!strlen(gbar_fname)) {
			break;
		}

		_widget_set_gbar_fb(common, gbar_fname);

		ret = _widget_sync_gbar_fb(common);
		if (ret < 0) {
			ErrPrint("Failed to do sync FB (%s - %s)\n", pkgname, widget_util_basename(util_uri_to_path(id)));
		}

		/*!
		 * \brief
		 * GBAR doesn't need to create the lock file from here.
		 * Just create it from GBAR_CREATED event.
		 */

		break;
	case GBAR_TYPE_TEXT:
		_widget_set_text_gbar(common);
		break;
	case GBAR_TYPE_UIFW:
	default:
		break;
	}

	_widget_set_priority(common, priority);

	_widget_set_size_list(common, size_list);
	_widget_set_group(common, cluster, category);

	_widget_set_content(common, content);
	_widget_set_title(common, title);

	_widget_set_user(common, user);

	_widget_set_auto_launch(common, auto_launch);
	_widget_set_pinup(common, pinup_supported);

	_widget_set_period(common, period);

	ret = 0;

	if (common->state == WIDGET_STATE_CREATE) {
		dlist_foreach(common->widget_list, l, handler) {
			/*!
			 * \note
			 * These callback can change the handler->state.
			 * So we have to use the "old_state" which stored state before call these callbacks
			 */

			if (handler->cbs.created.cb) {
				widget_ret_cb cb;
				void *cbdata;

				cb = handler->cbs.created.cb;
				cbdata = handler->cbs.created.data;

				handler->cbs.created.cb = NULL;
				handler->cbs.created.data = NULL;

				cb(handler, ret, cbdata);
			} else {
				_widget_invoke_event_handler(handler, WIDGET_EVENT_CREATED);
			}

			/**
			 * If there is any updates before get this event,
			 * Invoke all update event forcely
			 */
			switch (common->widget.last_extra_buffer_idx) {
			case WIDGET_UNKNOWN_BUFFER:
				break;
			case WIDGET_PRIMARY_BUFFER:
				DbgPrint("Primary buffer updated\n");
				_widget_invoke_event_handler(handler, WIDGET_EVENT_WIDGET_UPDATED);
				break;
			default:
				DbgPrint("Extra buffer updated\n");
				_widget_invoke_event_handler(handler, WIDGET_EVENT_WIDGET_EXTRA_UPDATED);
				break;
			}
		}
	}

out:
	if (ret == 0 && old_state == WIDGET_STATE_DELETE) {
		struct dlist *n;

		DbgPrint("Take place an unexpected case [%d]\n", common->refcnt);
		dlist_foreach_safe(common->widget_list, l, n, handler) {
			if (handler->cbs.created.cb) {
				if (!handler->common->request.deleted) {
					if (_widget_send_delete(handler, common->delete_type, handler->cbs.created.cb, handler->cbs.created.data) < 0) {
						/*!
						 * \note
						 * Already sent or something else happens.
						 * Callback will be called in any cases
						 */
					}
				} else if (handler->state != WIDGET_STATE_DELETE) {
					handler->cbs.created.cb(handler, WIDGET_ERROR_CANCELED, handler->cbs.created.data);
					_widget_unref(handler, 1);
				}
			} else {
				_widget_invoke_event_handler(handler, WIDGET_EVENT_DELETED);
				_widget_unref(handler, 1);
			}
		}

		/*!
		 * \note
		 * handler->cbs.created.cb = NULL;
		 * handler->cbs.created.data = NULL;
		 *
		 * Do not clear this to use this from the deleted event callback.
		 * if this value is not cleared when the deleted event callback check it,
		 * it means that the created function is not called yet.
		 * Then the call the deleted event callback with WIDGET_ERROR_CANCELED errno.
		 */
	}

	return NULL;
}

static struct method s_direct_table[] = {
	{
		.cmd = CMD_STR_WIDGET_UPDATED, /* pkgname, id, lb_w, lb_h, priority, ret */
		.handler = master_widget_updated,
	},
	{
		.cmd = CMD_STR_GBAR_UPDATED, /* pkgname, id, descfile, pd_w, pd_h, ret */
		.handler = master_gbar_updated,
	},
	{
		.cmd = CMD_STR_EXTRA_UPDATED,
		.handler = master_extra_updated,
	},
	{
		.cmd = NULL,
		.handler = NULL,
	},
};

static struct method s_table[] = {
	{ /* WIDGET_UPDATED */
		.cmd = CMD_STR_WIDGET_UPDATED, /* pkgname, id, widget_w, widget_h, priority, ret */
		.handler = master_widget_updated,
	},
	{ /* GBAR_UPDATED */
		.cmd = CMD_STR_GBAR_UPDATED, /* pkgname, id, descfile, pd_w, pd_h, ret */
		.handler = master_gbar_updated,
	},
	{ /* EXTRA_UPDATED */
		.cmd = CMD_STR_EXTRA_UPDATED,
		.handler = master_extra_updated,
	},
	{ /* EXTRA_INFO */
		.cmd = CMD_STR_EXTRA_INFO,
		.handler = master_extra_info,
	},
	{ /* DELETED */
		.cmd = CMD_STR_DELETED, /* pkgname, id, timestamp, ret */
		.handler = master_deleted,
	},
	{ /* FAULTED */
		.cmd = CMD_STR_FAULT_PACKAGE, /* pkgname, id, function, ret */
		.handler = master_fault_package,
	},
	{ /* SCROLL */
		.cmd = CMD_STR_SCROLL,
		.handler = master_hold_scroll,
	},
	{ /* WIDGET_UPDATE_BEGIN */
		.cmd = CMD_STR_WIDGET_UPDATE_BEGIN,
		.handler = master_widget_update_begin,
	},
	{ /* WIDGET_UPDATE_END */
		.cmd = CMD_STR_WIDGET_UPDATE_END,
		.handler = master_widget_update_end,
	},
	{ /* GBAR_UPDATE_BEGIN */
		.cmd = CMD_STR_GBAR_UPDATE_BEGIN,
		.handler = master_gbar_update_begin,
	},
	{ /* GBAR_UPDATE_END */
		.cmd = CMD_STR_GBAR_UPDATE_END,
		.handler = master_gbar_update_end,
	},
	{ /* ACCESS_STATUS */
		.cmd = CMD_STR_ACCESS_STATUS,
		.handler = master_access_status,
	},
	{ /* KEY_STATUS */
		.cmd = CMD_STR_KEY_STATUS,
		.handler = master_key_status,
	},
	{ /* CLOSE_GBAR */
		.cmd = CMD_STR_CLOSE_GBAR,
		.handler = master_request_close_gbar,
	},
	{ /* GBAR_CREATED */
		.cmd = CMD_STR_GBAR_CREATED,
		.handler = master_gbar_created,
	},
	{ /* GBAR_DESTROYED */
		.cmd = CMD_STR_GBAR_DESTROYED,
		.handler = master_gbar_destroyed,
	},
	{ /* CREATED */
		.cmd = CMD_STR_CREATED,
		.handler = master_created,
	},
	{ /* GROUP_CHANGED */
		.cmd = CMD_STR_GROUP_CHANGED,
		.handler = master_group_changed,
	},
	{ /* PERIOD_CHANGED */
		.cmd = CMD_STR_PERIOD_CHANGED,
		.handler = master_period_changed,
	},
	{ /* SIZE_CHANGED */
		.cmd = CMD_STR_SIZE_CHANGED,
		.handler = master_size_changed,
	},
	{ /* PINUP */
		.cmd = CMD_STR_PINUP,
		.handler = master_pinup,
	},
	{ /* UPDATE_MODE */
		.cmd = CMD_STR_UPDATE_MODE,
		.handler = master_update_mode,
	},
	{ /* WIDGET_CREATE_XBUF */
		.cmd = CMD_STR_WIDGET_CREATE_XBUF,
		.handler = master_widget_extra_buffer_created,
	},
	{ /* GBAR_CREATE_XBUF */
		.cmd = CMD_STR_GBAR_CREATE_XBUF,
		.handler = master_gbar_extra_buffer_created,
	},
	{ /* WIDGET_DESTROY_XBUF */
		.cmd = CMD_STR_WIDGET_DESTROY_XBUF,
		.handler = master_widget_extra_buffer_destroyed,
	},
	{ /* GBAR_DESTROY_XBUF */
		.cmd = CMD_STR_GBAR_DESTROY_XBUF,
		.handler = master_gbar_extra_buffer_destroyed,
	},
	{ /* UPDATE_ID */
		.cmd = CMD_STR_UPDATE_ID,
		.handler = master_update_id,
	},
	{
		.cmd = NULL,
		.handler = NULL,
	},
};

static void make_direct_connection(void)
{
	struct packet *packet;
	int ret;

	s_info.master_direct_fd = com_core_packet_client_init(SHARED_SOCKET, 0, s_direct_table);
	if (s_info.master_direct_fd < 0) {
		ErrPrint("Failed to create a connection\n");
		return;
	}

	packet = packet_create_noack(CMD_STR_DIRECT_HELLO, "s", client_direct_addr());
	if (!packet) {
		ErrPrint("Packet is not valid\n");
		return;
	}

	ret = com_core_packet_send_only(s_info.master_direct_fd, packet);
	packet_destroy(packet);

	DbgPrint("Direct connection request is sent: %d\n", ret);
}

static void acquire_cb(widget_h handler, const struct packet *result, void *data)
{
	if (!result) {
		DbgPrint("Result packet is not valid\n");
	} else {
		int ret;
		int extra_buffer_count;

		if (packet_get(result, "ii", &ret, &extra_buffer_count) != 2) {
			ErrPrint("Invalid argument\n");
		} else {
			DbgPrint("Acquire returns: %d (%d)\n", ret, extra_buffer_count);
			conf_set_extra_buffer_count(extra_buffer_count);
			/**
			 * Now the master has client object.
			 * We can make a direct connection handler from now.
			 */
			make_direct_connection();
		}
	}

	return;
}

static void prepare_direct_update(void)
{
	char path[MAX_DIRECT_ADDR];

	if (!conf_direct_update()) {
		return;
	}

	if (s_info.direct_addr) {
		DbgPrint("Direct path is already initiated: %s\n", s_info.direct_addr);
		return;
	}

	if (!strncmp(s_info.client_addr, COM_CORE_REMOTE_SCHEME, strlen(COM_CORE_REMOTE_SCHEME))) {
		ErrPrint("Remote model is not support this\n");
		return;
	}

	snprintf(path, sizeof(path) - 1, "%s/.%d.%lf.widget.viewer", WIDGET_CONF_IMAGE_PATH, getpid(), util_timestamp());

	s_info.direct_addr = strdup(path);
	if (!s_info.direct_addr) {
		ErrPrint("strdup: %d\n", errno);
		return;
	}

	s_info.direct_fd = com_core_packet_server_init(client_direct_addr(), s_direct_table);
	if (s_info.direct_fd < 0) {
		ErrPrint("Failed to prepare server: %s\n", client_direct_addr());
		free(s_info.direct_addr);
		s_info.direct_addr = NULL;
		return;
	}

	DbgPrint("Direct update is prepared: %s - %d\n", client_direct_addr(), client_direct_fd());
}

static inline int make_connection(void)
{
	struct packet *packet;
	unsigned int cmd = CMD_ACQUIRE;
	int ret;

	/**
	 * @note
	 * Before creating a connection with master,
	 * Initiate the private channel for getting the updated event from providers
	 * And this channel should be created after the master is launched.
	 * Or the master will delete all files in the shared folder.
	 */
	prepare_direct_update();

	DbgPrint("Let's making connection!\n");

	s_info.fd = com_core_packet_client_init(client_addr(), 0, s_table);
	if (s_info.fd < 0) {
		ErrPrint("Try this again later\n");
		return WIDGET_ERROR_IO_ERROR;
	}

	packet = packet_create((const char *)&cmd, "ds", util_timestamp(), client_direct_addr());
	if (!packet) {
		com_core_packet_client_fini(s_info.fd);
		s_info.fd = -1;
		return WIDGET_ERROR_FAULT;
	}

	ret = master_rpc_async_request(NULL, packet, 1, acquire_cb, NULL);
	if (ret < 0) {
		ErrPrint("Master RPC returns %d\n", ret);
		com_core_packet_client_fini(s_info.fd);
		s_info.fd = -1;
		return WIDGET_ERROR_IO_ERROR;
	}

	return WIDGET_ERROR_NONE;
}

static int connected_cb(int handle, void *data)
{
	if (s_info.fd == handle) {
		master_rpc_check_and_fire_consumer();
	}
	return 0;
}

static void master_started_cb(keynode_t *node, void *data)
{
	int state = 0;

	if (vconf_get_bool(VCONFKEY_MASTER_STARTED, &state) < 0) {
		ErrPrint("Unable to get [%s]\n", VCONFKEY_MASTER_STARTED);
	}

	DbgPrint("Master state: %d\n", state);
	if (state == 1 && make_connection() == (int)WIDGET_ERROR_NONE) {
		int ret;
		ret = vconf_ignore_key_changed(VCONFKEY_MASTER_STARTED, master_started_cb);
		if (ret < 0) {
			DbgPrint("master_started vconf key de-registered [%d]\n", ret);
		}
	}
}

static gboolean timeout_cb(gpointer data)
{
	if (vconf_notify_key_changed(VCONFKEY_MASTER_STARTED, master_started_cb, NULL) < 0) {
		ErrPrint("Failed to add vconf for monitoring service state\n");
	} else {
		DbgPrint("vconf event callback is registered\n");
	}

	master_started_cb(NULL, NULL);

	s_info.timer_id = 0;
	return FALSE;
}

static int disconnected_cb(int handle, void *data)
{
	if (s_info.fd != handle) {
		/*!< This handle is not my favor */
		return 0;
	}

	s_info.fd = -1; /*!< Disconnected */

	master_rpc_clear_all_request();
	_widget_invoke_fault_handler(WIDGET_FAULT_PROVIDER_DISCONNECTED, MASTER_PKGNAME, "default", "disconnected");

	_widget_delete_all();

	/* Try to reconnect after 1 sec later */
	if (!s_info.timer_id) {
		DbgPrint("Reconnecting timer is added\n");
		s_info.timer_id = g_timeout_add(1000, timeout_cb, NULL);
		if (s_info.timer_id == 0) {
			ErrPrint("Unable to add reconnecting timer\n");
			return 0;
		}
	} else {
		ErrPrint("Reconnecting timer is already exists\n");
	}

	return 0;
}

int client_init(int use_thread)
{
	widget_conf_init();

	com_core_packet_use_thread(use_thread);

	s_info.client_addr = vconf_get_str(VCONFKEY_MASTER_CLIENT_ADDR);
	if (!s_info.client_addr) {
		s_info.client_addr = strdup(CLIENT_SOCKET);
		if (!s_info.client_addr) {
			ErrPrint("Heap: %d\n", errno);
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}
	}

	(void)file_service_init();

	DbgPrint("Server Address: %s\n", s_info.client_addr);

	com_core_add_event_callback(CONNECTOR_DISCONNECTED, disconnected_cb, NULL);
	com_core_add_event_callback(CONNECTOR_CONNECTED, connected_cb, NULL);

	if (vconf_notify_key_changed(VCONFKEY_MASTER_STARTED, master_started_cb, NULL) < 0) {
		ErrPrint("Failed to add vconf for service state\n");
	} else {
		DbgPrint("vconf event callback is registered\n");
	}

	master_started_cb(NULL, NULL);
	return WIDGET_ERROR_NONE;
}

int client_fd(void)
{
	return s_info.fd;
}

const char *client_addr(void)
{
	return s_info.client_addr;
}

const char *client_direct_addr(void)
{
	return s_info.direct_addr;
}

int client_direct_fd(void)
{
	return s_info.direct_fd;
}

int client_fini(void)
{
	int ret;

	(void)file_service_fini();

	ret = vconf_ignore_key_changed(VCONFKEY_MASTER_STARTED, master_started_cb);
	if (ret < 0) {
		DbgPrint("Ignore vconf key: %d\n", ret);
	}

	com_core_del_event_callback(CONNECTOR_DISCONNECTED, disconnected_cb, NULL);
	com_core_del_event_callback(CONNECTOR_CONNECTED, connected_cb, NULL);

	if (s_info.fd >= 0) {
		com_core_packet_client_fini(s_info.fd);
		s_info.fd = -1;
	}

	if (s_info.master_direct_fd >= 0) {
		com_core_packet_client_fini(s_info.master_direct_fd);
		s_info.master_direct_fd = -1;
	}

	if (s_info.direct_fd >= 0) {
		com_core_packet_server_fini(s_info.direct_fd);
		s_info.direct_fd = -1;
	}

	if (s_info.timer_id > 0) {
		g_source_remove(s_info.timer_id);
		s_info.timer_id = 0;
	}

	free(s_info.client_addr);
	s_info.client_addr = NULL;

	free(s_info.direct_addr);
	s_info.direct_addr = NULL;

	widget_conf_reset();
	return WIDGET_ERROR_NONE;
}

/* End of a file */
