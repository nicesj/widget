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
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <pthread.h>

#include <Eina.h>
#include <Ecore.h>

#include <dlog.h>
#include <widget_errno.h>
#include <widget_service.h>

#include "util.h"
#include "debug.h"

int util_check_ext(const char *icon, const char *ext)
{
	int len;

	len = strlen(icon) - 1;
	while (len >= 0 && *ext && icon[len] == *ext) {
		len--;
		ext++;
	}

	return *ext ? 0 : 1;
}

int util_get_filesize(const char *filename)
{
	struct stat buf;

	if (stat(filename, &buf) < 0) {
		ErrPrint("stat: %d\n", errno);
		return WIDGET_ERROR_IO_ERROR;
	}

	if (!S_ISREG(buf.st_mode)) {
		ErrPrint("%s is not a file\n", filename);
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	return buf.st_size;
}

double util_timestamp(void)
{
#if defined(_USE_ECORE_TIME_GET)
	return ecore_time_get();
#else
	struct timeval tv;

	if (gettimeofday(&tv, NULL) < 0) {
		static unsigned long internal_count = 0;

		ErrPrint("gettimeofday: %d\n", errno);
		tv.tv_sec = internal_count++;
		tv.tv_usec = 0;
	}

	return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0f;
#endif
}

double util_time_delay_for_compensation(double period)
{
	unsigned long long curtime;
	unsigned long long _period;
	unsigned long long remain;
	struct timeval tv;
	double ret;

	if (period == 0.0f) {
		DbgPrint("Period is ZERO\n");
		return 0.0f;
	}

	if (gettimeofday(&tv, NULL) < 0) {
		ErrPrint("gettimeofday: %d\n", errno);
		return period;
	}

	curtime = (unsigned long long)tv.tv_sec * 1000000llu + (unsigned long long)tv.tv_usec;

	_period = (unsigned long long)(period * (double)1000000);
	if (_period == 0llu) {
		ErrPrint("%lf <> %llu\n", period, _period);
		return period;
	}

	remain = curtime % _period;

	ret = (double)remain / (double)1000000;
	return period - ret;
}

void *util_timer_add(double interval, Eina_Bool (*cb)(void *data), void *data)
{
	Ecore_Timer *timer;
	double delay;

	timer = ecore_timer_add(interval, cb, data);
	if (!timer) {
		return NULL;
	}

	delay = util_time_delay_for_compensation(interval) - interval;
	ecore_timer_delay(timer, delay);

	return timer;
}

void util_timer_interval_set(void *timer, double interval)
{
	double delay;
	ecore_timer_interval_set(timer, interval);
	ecore_timer_reset(timer);

	delay = util_time_delay_for_compensation(interval) - interval;
	ecore_timer_delay(timer, delay);
}

void util_timer_freeze(void *timer)
{
	ecore_timer_freeze(timer);
}

void util_timer_thaw(void *timer)
{
	ecore_timer_thaw(timer);
}

void util_timer_del(void *timer)
{
	ecore_timer_del(timer);
}

char *util_path_to_uri(const char *id)
{
	char *uri;

	if (!id) {
		uri = strdup("");
		if (!uri) {
			ErrPrint("strdup: %d\n", errno);
			return NULL;
		}
	} else if (!strncmp(id, SCHEMA_FILE, strlen(SCHEMA_FILE))) {
		uri = strdup(id);
		if (!uri) {
			ErrPrint("strdup: %d\n", errno);
			return NULL;
		}
	} else if (!strncmp(id, SCHEMA_INTERNAL, strlen(SCHEMA_INTERNAL))) {
		uri = strdup(id);
		if (!uri) {
			ErrPrint("strdup: %d\n", errno);
			return NULL;
		}
	} else {
		int len;
		len = strlen(SCHEMA_FILE) + strlen(id) + 2;
		uri = malloc(len);
		if (!uri) {
			ErrPrint("malloc: %d\n", errno);
			return NULL;
		}

		snprintf(uri, len, SCHEMA_FILE"%s", id);
	}

	return uri;
}

/* End of a file */
