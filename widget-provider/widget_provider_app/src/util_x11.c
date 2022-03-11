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
#include <Ecore_X.h>

#include <widget_errno.h>
#include "util.h"

int util_screen_size_get(int *width, int *height)
{
	ecore_x_window_size_get(0, width, height);
	return WIDGET_ERROR_NONE;
}

int util_screen_init(void)
{
	return ecore_x_init(NULL);
}

int util_screen_fini(void)
{
	return ecore_x_shutdown();
}

void *util_screen_get(void)
{
	return ecore_x_display_get();
}

/* End of a file */
