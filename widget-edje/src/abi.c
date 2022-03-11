#define _GNU_SOURCE

#include <stdio.h>
#include <dlfcn.h>

#include <dlog.h>
#include <widget_errno.h>

#include "debug.h"
#include "abi.h"

int script_buffer_load(void *handle)
{
	static int (*load)(void *handle) = NULL;
	if (!load) {
		load = dlsym(RTLD_DEFAULT, "buffer_handler_load");
		if (!load) {
			ErrPrint("broken ABI\n");
			return WIDGET_ERROR_NOT_SUPPORTED;
		}
	}

	return load(handle);
}

int script_buffer_unload(void *handle)
{
	static int (*unload)(void *handle) = NULL;

	if (!unload) {
		unload = dlsym(RTLD_DEFAULT, "buffer_handler_unload");
		if (!unload) {
			ErrPrint("broken ABI\n");
			return WIDGET_ERROR_NOT_SUPPORTED;
		}
	}

	return unload(handle);
}

int script_buffer_is_loaded(const void *handle)
{
	static int (*is_loaded)(const void *handle) = NULL;

	if (!is_loaded) {
		is_loaded = dlsym(RTLD_DEFAULT, "buffer_handler_is_loaded");
		if (!is_loaded) {
			ErrPrint("broken ABI\n");
			return WIDGET_ERROR_NOT_SUPPORTED;
		}
	}

	return is_loaded(handle);
}

int script_buffer_resize(void *handle, int w, int h)
{
	static int (*resize)(void *handle, int w, int h) = NULL;

	if (!resize) {
		resize = dlsym(RTLD_DEFAULT, "buffer_handler_resize");
		if (!resize) {
			ErrPrint("broken ABI\n");
			return WIDGET_ERROR_NOT_SUPPORTED;
		}
	}

	return resize(handle, w, h);
}

void script_buffer_update_size(void *handle, int w, int h)
{
	static void (*update_size)(void *handle, int w, int h) = NULL;

	if (!update_size) {
		update_size = dlsym(RTLD_DEFAULT, "buffer_handler_update_size");
		if (!update_size) {
			ErrPrint("broken ABI\n");
			return;
		}
	}

	return update_size(handle, w, h);	/*! "void" function can be used with "return" statement */
}

const char *script_buffer_id(const void *handle)
{
	static const char *(*buffer_id)(const void *handle) = NULL;

	if (!buffer_id) {
		buffer_id = dlsym(RTLD_DEFAULT, "buffer_handler_id");
		if (!buffer_id) {
			ErrPrint("broken ABI\n");
			return NULL;
		}
	}

	return buffer_id(handle);
}

enum buffer_type script_buffer_type(const void *handle)
{
	static enum buffer_type (*buffer_type)(const void *handle) = NULL;

	if (!buffer_type) {
		buffer_type = dlsym(RTLD_DEFAULT, "buffer_handler_type");
		if (!buffer_type) {
			ErrPrint("broken ABI\n");
			return BUFFER_TYPE_ERROR;
		}
	}

	return buffer_type(handle);
}

int script_buffer_pixmap(const void *handle)
{
	static int (*buffer_pixmap)(const void *handle) = NULL;

	if (!buffer_pixmap) {
		buffer_pixmap = dlsym(RTLD_DEFAULT, "buffer_handler_pixmap");
		if (!buffer_pixmap) {
			ErrPrint("broken ABI\n");
			return WIDGET_ERROR_NOT_SUPPORTED;
		}
	}

	return buffer_pixmap(handle);
}

void *script_buffer_pixmap_acquire_buffer(void *handle)
{
	static void *(*pixmap_acquire_buffer)(void *handle) = NULL;

	if (!pixmap_acquire_buffer) {
		pixmap_acquire_buffer = dlsym(RTLD_DEFAULT, "buffer_handler_pixmap_acquire_buffer");
		if (!pixmap_acquire_buffer) {
			ErrPrint("broken ABI\n");
			return NULL;
		}
	}

	return pixmap_acquire_buffer(handle);
}

