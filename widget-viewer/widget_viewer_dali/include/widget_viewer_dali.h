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

#ifndef __WIDGET_VIEWER_DALI_H__
#define __WIDGET_VIEWER_DALI_H__

#include <Ecore_X.h>

#include "widget_viewer_dali_generic.h"
#include "widget_dali.h"

using namespace Dali;

namespace WidgetViewerDali
{
class CWidgetList;

class CWidgetManager :  public ConnectionTracker
{
public:
	/*
	 * Signals
	 */
	typedef Dali::Signal<void(WidgetViewerDali::CWidget)> WidgetAddedSignal;
	typedef Dali::Signal<void(WidgetViewerDali::CWidget)> WidgetDeletedSignal;

public:
	CWidgetManager();
	~CWidgetManager();

	static CWidgetManager* GetInstance();

	void Initialize();

	WidgetAddedSignal& GetSignalWidgetAdded() { return mSignalWidgetAdded; }
	WidgetDeletedSignal& GetSignalWidgetDeleted() { return mSignalWidgetDeleted; }

	int AddWidget(unsigned int id, const char *pkgname, const char *name, widget_size_type_e type, const char *content,
		const char *icon, int pid, double period, int allow_duplicate);

	void WidgetEventCreated(void *handler);
	void WidgetEventUpdated(void *handler);
	void WidgetEventDeleted(void *handler);
	void WidgetFaulted(const char *pkgname);
	void EventPixmapAcquired(void *handler, unsigned int pixmapId);

	bool OnWidgetDeleted(WidgetViewerDali::CWidget& widget);
	bool OnWidgetReload(WidgetViewerDali::CWidget& widget);

private:
	static CWidgetManager* m_pInstance;

	WidgetAddedSignal mSignalWidgetAdded;
	WidgetDeletedSignal mSignalWidgetDeleted;

	CWidgetList* mWidgetList;
};
}

#endif