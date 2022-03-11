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

#include <Ecore_X.h>

#include <widget_viewer.h>

#include <widget_errno.h>

#include "widget_viewer_dali_generic.h"
#include "widget_viewer_dali.h"
#include "widget_dali_list.h"
#include "debug.h"

#define WIDGET_DEFAULT_CLUSTER "user,created"
#define WIDGET_DEFAULT_CATEGORY "default"

#define FAULT_RETRY_MAX_COUNT 3

 using namespace Dali;
 using namespace WidgetViewerDali;

CWidgetManager* CWidgetManager::m_pInstance = NULL;

static int widget_is_exists(const char *pkgname)
{
	int ret=1;
	char *widget;
	widget = widget_service_get_widget_id(pkgname);
	if(!widget) {
		ret = 0;
	}
	free(widget);
	return ret;
}

static int WidgetProviderWidgetEventCB(widget_h handler, widget_event_type_e event, void *data)
{
	CWidgetManager *pWidgeProvider = static_cast<CWidgetManager *>(data);

	if(handler == NULL)
	{
		return EXIT_FAILURE;
	}

	if(event == WIDGET_EVENT_WIDGET_UPDATED)
	{
		DbgPrint("handler : %p, event : update[%d]", handler, event);
		pWidgeProvider->WidgetEventUpdated(handler);
	}
	else if(event == WIDGET_EVENT_DELETED)
	{
		DbgPrint("handler : %p, event : deleted[%d]", handler, event);
		pWidgeProvider->WidgetEventDeleted(handler);
	}
	else if(event == WIDGET_EVENT_HOLD_SCROLL)
	{
		DbgPrint("handler : %p, event : hold scroll[%d]", handler, event);
		/*pWidgeProvider->WidgetEventScrollLocked(handler, false);*/
	}
	else if(event == WIDGET_EVENT_RELEASE_SCROLL)
	{
		DbgPrint("handler : %p, event : release scroll[%d]", handler, event);
		/*pWidgeProvider->WidgetEventScrollLocked(handler, true);*/
	}
	else
	{
		DbgPrint("Not supported event : %d", event);
	}

	return EXIT_SUCCESS;
}

static void WidgetProviderWidgetActivateCB(widget_h handle, int ret, void *data)
{
	if(ret == 0)
	{
		/* Success */
		DbgPrint("handle : %p, Activate", handle);
	}
	else
	{
		ErrPrint("Failed to widget_activate[%x]", ret);
	}
}

static int WidgetProviderWidgetFaultCB(widget_fault_type_e event, const char *pkgname, const char *filename, const char *funcname, void *data)
{
	CWidgetManager *pWidgeProvider = static_cast<CWidgetManager *>(data);

	if(pWidgeProvider && event == WIDGET_FAULT_DEACTIVATED)
	{
		DbgPrint("event[%d], pkgname[%s]", event, pkgname);
		widget_viewer_activate_faulted_widget(pkgname, WidgetProviderWidgetActivateCB, data);

		pWidgeProvider->WidgetFaulted(pkgname);
	}
	else if(pWidgeProvider && event == WIDGET_FAULT_PROVIDER_DISCONNECTED)
	{
		/*pWidgeProvider->WidgetProviderDisconnected();*/
	}

	return EXIT_SUCCESS;
}

static void WidgetProviderWidgetAddCB(widget_h handle, int ret, void *data)
{
	CWidgetManager *pWidgeProvider = static_cast<CWidgetManager *>(data);

	if(pWidgeProvider == NULL)
	{
		ErrPrint("invalid add callback data");
		return;
	}

	switch (ret) {
	case WIDGET_ERROR_NONE:
		DbgPrint("handle : %p, Add", handle);
		pWidgeProvider->WidgetEventCreated(handle);
		break;
	case WIDGET_ERROR_ALREADY_EXIST:
		/*ret = notification_status_message_post("Dynamic Box is already exists");
		if(ret != NOTIFICATION_ERROR_NONE)
		{
			ErrPrint("Failed to make notification");
		}
		pWidgeProvider->WidgetEventDeleted(handle);*/
		break;
	case -1: /*fall through*/
		/* Temporarly used */
		pWidgeProvider->WidgetEventDeleted(handle);
		break;
	default:
		/*ErrPrint("Failed to widget_viewer_add_widget[%d][%p]", ret, handle);
		widgetDataProvider->WidgetEventDeleted(handle);

		pWidgeProvider->WidgetAddFailed(handle, ret);*/
		break;
	}
}

