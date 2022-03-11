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

 #ifndef __WIDGET_VIEWER_DALI_GENERIC_H__
#define __WIDGET_VIEWER_DALI_GENERIC_H__

#include <cstdio>
#include <iostream>

#include <list>
#include <map>
#include <vector>

#include <dali/dali.h>
#include <dali-toolkit/dali-toolkit.h>
#include <dlog.h>

#include <stdio.h>
#include <libintl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#define HS_KEY_MENU	"XF86Menu"
#define HS_KEY_BACK	"XF86Back"
#define HS_KEY_HOME	"XF86Home"
#define HS_KEY_APPS		"XF86Apps"

#if !defined(EAPI)
#define EAPI __attribute__((visibility("default")))
#endif

#if defined(LOG_TAG)
#undef LOG_TAG
#endif

#define LOG_TAG "WIDGET_DALI"

#if !defined(SECURE_LOGD)
#define SECURE_LOGD LOGD
#endif

#if !defined(SECURE_LOGW)
#define SECURE_LOGW LOGW
#endif

#if !defined(SECURE_LOGE)
#define SECURE_LOGE LOGE
#endif

#if !defined(S_)
#define S_(str) dgettext("sys_string", str)
#endif

#if !defined(T_)
#define T_(str) dgettext(PKGNAME, str)
#endif

#if !defined(N_)
#define N_(str) (str)
#endif

#if !defined(_)
#define _(str) gettext(str)
#endif


#if !defined(DbgPrint)
#define DbgPrint(format, arg...)	SECURE_LOGD(format, ##arg)
#endif

#if !defined(ErrPrint)
#define ErrPrint(format, arg...)	SECURE_LOGE(format, ##arg)
#endif

#if !defined(WarnPrint)
#define WarnPrint(format, arg...)	SECURE_LOGW(format, ##arg)
#endif

#if !defined(WIDGET_VIEWER_RESDIR)
#define WIDGET_VIEWER_RESDIR "/usr/share/widget_viewer_dali/res"
#endif

#if !defined(WIDGET_VIEWER_ICONDIR)
#define WIDGET_VIEWER_ICONDIR WIDGET_VIEWER_RESDIR"/image"
#endif

#endif /* __WIDGET_VIEWER_DALI_GENERIC_H__ */

