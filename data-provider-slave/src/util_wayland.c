#include <stdio.h>
#include <Eina.h>

#include <widget_errno.h>

#include "util.h"

void *util_screen_get(void)
{
	return NULL;
}

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

/* End of a file */
