#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>

#include <Elementary.h>

#if defined(ENABLE_EFL_ASSIST)
#include <efl_assist.h>
#endif

#include <dlog.h>

#include <widget_errno.h>

#include "conf.h"
#include "debug.h"

#if defined(ENABLE_EFL_ASSIST)
static struct info {
	Eina_List *color_list;
	Eina_List *font_list;
} s_info = {
	.color_list = NULL,
	.font_list = NULL,
};
#endif

static int load_files(const char *path, int (*cb)(const char *fname, void *data), void *data)
{
	DIR *handle;
	struct dirent *ent;
	char *fname;
	int plen;
	int len;
	int ret;

	handle = opendir(path);
	if (!handle) {
		ErrPrint("opendir: %d (%s)\n", errno, path);
		return WIDGET_ERROR_IO_ERROR;
	}

	plen = strlen(path);

	while ((ent = readdir(handle))) {
		if (ent->d_name[0] == '.') {
			continue;
		}

		len = strlen(ent->d_name);
		DbgPrint("File: %s\n", ent->d_name);
		if (len <= 4 || strcasecmp(ent->d_name + len - 4, ".xml")) {
			DbgPrint("Skip: %s\n", ent->d_name);
			continue;
		}

		len = plen + len + 2;

		fname = malloc(len);
		if (!fname) {
			if (closedir(handle) < 0) {
				ErrPrint("closedir: %d\n", errno);
			}
			return WIDGET_ERROR_OUT_OF_MEMORY;
		}

		snprintf(fname, len, "%s/%s", path, ent->d_name);
		ret = cb(fname, data);
		free(fname);
		if (ret < 0) {
			break;
		}
	}

	if (closedir(handle) < 0) {
		ErrPrint("closedir: %d\n", errno);
	}

	return WIDGET_ERROR_NONE;
}

static int color_loader_cb(const char *fname, void *data)
{
#if defined(ENABLE_EFL_ASSIST)
	Ea_Theme_Color_Table *color;

	DbgPrint("Load color theme: %s\n", fname);
	color = ea_theme_color_table_new(fname);
	if (!color) {
		ErrPrint("Failed to load theme table: %s\n", fname);
		return WIDGET_ERROR_NONE;
	}

	ea_theme_colors_set(color, EA_THEME_STYLE_DEFAULT);

	s_info.color_list = eina_list_append(s_info.color_list, color);
	return WIDGET_ERROR_NONE;
#else
	return WIDGET_ERROR_NOT_SUPPORTED;
#endif
}

static int font_loader_cb(const char *fname, void *data)
{
#if defined(ENABLE_EFL_ASSIST)
	Eina_List *list;

	DbgPrint("Load font theme: %s\n", fname);
	list = ea_theme_font_table_new(fname);
	if (!list) {
		ErrPrint("Failed to load font table: %s\n", fname);
		return WIDGET_ERROR_NONE;
	}

	ea_theme_fonts_set(list);
	s_info.font_list = eina_list_append(s_info.font_list, list);
	return WIDGET_ERROR_NONE;
#else
	return WIDGET_ERROR_NOT_SUPPORTED;
#endif
}

HAPI int theme_loader_load(const char *path)
{
	char *folder;
	int len;

	len = strlen(path) + strlen("color") + 2;
	folder = malloc(len);
	if (!folder) {
		ErrPrint("malloc: %d\n", errno);
		return WIDGET_ERROR_OUT_OF_MEMORY;
	}

	snprintf(folder, len, "%s/color", path);
	(void)load_files(folder, color_loader_cb, NULL);

	snprintf(folder, len, "%s/font", path);
	(void)load_files(folder, font_loader_cb, NULL);

	free(folder);
	return WIDGET_ERROR_NONE;
}

HAPI void theme_loader_unload(void)
{
#if defined(ENABLE_EFL_ASSIST)
	Ea_Theme_Color_Table *color_item;
	Eina_List *font_item;

	EINA_LIST_FREE(s_info.color_list, color_item) {
		ea_theme_color_table_free(color_item);
	}

	EINA_LIST_FREE(s_info.font_list, font_item) {
		ea_theme_font_table_free(font_item);
	}
#endif
}

/* End of a file */
