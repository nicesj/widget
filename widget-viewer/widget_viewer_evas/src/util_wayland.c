#include <stdio.h>
#include <Evas.h>

#include <widget_errno.h>
#include <widget_viewer.h>

int util_screen_size_get(int *x, int *y)
{
	*x = 0;
	*y = 0;

	return 0;
}

unsigned int util_replace_native_surface(struct widget *handle, int gbar, Evas_Object *content, unsigned int pixmap)
{
	return 0u;
}

int util_set_native_surface(struct widget *handle, int gbar, Evas_Object *content, unsigned int pixmap, int skip_acquire)
{
	return 1;
}

unsigned int util_get_resource_id_of_native_surface(Evas_Native_Surface *surface)
{
	return 0u;
}

void *util_display_get(void)
{
	return NULL;
}

/* End of a file */