int script_buffer_pixmap_release_buffer(void *canvas)
{
	static int (*pixmap_release_buffer)(void *canvas) = NULL;

	if (!pixmap_release_buffer) {
		pixmap_release_buffer = dlsym(RTLD_DEFAULT, "buffer_handler_pixmap_release_buffer");
		if (!pixmap_release_buffer) {
			ErrPrint("broken ABI\n");
			return WIDGET_ERROR_NOT_SUPPORTED;
		}
	}

	return pixmap_release_buffer(canvas);
}

void *script_buffer_pixmap_ref(void *handle)
{
	static void *(*pixmap_ref)(void *handle) = NULL;

	if (!pixmap_ref) {
		pixmap_ref = dlsym(RTLD_DEFAULT, "buffer_handler_pixmap_ref");
		if (!pixmap_ref) {
			ErrPrint("broken ABI\n");
			return NULL;
		}
	}

	return pixmap_ref(handle);
}

int script_buffer_pixmap_unref(void *buffer_ptr)
{
	static int (*pixmap_unref)(void *ptr) = NULL;

	if (!pixmap_unref) {
		pixmap_unref = dlsym(RTLD_DEFAULT, "buffer_handler_pixmap_unref");
		if (!pixmap_unref) {
			ErrPrint("broken ABI\n");
			return WIDGET_ERROR_NOT_SUPPORTED;
		}
	}

	return pixmap_unref(buffer_ptr);
}

void *script_buffer_pixmap_find(int pixmap)
{
	static void *(*pixmap_find)(int pixmap) = NULL;

	if (!pixmap_find) {
		pixmap_find = dlsym(RTLD_DEFAULT, "buffer_handler_pixmap_find");
		if (!pixmap_find) {
			ErrPrint("broken ABI\n");
			return NULL;
		}
	}

	return pixmap_find(pixmap);
}

void *script_buffer_pixmap_buffer(void *handle)
{
	static void *(*pixmap_buffer)(void *handle) = NULL;

	if (!pixmap_buffer) {
		pixmap_buffer = dlsym(RTLD_DEFAULT, "buffer_handler_pixmap_buffer");
		if (!pixmap_buffer) {
			ErrPrint("broken ABI\n");
			return NULL;
		}
	}

	return pixmap_buffer(handle);
}

void *script_buffer_fb(void *handle)
{
	static void *(*buffer_fb)(void *handle) = NULL;

	if (!buffer_fb) {
		buffer_fb = dlsym(RTLD_DEFAULT, "buffer_handler_fb");
		if (!buffer_fb) {
			ErrPrint("broken ABI\n");
			return NULL;
		}
	}

	return buffer_fb(handle);
}

int script_buffer_get_size(void *handle, int *w, int *h)
{
	static int (*get_size)(void *handle, int *w, int *h) = NULL;

	if (!get_size) {
		get_size = dlsym(RTLD_DEFAULT, "buffer_handler_get_size");
		if (!get_size) {
			ErrPrint("broken ABI\n");
			return WIDGET_ERROR_NOT_SUPPORTED;
		}
	}

	return get_size(handle, w, h);
}

void script_buffer_flush(void *handle)
{
	static void (*buffer_flush)(void *handle) = NULL;

	if (!buffer_flush) {
		buffer_flush = dlsym(RTLD_DEFAULT, "buffer_handler_flush");
		if (!buffer_flush) {
			ErrPrint("broken ABI\n");
			return;
		}
	}

	return buffer_flush(handle);	/* "void" function can be used to return from this function ;) */
}

void *script_buffer_instance(void *handle)
{
	static void *(*buffer_instance)(void *handle) = NULL;

	if (!buffer_instance) {
		buffer_instance = dlsym(RTLD_DEFAULT, "buffer_handler_instance");
		if (!buffer_instance) {
			ErrPrint("broken ABI\n");
			return NULL;
		}
	}

	return buffer_instance(handle);
}

void *script_buffer_raw_open(enum buffer_type type, void *resource)
{
	static void *(*raw_open)(enum buffer_type type, void *resource) = NULL;

	if (!raw_open) {
		raw_open = dlsym(RTLD_DEFAULT, "buffer_handler_raw_open");
		if (!raw_open) {
			ErrPrint("broken ABI\n");
			return NULL;
		}
	}

	return raw_open(type, resource);
}

