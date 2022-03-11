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
#include <cmath>

#include "widget_viewer_dali_generic.h"
#include "widget_viewer_dali_const.h"
#include "widget_dali_impl.h"
#include "debug.h"

#if defined(LOG_TAG)
#undef LOG_TAG
#endif

#define LOG_TAG "WIDGET_DALI"

using namespace Dali;
using namespace WidgetViewerDali;

namespace WidgetViewerDali
{

namespace Internal
{

EAPI CWidget::CWidget(unsigned int id, void* handle, const char *pkgname, widget_size_type_e size, double period)
: Dali::Toolkit::Internal::Control(ControlBehaviour(REQUIRES_TOUCH_EVENTS)),
  mHandler(handle),
  mSize(size),
  mPixmap(0),
  mRowLen(1),
  mColLen(1),
  mId(id),
  mPkgName(pkgname),
  mPeriod(period),
  mIsTouched(false),
  mIsTouchable(true),
  mIsConnectedTouchSignal(false),
  mIsFaulted(false),
  mIsEditMode(false)
{
}

EAPI CWidget::~CWidget()
{
}

EAPI WidgetViewerDali::CWidget CWidget::New(unsigned int id, void* handler, const char *pkgname, widget_size_type_e size, double period)
{
	WidgetInternalPtr pWidget(new CWidget(id, handler, pkgname, size, period));

	WidgetViewerDali::CWidget handle(*pWidget);

	pWidget->Initialize();

	return handle;
}

void CWidget::Initialize()
{
	DbgPrint("%x", mSize);
	Actor self = Self();

	self.SetParentOrigin( ParentOrigin::TOP_CENTER );
	self.SetAnchorPoint( AnchorPoint::TOP_CENTER );

	mBaseImage = ImageActor::New();
	mBaseImage.SetParentOrigin( ParentOrigin::TOP_LEFT );
	mBaseImage.SetAnchorPoint( AnchorPoint::TOP_LEFT );
	Constraint constraint = Constraint::New<Vector3>( mBaseImage, Actor::Property::SIZE, EqualToConstraint() );
	constraint.AddSource( ParentSource(Actor::Property::SIZE) );
	constraint.Apply();

	self.Add(mBaseImage);

	mEventArea = Actor::New();

	Constraint eventAreaConstraint = Constraint::New<Vector3>( mEventArea, Actor::Property::SIZE, EqualToConstraint() );
	eventAreaConstraint.AddSource( ParentSource(Actor::Property::SIZE) );
	eventAreaConstraint.Apply();

	mEventArea.SetAnchorPoint(AnchorPoint::CENTER);
	mEventArea.SetParentOrigin(ParentOrigin::CENTER);
	mEventArea.SetPosition(0.0f, 0.0f, 0.5f);

	self.Add(mEventArea);

	/* supporting to operate normal and edit mode */
	mEventArea.TouchedSignal().Connect(this, &Internal::CWidget::_OnEventAreaTouchEvent);
	mEventArea.SetLeaveRequired(true);
	mIsConnectedTouchSignal = true;

	Vector2 resolution = Stage::GetCurrent().GetDpi();
	mLoadingTextLabel = Toolkit::TextLabel::New(_("IDS_ST_BODY_LOADING_ING"));
	mLoadingTextLabel.SetProperty(Toolkit::TextLabel::Property::HORIZONTAL_ALIGNMENT, "CENTER");
	mLoadingTextLabel.SetProperty(Toolkit::TextLabel::Property::VERTICAL_ALIGNMENT, "CENTER");
	mLoadingTextLabel.SetProperty(Toolkit::TextLabel::Property::POINT_SIZE, WIDGET_LOADING_TEXT_SIZE * 72.0f/resolution.y);
	mLoadingTextLabel.SetProperty(Toolkit::TextLabel::Property::TEXT_COLOR, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	mLoadingTextLabel.SetParentOrigin(ParentOrigin::CENTER);
	mLoadingTextLabel.SetAnchorPoint(AnchorPoint::CENTER);
	mLoadingTextLabel.SetSize(WIDGET_LOADING_TEXT_WIDTH, WIDGET_LOADING_TEXT_SIZE * 3);
	mLoadingTextLabel.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
	mBaseImage.Add(mLoadingTextLabel);

	Image iconImage;
	mDeleteButton = Dali::Toolkit::PushButton::New();
	iconImage = ResourceImage::New(WIDGET_VIEWER_ICONDIR"/btn_delete_nor.png");
	mDeleteButton.SetButtonImage(iconImage);
	iconImage = ResourceImage::New(WIDGET_VIEWER_ICONDIR"/btn_delete_press.png");
	mDeleteButton.SetSelectedImage(iconImage);
	mDeleteButton.SetSize(Vector2(WIDGET_DELETE_BUTTON_WIDTH, WIDGET_DELETE_BUTTON_HEIGHT));
	mDeleteButton.SetAnchorPoint(AnchorPoint::TOP_LEFT);
	mDeleteButton.SetParentOrigin(AnchorPoint::TOP_LEFT);
	mDeleteButton.SetPosition(Vector3(WIDGET_DELETE_BUTTON_X, WIDGET_DELETE_BUTTON_Y, WIDGET_DELETE_BUTTON_Z));
	mDeleteButton.SetOpacity(0.0f);

	mDeleteButton.ClickedSignal().Connect( this, &Internal::CWidget::_OnDeleteButtonClicked );

	mBaseImage.Add(mDeleteButton);

	switch (mSize) {
	case WIDGET_SIZE_TYPE_1x1 :
		mRowLen = 1;
		mColLen = 1;
		break;
	case WIDGET_SIZE_TYPE_2x1 :
		mRowLen = 1;
		mColLen = 2;
		break;
	case WIDGET_SIZE_TYPE_2x2 :
		mRowLen = 2;
		mColLen = 2;
		break;
	case WIDGET_SIZE_TYPE_4x1 :
		mRowLen = 1;
		mColLen = 4;
		break;
	case WIDGET_SIZE_TYPE_4x2 :
		mRowLen = 2;
		mColLen = 4;
		break;
	case WIDGET_SIZE_TYPE_4x3 :
		mRowLen = 3;
		mColLen = 4;
		break;
	case WIDGET_SIZE_TYPE_4x4 :
		mRowLen = 4;
		mColLen = 4;
		break;
	case WIDGET_SIZE_TYPE_4x5 :
		mRowLen = 5;
		mColLen = 4;
		break;
	case WIDGET_SIZE_TYPE_4x6 :
		mRowLen = 6;
		mColLen = 4;
		break;
	default:
		mRowLen = 1;
		mColLen = 1;
		break;
	}

	if (!mLongTapTimer)
	{
		mLongTapTimer = Timer::New(1000);
		mLongTapTimer.TickSignal().Connect(this, &Internal::CWidget::_OnLongTapTimerComplete);
	}

	DbgPrint("rowLen, colLen [%d, %d]", mRowLen, mColLen);
}

void CWidget::UpdateByPixmap(unsigned int pixmap)
{
	DbgPrint("Update widget by pixmap[%d]", pixmap);
	mPixmap = pixmap;
	mPixmapImage = PixmapImage::New(mPixmap);
	mBaseNativeImageData = NativeImage::New(*mPixmapImage);

	if(mLoadingTextLabel)
	{
		mBaseImage.Remove(mLoadingTextLabel);
		mLoadingTextLabel.Reset();
	}

	if(mFaultDimBg)
	{
		mFaultDimBg.SetOpacity(0.0f);
	}

	mBaseImage.SetImage(mBaseNativeImageData);

	mIsFaulted = false;
}

void CWidget::SetPixmap(unsigned int pixmap)
{
	DbgPrint("Set [%d]", pixmap);
	mPixmap = pixmap;
}

void CWidget::SetWidgetId(unsigned int id)
{
	DbgPrint("Set [%d]", id);
	mId = id;
}

unsigned int CWidget::GetWidgetId()
{
	DbgPrint("Get Id [%d]", mId);
	return mId;
}

void CWidget::SetWidgetHandler(void* handle)
{
	mHandler = handle;
}

void CWidget::SetEditMode(bool mode)
{
	mIsEditMode = mode;
	CancelTouchOperation();

	mDeleteButton.SetOpacity(mode ? 1.0f : 0.0f);

	_ConnectTouchSignal(!mode);
}

void CWidget::SetTouchable(bool able)
{
	mIsTouchable = able;
	_ConnectTouchSignal(able);
}

void CWidget::_ConnectTouchSignal(bool isConnect)
{
	if(isConnect && mIsTouchable)
	{
		mEventArea.TouchedSignal().Connect(this, &Internal::CWidget::_OnEventAreaTouchEvent);
		mIsConnectedTouchSignal = true;
	}
	else if(mIsConnectedTouchSignal)
	{
		mEventArea.TouchedSignal().Disconnect(this, &Internal::CWidget::_OnEventAreaTouchEvent);
		mIsConnectedTouchSignal = false;
	}
}

void CWidget::CancelTouchOperation()
{
	if (mLongTapTimer.IsRunning())
	{
		mLongTapTimer.Stop();
	}

	if(mIsTouched)
	{
		mIsTouched = false;

		TouchEvent event;
		Vector3 screenposition = _GetScreenPosition();
		event.points.push_back( TouchPoint ( 0, Dali::TouchPoint::Leave, screenposition.x, screenposition.y ) );
		DbgPrint("cancel x, y [%.2f, %.2f]", screenposition.x, screenposition.y);
		_OnEventAreaTouchEvent(Self(), event);
	}
}

bool CWidget::_OnDeleteButtonClicked( Dali::Toolkit::Button button )
{
	Actor self = Self();

	WidgetViewerDali::CWidget handle ( GetOwner() );

	mSignalDeleted.Emit(handle);

	return true;
}

bool CWidget::_OnEventAreaTouchEvent(Dali::Actor actor, const TouchEvent& event)
{
	DbgPrint("_OnEventAreaTouchEvent!! widget id[%d]", mId);
	WidgetViewerDali::CWidget handle ( GetOwner() );

	mSignalTouched.Emit(handle, event.GetPoint(0).state, event.GetPoint(0).screen);

	if(event.GetPoint(0).state == Dali::TouchPoint::Down)
	{
		mLongTapTimer.Start();
	}

	Actor self = Self();

	struct widget_mouse_event_info info;

	Vector3 screenposition = _GetScreenPosition();
	info.x = event.GetPoint(0).screen.x - screenposition.x;
	info.y = event.GetPoint(0).screen.y - screenposition.y;

	info.ratio_w = 1.0f;
	info.ratio_h = 1.0f;
	info.device = 0;

	if(event.GetPoint(0).state == Dali::TouchPoint::Down)
	{
		mIsTouched = true;
		mLongTapTimer.Start();
		mDownPos.x = info.x;
		mDownPos.y = info.y;

		widget_viewer_feed_mouse_event(static_cast<widget_h >(mHandler), WIDGET_MOUSE_ENTER, &info);
		widget_viewer_feed_mouse_event(static_cast<widget_h >(mHandler), WIDGET_MOUSE_DOWN, &info);
	}
	else if(event.GetPoint(0).state == Dali::TouchPoint::Up)
	{
		if(mIsTouched)
		{
			float gapX = fabs(info.x - mDownPos.x);
			float gapY = fabs(info.y - mDownPos.y);

			if(gapX <= TAPPED_BOUNDARY_WIDTH && gapY <= TAPPED_BOUNDARY_HEIGHT)
			{
				mSignalTapped.Emit(handle, event.GetPoint(0).screen);
				_OnWidgetTapped();
			}
		}
		mIsTouched = false;

		if (mLongTapTimer.IsRunning())
		{
			mLongTapTimer.Stop();
		}

		widget_viewer_feed_mouse_event(static_cast<widget_h >(mHandler), WIDGET_MOUSE_UP, &info);
		widget_viewer_feed_mouse_event(static_cast<widget_h >(mHandler), WIDGET_MOUSE_LEAVE, &info);
	}
	else if(event.GetPoint(0).state == Dali::TouchPoint::Motion)
	{
		if(mIsTouched && mLongTapTimer.IsRunning())
		{
			float gapX = fabs(info.x - mDownPos.x);
			float gapY = fabs(info.y - mDownPos.y);

			if(gapX > TAPPED_BOUNDARY_WIDTH || gapY > TAPPED_BOUNDARY_HEIGHT)
			{
				mLongTapTimer.Stop();
			}
		}
		widget_viewer_feed_mouse_event(static_cast<widget_h >(mHandler), WIDGET_MOUSE_MOVE, &info);
	}
	else if(event.GetPoint(0).state == Dali::TouchPoint::Leave || event.GetPoint(0).state == Dali::TouchPoint::Interrupted)
	{
		mIsTouched = false;

		if (mLongTapTimer.IsRunning())
		{
			mLongTapTimer.Stop();
		}

		/*widget_viewer_feed_mouse_event(static_cast<widget_h >(mHandler), WIDGET_MOUSE_UP, &info);*/
		widget_viewer_feed_mouse_event(static_cast<widget_h >(mHandler), WIDGET_MOUSE_LEAVE, &info);
	}

	return false;
}

bool CWidget::OnTouch(const TouchEvent& event)
{
	return _OnEventAreaTouchEvent(Self(), event);
}

void CWidget::_OnWidgetTapped()
{
	if(!mIsFaulted)
		return ;

	if(mFaultedTextLabel)
	{
		mFaultedTextLabel.SetProperty(Toolkit::TextLabel::Property::TEXT, _("IDS_ST_BODY_LOADING_ING"));
	}

	WidgetViewerDali::CWidget handle ( GetOwner() );
	mSignalReload.Emit(handle);
}

void CWidget::SetGridPosition(int row, int col)
{
	mRow = row;
	mCol = col;
}

void CWidget::GetGridPosition(int *row, int *col)
{
	*row = mRow;
	*col = mCol;
}

Vector3 CWidget::_GetScreenPosition()
{
	Actor self = Self();
	Vector3 position = self.GetCurrentPosition();
	Actor actor = self.GetParent();

	while(actor)
	{
		position += actor.GetCurrentPosition();

		actor = actor.GetParent();

	}

	return position;
}

void CWidget::SetFault(bool fault)
{
	mIsFaulted = fault;
}

bool CWidget::_OnLongTapTimerComplete()
{
	WidgetViewerDali::CWidget handle ( GetOwner() );
	mSignalLongpressed.Emit(handle, mDownPos);
	return false;
}

void CWidget::OnFault()
{
	mIsFaulted = true;
	if(mLoadingTextLabel)
	{
		mLoadingTextLabel.SetOpacity(0.0f);
	}

	if(!mFaultDimBg)
	{
		mFaultDimBg = Toolkit::CreateSolidColorActor(Vector4(0.3, 0.3, 0.3, 0.85));
		mFaultDimBg.SetZ(0.1f);
		mFaultDimBg.SetParentOrigin(ParentOrigin::CENTER);
		mFaultDimBg.SetAnchorPoint(AnchorPoint::CENTER);

		Constraint constraint = Constraint::New<Vector3>( mFaultDimBg, Actor::Property::SIZE, EqualToConstraint() );
		constraint.AddSource( ParentSource(Actor::Property::SIZE) );
		constraint.Apply();

		mBaseImage.Add(mFaultDimBg);
	}

	if(!mFaultedTextLabel)
	{
		Vector2 resolution = Stage::GetCurrent().GetDpi();
		mFaultedTextLabel = Toolkit::TextLabel::New(_("IDS_HS_BODY_UNABLE_TO_LOAD_DATA_TAP_TO_RETRY"));
		mFaultedTextLabel.SetProperty(Toolkit::TextLabel::Property::HORIZONTAL_ALIGNMENT, "CENTER");
		mFaultedTextLabel.SetProperty(Toolkit::TextLabel::Property::VERTICAL_ALIGNMENT, "CENTER");
		mFaultedTextLabel.SetProperty(Toolkit::TextLabel::Property::POINT_SIZE, WIDGET_LOADING_TEXT_SIZE * 72.0f/resolution.y);
		mFaultedTextLabel.SetProperty(Toolkit::TextLabel::Property::TEXT_COLOR, Vector4(1.0f, 1.0f, 1.0f, 1.0f));

		mFaultedTextLabel.SetAnchorPoint(AnchorPoint::CENTER);
		mFaultedTextLabel.SetParentOrigin(ParentOrigin::CENTER);

		mFaultedTextLabel.SetSize(WIDGET_LOADING_TEXT_WIDTH, WIDGET_LOADING_TEXT_SIZE * 3);
		mFaultedTextLabel.SetPosition(Vector3(0.0f, 0.0f, 0.0f));

		mFaultedTextLabel.SetZ(0.1f);

		mFaultDimBg.Add(mFaultedTextLabel);
	}
	else
	{
		mFaultedTextLabel.SetProperty(Toolkit::TextLabel::Property::TEXT, _("IDS_HS_BODY_UNABLE_TO_LOAD_DATA_TAP_TO_RETRY"));
	}

	mFaultDimBg.SetOpacity(mIsEditMode ? 0.0f : 1.0f);
}

}
}