static void WidgetProviderWidgetDeleteCB(widget_h handle, int ret, void *data)
{
	if(ret == 0)
	{
		/* Success */
		DbgPrint("handle : %p, Delete", handle);
		CWidgetManager *pWidgeProvider = static_cast<CWidgetManager *>(data);

		pWidgeProvider->WidgetEventDeleted(handle);
	}
	else
	{
		ErrPrint("Failed to widget_viewer_delete_widget[%d]", ret);
	}
}

static void WidgetProviderWidgetPixmapAcquiredCB(widget_h handle, int pixmapId, void *data)
{
	CWidgetManager *pWidgeProvider = static_cast<CWidgetManager *>(data);
	pWidgeProvider->EventPixmapAcquired(handle, (unsigned int)pixmapId);
}

namespace WidgetViewerDali
{
CWidgetManager::CWidgetManager()
{
	widget_viewer_set_option(WIDGET_OPTION_DIRECT_UPDATE, 1);
	/* initialize widget, (XDisplay, prevent_overwrite, event_filtering, use_thread) */
	widget_viewer_init(ecore_x_display_get(), 1, 0.01f, 1);
	widget_viewer_add_event_handler(WidgetProviderWidgetEventCB, this);
	widget_viewer_add_fault_handler(WidgetProviderWidgetFaultCB, this);
}

CWidgetManager::~CWidgetManager()
{
	widget_viewer_remove_fault_handler(WidgetProviderWidgetFaultCB);
	widget_viewer_remove_event_handler(WidgetProviderWidgetEventCB);

	widget_viewer_fini();
}

EAPI void CWidgetManager::Initialize()
{
	DbgPrint("Initialize");
	mWidgetList = new CWidgetList;
}

EAPI int CWidgetManager::AddWidget(unsigned int id, const char *pkgname, const char *name, widget_size_type_e type, const char *content,
		const char *icon, int pid, double period, int allow_duplicate)
{
	if(1 == widget_is_exists(pkgname))
	{
		void* handle = widget_viewer_add_widget(pkgname, NULL,  WIDGET_DEFAULT_CLUSTER, WIDGET_DEFAULT_CATEGORY, period, type, WidgetProviderWidgetAddCB, this);

		if(handle != NULL)
		{
			CWidget widget = CWidget::New(id, handle, pkgname, type, period);
			widget.SignalDeleted().Connect(this, &CWidgetManager::OnWidgetDeleted);
			widget.SignalReload().Connect(this, &CWidgetManager::OnWidgetReload);
			mWidgetList->Add(handle, widget);

			mSignalWidgetAdded.Emit(widget);
		}

		return 0;
	}

	return -1;
}

bool CWidgetManager::OnWidgetDeleted(WidgetViewerDali::CWidget& widget)
{
	unsigned int id = widget.GetWidgetId();
	DbgPrint("widget[%d] deleted", id);

	if(1 == widget_is_exists(widget.GetPkgName().c_str()))
	{
		if(widget.GetWidgetHandle())
		{
			int ret = widget_viewer_delete_widget(static_cast<widget_h >(widget.GetWidgetHandle()), WIDGET_DELETE_PERMANENTLY, WidgetProviderWidgetDeleteCB, this);
			DbgPrint("call widget_viewer_delete_widget[%d]", ret);
		}
	}
	else
	{
		mWidgetList->Remove(widget.GetWidgetHandle());
	}

	mSignalWidgetDeleted.Emit(widget);

	return true;
}

bool CWidgetManager::OnWidgetReload(WidgetViewerDali::CWidget& widget)
{
	unsigned int id = widget.GetWidgetId();
	DbgPrint("widget[%d] reload", id);

	if(1 == widget_is_exists(widget.GetPkgName().c_str()))
	{
		widget_viewer_activate_faulted_widget(widget.GetPkgName().c_str(), WidgetProviderWidgetActivateCB, this);

		void* handle = widget_viewer_add_widget(widget.GetPkgName().c_str(), widget.GetPkgName().c_str(),  WIDGET_DEFAULT_CLUSTER, WIDGET_DEFAULT_CATEGORY, widget.GetPeriod(), widget.GetSizeType(), WidgetProviderWidgetAddCB, this);

		if(widget.GetWidgetHandle())
		{
			int ret = widget_viewer_delete_widget(static_cast<widget_h >(widget.GetWidgetHandle()), WIDGET_DELETE_PERMANENTLY, WidgetProviderWidgetDeleteCB, this);
			DbgPrint("call widget_viewer_delete_widget[%d]", ret);
		}

		if(handle != NULL)
		{
			mWidgetList->Update(widget.GetWidgetHandle(), handle, widget);
		}
	}

	return true;
}

void CWidgetManager::WidgetEventCreated(void *handler)
{
	widget_h hWidget = static_cast<widget_h >(handler);
	widget_viewer_set_visibility(hWidget, WIDGET_SHOW);
}

void CWidgetManager::WidgetEventUpdated(void *handler)
{
	unsigned int id = mWidgetList->GetWidgetId(handler);

	DbgPrint("WidgetEventUpdated [%d]\n", id);

	if(!id)
	{
		ErrPrint("widget is not valid");
		return;
	}

	CWidget widget = mWidgetList->GetWidget(id);

	unsigned int resource_id =0;
	widget_viewer_get_resource_id(static_cast<widget_h>(handler), 0, &resource_id);
	DbgPrint("resource_id, pixmap [%d,%d]", resource_id, widget.GetPixmap());
	if(resource_id != widget.GetPixmap())
	{
		if(widget_viewer_acquire_resource_id(static_cast<widget_h>(handler), 0, WidgetProviderWidgetPixmapAcquiredCB, this) < 0)
		{
			DbgPrint("Failed to acquire pixmap\n");
		}
	}
	else
	{
		if(resource_id != 0)
		{
			widget.UpdateByPixmap(resource_id);
		}
		else
		{
			ErrPrint("Failed to get widget");
			return;
		}
	}
}

void CWidgetManager::WidgetEventDeleted(void *handler)
{
	unsigned int id = mWidgetList->GetWidgetId(handler);

	DbgPrint("WidgetEventDeleted [%p] [%d]", handler, id);

	if(!id)
	{
		ErrPrint("widget is not valid");
		return;
	}

	CWidget widget = mWidgetList->GetWidget(id);

	if(widget.IsFaulted())
	{
		widget.OnFault();

		if(widget.GetPixmap() != 0) {
			widget_viewer_release_resource_id((widget_h)handler, 0, widget.GetPixmap());
			widget.SetPixmap(0);
		}
	}
	else
	{
		if(widget.GetPixmap() != 0) {
			widget_viewer_release_resource_id((widget_h)handler, 0, widget.GetPixmap());
			widget.SetPixmap(0);
		}

		mWidgetList->Remove(widget.GetWidgetHandle());
	}

}


void CWidgetManager::WidgetFaulted(const char *pkgname)
{
	if(!pkgname)
	{
		ErrPrint("invalid pkgname");
		return;
	}

	std::vector<CWidget> list = mWidgetList->GetWidgetList(pkgname);
	std::vector<CWidget>::iterator iter;

	if(list.empty())
	{
		DbgPrint("No boxes that pkgname is[%s]", pkgname);
	}
	else
	{
		for (iter = list.begin(); iter != list.end(); iter++)
		{
			if(iter->IsFaulted())
			{
				DbgPrint("Already faulted widget[%d][%s]", iter->GetWidgetId(), iter->GetPkgName().c_str());
			}
			else
			{
				DbgPrint("Set faulted widget[%d][%s]", iter->GetWidgetId(), iter->GetPkgName().c_str());
				iter->SetFault(true);
			}
		}
	}
}

void CWidgetManager::EventPixmapAcquired(void *handler, unsigned int pixmapId)
{
	DbgPrint("EventPixmapAcquired [%p] [%d]\n", handler, pixmapId);
	unsigned int id = mWidgetList->GetWidgetId(handler);

	if(!id)
	{
		ErrPrint("widget is not valid");
		return;
	}

	CWidget widget = mWidgetList->GetWidget(id);

	if(widget.GetPixmap())
	{
		ErrPrint("Release  pixmap [%d]", widget.GetPixmap());
		widget_viewer_release_resource_id(static_cast<widget_h>(handler), 0, widget.GetPixmap());
	}

	if(pixmapId != 0)
	{
		widget.UpdateByPixmap(pixmapId);
	}
	else
	{
		ErrPrint("Failed to get widget");
		return;
	}
}

EAPI CWidgetManager* CWidgetManager::GetInstance()
{
	if(m_pInstance == NULL)
	{
		m_pInstance = new CWidgetManager;
	}

	return m_pInstance;
}

}
