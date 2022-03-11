#include <stdio.h>
#include <Evas.h>

#include <Ecore_X.h>

#include <dlog.h>

#include <widget_errno.h>
#include <widget_viewer.h>

#include "debug.h"

int util_screen_size_get(int *w, int *h)
{
	int _w;
	int _h;

	if (!w) {
		w = &_w;
	}

	if (!h) {
		h = &_h;
	}

	ecore_x_window_size_get(0, w, h);
	return 0;
}

unsigned int util_replace_native_surface(struct widget *handle, int gbar, Evas_Object *content, unsigned int pixmap)
{
	Evas_Native_Surface *old_surface;
	Evas_Native_Surface surface;
	unsigned int old_pixmap = 0u;

	surface.version = EVAS_NATIVE_SURFACE_VERSION;
	surface.type = EVAS_NATIVE_SURFACE_X11;
	surface.data.x11.pixmap = pixmap;

	old_surface = evas_object_image_native_surface_get(content);
	if (!old_surface) {
		surface.data.x11.visual = ecore_x_default_visual_get(ecore_x_display_get(), ecore_x_default_screen_get());

		evas_object_image_native_surface_set(content, &surface);

		DbgPrint("Created: %u\n", surface.data.x11.pixmap);
	} else {
		old_pixmap = old_surface->data.x11.pixmap;

		if (old_pixmap != pixmap) {
			surface.data.x11.visual = old_surface->data.x11.visual;
			evas_object_image_native_surface_set(content, &surface);
			DbgPrint("Replaced: %u (%u)\n", pixmap, old_pixmap);
		} else {
			DbgPrint("Same resource, reuse it [%u]\n", pixmap);
			old_pixmap = 0u;
		}
	}

	return old_pixmap;
}

int util_set_native_surface(struct widget *handle, int gbar, Evas_Object *content, unsigned int pixmap, int skip_acquire)
{
	Evas_Native_Surface *old_surface;
	Evas_Native_Surface surface;
	int is_first = 0;

	surface.version = EVAS_NATIVE_SURFACE_VERSION;
	surface.type = EVAS_NATIVE_SURFACE_X11;
	surface.data.x11.pixmap = (unsigned int)pixmap;

	old_surface = evas_object_image_native_surface_get(content);
	if (!old_surface) {
		surface.data.x11.visual = ecore_x_default_visual_get(ecore_x_display_get(), ecore_x_default_screen_get());
		evas_object_image_native_surface_set(content, &surface);

		is_first = 1;
	} else {
		unsigned int old_pixmap = 0u;
		old_pixmap = old_surface->data.x11.pixmap;
		surface.data.x11.visual = old_surface->data.x11.visual;
		evas_object_image_native_surface_set(content, &surface);

		if (old_pixmap) {
			if (!skip_acquire) {
				widget_viewer_release_resource_id(handle, gbar, old_pixmap);
			}
		}

		is_first = 0;
	}

	return is_first;
}

unsigned int util_get_resource_id_of_native_surface(Evas_Native_Surface *surface)
{
	return surface ? surface->data.x11.pixmap : 0u;
}

void *util_display_get(void)
{
	return ecore_x_display_get();
}

/* End of a file */
