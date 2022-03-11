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

#ifndef __WIDGET_IMPL_H__
#define __WIDGET_IMPL_H__

#include <widget_viewer.h>

#include "widget_viewer_dali_generic.h"
#include "widget_dali.h"

using namespace Dali;

namespace WidgetViewerDali
{

namespace Internal
{

class CWidget;
typedef IntrusivePtr<CWidget> WidgetInternalPtr;

class CWidget : public Dali::Toolkit::Internal::Control
{
public:
	CWidget(unsigned int id, void* handle, const char *pkgname, widget_size_type_e size, double period);

	CWidget(const CWidget&);
	CWidget(Internal::CWidget&);

	virtual ~CWidget();

	static WidgetViewerDali::CWidget New(unsigned int id, void* handler, const char *pkgname, widget_size_type_e size, double period);

	void Initialize();

	void UpdateByPixmap(unsigned int pixmap);

	int GetRowLen() { return mRowLen; }
	int GetColLen() { return mColLen; }

	void SetGridPosition(int row, int col);
	void GetGridPosition(int *row, int *col);

	unsigned int GetPixmap() { return mPixmap; }
	void SetPixmap(unsigned int pixmap);

	unsigned int GetWidgetId();
	void SetWidgetId(unsigned int id);

	void SetEditMode(bool mode);
	void SetTouchable(bool able);
	void CancelTouchOperation();

	std::string GetPkgName() { return mPkgName; }
	double GetPeriod() { return mPeriod; }
	void SetWidgetHandler(void* handle);
	void* GetWidgetHandle() { return mHandler; }
	widget_size_type_e GetSizeType() { return mSize; }
	bool IsFaulted() { return mIsFaulted; }
	void SetFault(bool fault);
	void OnFault();

	WidgetViewerDali::CWidget::DeletedSignal& SignalDeleted() { return mSignalDeleted; }
	WidgetViewerDali::CWidget::TouchedSignal& SignalTouched() { return mSignalTouched; };
	WidgetViewerDali::CWidget::TappedSignal& SignalTapped() { return mSignalTapped; };
	WidgetViewerDali::CWidget::LongpressedSignal& SignalLongpressed() { return mSignalLongpressed; };
	WidgetViewerDali::CWidget::ReloadSignal& SignalReload() { return mSignalReload; }

	bool OnTouch(const TouchEvent& event);

private:
	CWidget& operator=(const CWidget& rhs);

	bool _OnDeleteButtonClicked( Dali::Toolkit::Button button );
	bool _OnEventAreaTouchEvent(Dali::Actor actor, const TouchEvent& event);

	Vector3 _GetScreenPosition();
	void _ConnectTouchSignal(bool isConnect);
	bool _OnLongTapTimerComplete();
	void _OnWidgetTapped();

private:
	void* mHandler;
	widget_size_type_e mSize;
	unsigned int mPixmap;
	int mRow;
	int mCol;
	unsigned int mRowLen;
	unsigned int mColLen;
	unsigned int mId;

	PixmapImagePtr mPixmapImage;
	NativeImage mBaseNativeImageData;
	ImageActor mBaseImage;

	Toolkit::TextLabel mLoadingTextLabel;
	Toolkit::TextLabel mFaultedTextLabel;
	Dali::Toolkit::PushButton mDeleteButton;

	Actor mEventArea;

	WidgetViewerDali::CWidget::DeletedSignal mSignalDeleted;
	WidgetViewerDali::CWidget::TouchedSignal mSignalTouched;
	WidgetViewerDali::CWidget::TappedSignal mSignalTapped;
	WidgetViewerDali::CWidget::LongpressedSignal mSignalLongpressed;
	WidgetViewerDali::CWidget::ReloadSignal mSignalReload;

	std::string mPkgName;
	double mPeriod;
	bool mIsTouched;
	bool mIsTouchable;
	bool mIsConnectedTouchSignal;

	bool mIsFaulted;

	Timer mLongTapTimer;
	Vector2 mDownPos;

	bool mIsEditMode;
	ImageActor mFaultDimBg;

};

}

inline WidgetViewerDali::Internal::CWidget& GetImpl(WidgetViewerDali::CWidget& widget)
{
	DALI_ASSERT_ALWAYS(widget);

	Dali::RefObject& handle = widget.GetImplementation();

	return static_cast<WidgetViewerDali::Internal::CWidget&>(handle);
}

inline const WidgetViewerDali::Internal::CWidget& GetImpl(const WidgetViewerDali::CWidget& widget)
{
	DALI_ASSERT_ALWAYS(widget);

	const Dali::RefObject& handle = widget.GetImplementation();

	return static_cast<const WidgetViewerDali::Internal::CWidget&>(handle);
}

}

#endif

