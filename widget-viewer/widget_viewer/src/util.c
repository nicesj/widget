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
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#include <dlog.h>
#include <widget_errno.h> /* For error code */

#include "debug.h"
#include "util.h"

int errno;
#if defined(_USE_ECORE_TIME_GET)
static struct {
	clockid_t type;
} s_info = {
	.type = CLOCK_MONOTONIC,
};
#endif

int util_check_extension(const char *filename, const char *check_ptr)
{
	int name_len;

	name_len = strlen(filename);
	while (--name_len >= 0 && *check_ptr) {
		if (filename[name_len] != *check_ptr) {
			return WIDGET_ERROR_INVALID_PARAMETER;
		}

		check_ptr++;
	}

	return 0;
}

double util_timestamp(void)
{
#if defined(_USE_ECORE_TIME_GET)
	struct timespec ts;

	do {
		if (clock_gettime(s_info.type, &ts) == 0) {
			return ts.tv_sec + ts.tv_nsec / 1000000000.0f;
		}

		ErrPrint("%d: %d\n", s_info.type, errno);
		if (s_info.type == CLOCK_MONOTONIC) {
			s_info.type = CLOCK_REALTIME;
		} else if (s_info.type == CLOCK_REALTIME) {
			struct timeval tv;
			if (gettimeofday(&tv, NULL) < 0) {
				ErrPrint("gettimeofday: %d\n", errno);
				break;
			}

			return tv.tv_sec + tv.tv_usec / 1000000.0f;
		}
	} while (1);

	return 0.0f;
#else
	struct timeval tv;

	if (gettimeofday(&tv, NULL) < 0) {
		ErrPrint("gettimeofday: %d\n", errno);
		tv.tv_sec = 0;
		tv.tv_usec = 0;
	}

	return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0f;
#endif
}

const char *util_uri_to_path(const char *uri)
{
	int len;

	len = strlen(SCHEMA_FILE);
	if (strncasecmp(uri, SCHEMA_FILE, len)) {
		return NULL;
	}

	return uri + len;
}

int util_unlink(const char *filename)
{
	char *descfile;
	int desclen;
	int ret;

	if (!filename) {
		return WIDGET_ERROR_INVALID_PARAMETER;
	}

	desclen = strlen(filename) + 6; /* .desc */
	descfile = malloc(desclen);
	if (!descfile) {
		ErrPrint("Heap: %d\n", errno);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	ret = snprintf(descfile, desclen, "%s.desc", filename);
	if (ret < 0) {
		ErrPrint("Error: %d\n", errno);
		free(descfile);
		return WIDGET_ERROR_FAULT;
	}

	(void)unlink(descfile);
	free(descfile);
	(void)unlink(filename);

	return WIDGET_ERROR_NONE;
}

/* End of a file */
