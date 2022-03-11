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

#include <widget_viewer.h>

#include "widget_viewer_dali_generic.h"
#include "widget_dali.h"
#include "widget_dali_impl.h"
#include "debug.h"

using namespace Dali;
using namespace WidgetViewerDali;

namespace WidgetViewerDali
{

EAPI CWidget::CWidget()
{
}

EAPI CWidget::CWidget(const CWidget& widget)
: Dali::Toolkit::Control(widget)
{
}

EAPI CWidget::CWidget(Internal::CWidget& impl)
: Dali::Toolkit::Control(impl)
{
}

EAPI CWidget::CWidget(Dali::Internal::CustomActor* implementation)
: Dali::Toolkit::Control(implementation)
{
	VerifyCustomActorPointer<Internal::CWidget>(implementation);
}

EAPI CWidget::~CWidget()
{
}

CWidget CWidget::New(unsigned int id, void* handle, const char *pkgname, widget_size_type_e size, double period)
{
	return Internal::CWidget::New(id, handle, pkgname, size, period);
}

void CWidget::Initialize()
{
	GetImpl(*this).Initialize();
}

EAPI CWidget& CWidget::operator=( const CWidget& widget )
{
	if( &widget != this )
	{
		Dali::Toolkit::Control::operator=( widget );
	}

	return *this;
}

EAPI CWidget CWidget::DownCast( BaseHandle handle )
{
	return Dali::Toolkit::Control::DownCast<CWidget, Internal::CWidget>(handle);
}

EAPI int CWidget::GetRowLen()
{
	return GetImpl(*this).GetRowLen();
}

EAPI int CWidget::GetColLen()
{
	return GetImpl(*this).GetColLen();
}

EAPI void CWidget::SetGridPosition(int row, int col)
{
	GetImpl(*this).SetGridPosition(row, col);
}

EAPI void CWidget::GetGridPosition(int *row, int *col)
{
	GetImpl(*this).GetGridPosition(row, col);
}

unsigned int CWidget::GetPixmap()
{
	return GetImpl(*this).GetPixmap();
}

void CWidget::SetPixmap(unsigned int pixmap)
{
	GetImpl(*this).SetPixmap(pixmap);
}

EAPI unsigned int CWidget::GetWidgetId()
{
	return GetImpl(*this).GetWidgetId();
}

void CWidget::SetWidgetId(unsigned int id)
{
	GetImpl(*this).SetWidgetId(id);
}

void CWidget::UpdateByPixmap(unsigned int pixmap)
{
	GetImpl(*this).UpdateByPixmap(pixmap);
}

EAPI void CWidget::SetEditMode(bool mode)
{
	GetImpl(*this).SetEditMode(mode);
}

EAPI void CWidget::SetTouchable(bool able)
{
	GetImpl(*this).SetTouchable(able);
}

EAPI void CWidget::CancelTouchOperation()
{
	GetImpl(*this).CancelTouchOperation();
}

CWidget::DeletedSignal& CWidget::SignalDeleted()
{
	return GetImpl(*this).SignalDeleted();
}

EAPI CWidget::TouchedSignal& CWidget::SignalTouched()
{
	return GetImpl(*this).SignalTouched();
}

EAPI CWidget::TappedSignal& CWidget::SignalTapped()
{
	return GetImpl(*this).SignalTapped();
}

EAPI CWidget::LongpressedSignal& CWidget::SignalLongpressed()
{
	return GetImpl(*this).SignalLongpressed();
}

CWidget::ReloadSignal& CWidget::SignalReload()
{
	return GetImpl(*this).SignalReload();
}

std::string CWidget::GetPkgName()
{
	return GetImpl(*this).GetPkgName();
}

double CWidget::GetPeriod()
{
	return GetImpl(*this).GetPeriod();
}

bool CWidget::IsFaulted()
{
	return GetImpl(*this).IsFaulted();
}

void CWidget::SetFault(bool fault)
{
	GetImpl(*this).SetFault(fault);
}

void CWidget::SetWidgetHandler(void* handle)
{
	GetImpl(*this).SetWidgetHandler(handle);
}

void* CWidget::GetWidgetHandle()
{
	return GetImpl(*this).GetWidgetHandle();
}

widget_size_type_e CWidget::GetSizeType()
{
	return GetImpl(*this).GetSizeType();
}

EAPI bool CWidget::OnTouch(const TouchEvent& event)
{
	return GetImpl(*this).OnTouch(event);
}

void CWidget::OnFault()
{
	GetImpl(*this).OnFault();
}

}
