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
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#include <dlog.h>

#include "util.h"
#include "debug.h"

int errno;
#if defined(_USE_ECORE_TIME_GET)
static struct {
	clockid_t type;
} s_info = {
	.type = CLOCK_MONOTONIC,
};
#endif

double util_timestamp(void)
{
#ifdef _USE_ECORE_TIME_GET
	struct timespec ts;

	do {
		if (clock_gettime(s_info.type, &ts) == 0) {
			return ts.tv_sec + ts.tv_nsec / 1000000000.0f;
		}

		ErrPrint("clock_gettime: %d: %d\n", s_info.type, errno);
		if (s_info.type == CLOCK_MONOTONIC) {
			s_info.type = CLOCK_REALTIME;
		} else if (s_info.type == CLOCK_REALTIME) {
			struct timeval tv;

			if (gettimeofday(&tv, NULL) < 0) {
				ErrPrint("gettimeofday: %d\n", errno);
				break;
			}

			return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0f;
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

/* End of a file */
