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
#include <Eina.h> /* for util.h */

#include <widget_errno.h>
#include <widget_service.h>

#include "util.h"

int util_screen_size_get(int *width, int *height)
{
	*width = 0;
	*height = 0;
	return WIDGET_ERROR_NOT_SUPPORTED;
}

int util_screen_init(void)
{
	return WIDGET_ERROR_NONE;
}

int util_screen_fini(void)
{
	return WIDGET_ERROR_NONE;
}

void *util_screen_get(void)
{
	return NULL;
}

/* End of a file */
