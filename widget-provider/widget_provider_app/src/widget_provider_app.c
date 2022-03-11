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
#include <string.h>
#include <stdbool.h>

#include <app.h>
#include <dlog.h>

#include <widget_conf.h>
#include <widget_errno.h>

#include <Ecore.h>
#include <widget_provider.h>

#include "widget_provider_buffer.h"
#include "widget_provider_app.h"
#include "widget_provider_app_internal.h"

#include "client.h"
#include "util.h"
#include "debug.h"

int errno;

static struct info {
	char *pkgname;
} s_info = {
	.pkgname = NULL,
};

const char *widget_provider_app_pkgname(void)
{
	return s_info.pkgname;
}

PAPI bool widget_provider_app_is_initialized(void)
{
	return client_is_initialized();
}

PAPI int widget_provider_app_init_sync(widget_provider_event_callback_s table)
{
	char *pkgname;
	int ret;

	if (app_get_id(&pkgname) != APP_ERROR_NONE || pkgname == NULL) {
		ErrPrint("Failed to get app package name\n");
		return WIDGET_ERROR_FAULT;
	}

	if (s_info.pkgname) {
		free(s_info.pkgname);
		s_info.pkgname = NULL;
	}

	s_info.pkgname = pkgname;

	ret = client_init_sync(table);
	if (ret < 0) {
		ErrPrint("Client init returns: 0x%X\n", ret);
		free(pkgname);
		s_info.pkgname = NULL;
		return ret;
	}

	return WIDGET_ERROR_NONE;
}

PAPI int widget_provider_app_init(app_control_h service, widget_provider_event_callback_s table)
{
	int ret;
	char *pkgname;

	if (!service) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	if (app_get_id(&pkgname) != APP_ERROR_NONE || pkgname == NULL) {
		ErrPrint("Failed to get app package name\n");
		return WIDGET_ERROR_FAULT;
	}

	DbgPrint("Package: %s\n", pkgname);
	if (s_info.pkgname) {
		free(s_info.pkgname);
		s_info.pkgname = NULL;
	}

	s_info.pkgname = pkgname;

	ret = client_init(service, table);
	if (ret < 0) {
		ErrPrint("Client init returns: 0x%X\n", ret);
		free(s_info.pkgname);
		s_info.pkgname = NULL;
		return ret;
	}

	return WIDGET_ERROR_NONE;
}

PAPI int widget_provider_app_set_control_mode(int flag)
{
	if (flag) {
		flag = WIDGET_PROVIDER_CTRL_MANUAL_REACTIVATION | WIDGET_PROVIDER_CTRL_MANUAL_TERMINATION;
	}

	return widget_provider_control(flag);
}

PAPI widget_buffer_h widget_provider_app_create_buffer(const char *id, int for_pd, int (*widget_handler)(widget_buffer_h , widget_buffer_event_data_t, void *), void *data)
{
	widget_buffer_h handle;
	char *uri;

	uri = util_path_to_uri(id);
	if (!uri) {
		ErrPrint("Failed to conver uri from id\n");
		return NULL;
	}

	handle = widget_provider_buffer_create(for_pd ? WIDGET_TYPE_GBAR : WIDGET_TYPE_WIDGET, s_info.pkgname, uri, WIDGET_CONF_AUTO_ALIGN, widget_handler, data);
	free(uri);
	if (!handle) {
		ErrPrint("Unable to create buffer (%s)\n", id);
		return NULL;
	}

	return handle;
}

PAPI int widget_provider_app_acquire_buffer(widget_buffer_h handle, int w, int h, int bpp)
{
	return widget_provider_buffer_acquire(handle, w, h, bpp);
}

PAPI int widget_provider_app_acquire_extra_buffer(widget_buffer_h handle, int idx, int w, int h, int bpp)
{
	return widget_provider_buffer_extra_acquire(handle, idx, w, h, bpp);
}

PAPI int widget_provider_app_release_extra_buffer(widget_buffer_h handle, int idx)
{
	return widget_provider_buffer_extra_release(handle, idx);
}

PAPI int widget_provider_app_release_buffer(widget_buffer_h handle)
{
	return widget_provider_buffer_release(handle);
}

PAPI int widget_provider_app_destroy_buffer(widget_buffer_h handle)
{
	return widget_provider_buffer_destroy(handle);
}

PAPI int widget_provider_app_send_access_status(widget_buffer_h handle, int status)
{
	return widget_provider_send_access_status(widget_provider_buffer_pkgname(handle), widget_provider_buffer_id(handle), status);
}

PAPI int widget_provider_app_send_key_status(widget_buffer_h handle, int status)
{
	return widget_provider_send_key_status(widget_provider_buffer_pkgname(handle), widget_provider_buffer_id(handle), status);
}

PAPI int widget_provider_app_send_deleted(const char *id)
{
	char *uri;
	int ret;

	uri = util_path_to_uri(id);
	if (!uri) {
		ErrPrint("Failed to convert uri from id\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	ret = widget_provider_send_deleted(s_info.pkgname, uri);
	free(uri);
	return ret;
}

PAPI unsigned int widget_provider_app_get_buffer_resource_id(widget_buffer_h handle)
{
	return widget_provider_buffer_resource_id(handle);
}

PAPI unsigned int widget_provider_app_get_extra_buffer_resource_id(widget_buffer_h handle, int idx)
{
	return widget_provider_buffer_extra_resource_id(handle, idx);
}

PAPI int widget_provider_app_hold_scroll(const char *id, int seize)
{
	char *uri;
	int ret;

	uri = util_path_to_uri(id);
	if (!uri) {
		ErrPrint("Failed to convert uri from id\n");
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	ret = widget_provider_send_hold_scroll(s_info.pkgname, uri, seize);
	free(uri);
	return ret;
}

PAPI void widget_provider_app_fini(void)
{
	client_fini();
	free(s_info.pkgname);
	s_info.pkgname = NULL;
}

/* End of a file */
