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

#include <string.h>

#include "widget_dali_list.h"

using namespace Dali;
using namespace WidgetViewerDali;

namespace WidgetViewerDali
{

CWidgetList::CWidgetList()
{
	m_mWidgetList.clear();
}

CWidgetList::~CWidgetList()
{
}

int CWidgetList::Add(void* handle, CWidget widget)
{
	DbgPrint("size[%d]", m_mWidgetList.size());

	m_mWidgetList[handle] = widget;

	DbgPrint("size[%d]", m_mWidgetList.size());

	return m_mWidgetList.size();
}

void CWidgetList::Remove(void* handle)
{
	DbgPrint("size[%d]", m_mWidgetList.size());

	std::map<void*, CWidget>::iterator iter = m_mWidgetList.find(handle);

	m_mWidgetList.erase(iter);

	DbgPrint("size[%d]", m_mWidgetList.size());
}

CWidget CWidgetList::GetWidget(unsigned int id)
{
	std::map<void*, CWidget>::iterator iter;
	for(iter = m_mWidgetList.begin(); iter!= m_mWidgetList.end(); iter++)
	{
		if(iter->second.GetWidgetId() == id)
		{
			DbgPrint("Get id[%d]", id);
			return iter->second;
		}
	}

	return NULL;
}

unsigned int CWidgetList::GetWidgetId(void *handle)
{
	std::map<void*, CWidget>::iterator iter;
	for(iter = m_mWidgetList.begin(); iter!= m_mWidgetList.end(); iter++)
	{
		if(iter->first == handle)
		{
			return iter->second.GetWidgetId();
		}
	}

	ErrPrint("Widget is wrong!");
	return 0;
}

std::vector<WidgetViewerDali::CWidget> CWidgetList::GetWidgetList(const char * pkgname)
{
	std::vector<WidgetViewerDali::CWidget> list;
	std::map<void*, CWidget>::iterator iter;
	for(iter = m_mWidgetList.begin(); iter!= m_mWidgetList.end(); iter++)
	{
		if(!iter->second.GetPkgName().compare(pkgname))
		{
			list.push_back(iter->second);
		}
	}

	return list;
}

void CWidgetList::Update(void* handle, void* newHandle, CWidget widget)
{
	DbgPrint("handle %p, newhandle %p", handle, newHandle);

	widget.SetWidgetHandler(newHandle);
	m_mWidgetList[newHandle] = widget;
	Remove(handle);
}

}

