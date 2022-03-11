enum buffer_type { /*!< Must have to be sync with libprovider, libwidget-viewer, libwidget-edje */
	BUFFER_TYPE_FILE,
	BUFFER_TYPE_SHM,
	BUFFER_TYPE_PIXMAP,
	BUFFER_TYPE_ERROR
};

extern int script_buffer_load(void *handle);
extern int script_buffer_unload(void *handle);
extern int script_buffer_is_loaded(const void *handle);
extern int script_buffer_resize(void *handle, int w, int h);
extern void script_buffer_update_size(void *handle, int w, int h);
extern const char *script_buffer_id(const void *handle);
extern enum buffer_type script_buffer_type(const void *handle);

extern int script_buffer_pixmap(const void *handle);
extern void *script_buffer_pixmap_acquire_buffer(void *handle);
extern int script_buffer_pixmap_release_buffer(void *canvas);
extern void *script_buffer_pixmap_ref(void *handle);
extern int script_buffer_pixmap_unref(void *buffer_ptr);
extern void *script_buffer_pixmap_find(int pixmap);
extern void *script_buffer_pixmap_buffer(void *handle);
extern int script_buffer_lock(void *handle);
extern int script_buffer_unlock(void *handle);
extern int script_buffer_stride(void *handle);
extern int script_buffer_auto_align(void);
extern int script_buffer_pixels(void *handle);

extern void *script_buffer_fb(void *handle);
extern int script_buffer_get_size(void *handle, int *w, int *h);
extern void script_buffer_flush(void *handle);
extern void *script_buffer_instance(void *handle);

extern void *script_buffer_raw_open(enum buffer_type type, void *resource);
extern int script_buffer_raw_close(void *buffer);
extern void *script_buffer_raw_data(void *buffer);
extern int script_buffer_raw_size(void *buffer);

extern int script_buffer_signal_emit(void *buffer_handle, const char *part, const char *signal, double x, double y, double ex, double ey);

/* End of a file */
