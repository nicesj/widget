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

#ifndef __WIDGET_H__
#define __WIDGET_H__

#include <widget_viewer.h>
#include <widget_errno.h>

#include "widget_viewer_dali_generic.h"

using namespace Dali;

namespace WidgetViewerDali
{

namespace Internal
{
class CWidget;
}

class CWidget : public Dali::Toolkit::Control
{
public:
	typedef Signal<bool(CWidget&)> DeletedSignal;
	typedef Signal<bool(CWidget&, TouchPoint::State, Vector2)> TouchedSignal;
	typedef Signal<bool(CWidget&, Vector2)> TappedSignal;
	typedef Signal<bool(CWidget&, Vector2)> LongpressedSignal;
	typedef Signal<bool(CWidget&)> ReloadSignal;

public:
	CWidget();
	CWidget(const CWidget& widget);
	CWidget(Internal::CWidget&);
	CWidget(Dali::Internal::CustomActor*);

	~CWidget();

	static WidgetViewerDali::CWidget New(unsigned int id, void* handle, const char *pkgname, widget_size_type_e size, double period);

	CWidget& operator=( const CWidget& widget );
	static CWidget DownCast( BaseHandle handle );

	void Initialize();

	void UpdateByPixmap(unsigned int pixmap);

	int GetRowLen();
	int GetColLen();

	void SetGridPosition(int row, int col);
	void GetGridPosition(int *row, int *col);

	unsigned int GetPixmap();
	void SetPixmap(unsigned int pixmap);
	unsigned int GetWidgetId();
	void SetWidgetId(unsigned int id);

	void SetEditMode(bool mode);
	void SetTouchable(bool able);
	void CancelTouchOperation();

	DeletedSignal& SignalDeleted();
	TouchedSignal& SignalTouched();
	TappedSignal& SignalTapped();
	LongpressedSignal& SignalLongpressed();
	ReloadSignal& SignalReload();

	std::string GetPkgName();
	double GetPeriod();
	void SetWidgetHandler(void* handle);
	void* GetWidgetHandle();
	widget_size_type_e GetSizeType();

	bool IsFaulted();
	void SetFault(bool fault);

	bool OnTouch(const TouchEvent& event);
	void OnFault();
};

}

#endif