int script_buffer_raw_close(void *buffer)
{
	static int (*raw_close)(void *buffer) = NULL;

	if (!raw_close) {
		raw_close = dlsym(RTLD_DEFAULT, "buffer_handler_raw_close");
		if (!raw_close) {
			ErrPrint("broken ABI\n");
			return WIDGET_ERROR_NOT_SUPPORTED;
		}
	}

	return raw_close(buffer);
}

void *script_buffer_raw_data(void *buffer)
{
	static void *(*raw_data)(void *buffer) = NULL;

	if (!raw_data) {
		raw_data = dlsym(RTLD_DEFAULT, "buffer_handler_raw_data");
		if (!raw_data) {
			ErrPrint("broken ABI\n");
			return NULL;
		}
	}

	return raw_data(buffer);
}

int script_buffer_raw_size(void *buffer)
{
	static int (*raw_size)(void *buffer) = NULL;

	if (!raw_size) {
		raw_size = dlsym(RTLD_DEFAULT, "buffer_handler_raw_size");
		if (!raw_size) {
			ErrPrint("broken ABI\n");
			return WIDGET_ERROR_NOT_SUPPORTED;
		}
	}

	return raw_size(buffer);
}

int script_buffer_lock(void *handle)
{
	static int (*buffer_lock)(void *handle) = NULL; 

	if (!buffer_lock) {
		buffer_lock = dlsym(RTLD_DEFAULT, "buffer_handler_lock");
		if (!buffer_lock) {
			ErrPrint("broken ABI\n");
			return WIDGET_ERROR_NOT_SUPPORTED;
		}
	}

	return buffer_lock(handle);
}

int script_buffer_unlock(void *handle)
{
	static int (*buffer_unlock)(void *handle) = NULL;

	if (!buffer_unlock) {
		buffer_unlock = dlsym(RTLD_DEFAULT, "buffer_handler_unlock");
		if (!buffer_unlock) {
			ErrPrint("broken ABI\n");
			return WIDGET_ERROR_NOT_SUPPORTED;
		}
	}

	return buffer_unlock(handle);
}

int script_buffer_signal_emit(void *buffer_handle, const char *part, const char *signal, double x, double y, double ex, double ey)
{
	static int (*signal_emit)(void *buffer_handle, const char *part, const char *signal, double x, double y, double ex, double ey) = NULL;

	if (!signal_emit) {
		signal_emit = dlsym(RTLD_DEFAULT, "script_signal_emit");
		if (!signal_emit) {
			ErrPrint("broken ABI\n");
			return WIDGET_ERROR_NOT_SUPPORTED;
		}
	}

	return signal_emit(buffer_handle, part, signal, x, y, ex, ey);
}

int script_buffer_stride(void *handle)
{
	static int (*buffer_stride)(void *handle) = NULL;

	if (!buffer_stride) {
		buffer_stride = dlsym(RTLD_DEFAULT, "buffer_handler_stride");
		if (!buffer_stride) {
			ErrPrint("broken ABI\n");
			return WIDGET_ERROR_NOT_SUPPORTED;
		}
	}

	return buffer_stride(handle);
}

int script_buffer_auto_align(void)
{
	static int (*buffer_auto_align)(void) = NULL;

	if (!buffer_auto_align) {
		buffer_auto_align = dlsym(RTLD_DEFAULT, "buffer_handler_auto_align");
		if (!buffer_auto_align) {
			ErrPrint("broken ABI\n");
			return WIDGET_ERROR_NOT_SUPPORTED;
		}
	}

	return buffer_auto_align();
}

int script_buffer_pixels(void *handle)
{
	static int (*buffer_pixels)(void *handle) = NULL;

	if (!buffer_pixels) {
		buffer_pixels = dlsym(RTLD_DEFAULT, "buffer_handler_pixels");
		if (!buffer_pixels) {
			ErrPrint("broken ABI\n");
			return WIDGET_ERROR_NOT_SUPPORTED;
		}
	}

	return buffer_pixels(handle);
}

/* End of a file */

