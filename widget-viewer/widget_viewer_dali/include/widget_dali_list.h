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

#ifndef __WIDGET_LIST_H__
#define __WIDGET_LIST_H__

#include <map>

#include "widget_viewer_dali_generic.h"
#include "widget_dali.h"

using namespace Dali;

namespace WidgetViewerDali
{

class CWidgetList
{
public:
	CWidgetList();

	~CWidgetList();

	int Add(void* handle, CWidget widget);
	void Remove(void* handle);
	CWidget GetWidget(unsigned int id);
	unsigned int GetWidgetId(void *handle);
	std::vector<WidgetViewerDali::CWidget> GetWidgetList(const char * pkgname);
	void Update(void* handle, void* newHandle, CWidget widget);

private:
	std::map<void*, CWidget> m_mWidgetList;
};

}

#endif


