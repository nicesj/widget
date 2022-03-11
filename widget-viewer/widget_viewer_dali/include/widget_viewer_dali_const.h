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

#ifndef _WIDGET_VIEWER_DALI_CONST_H_
#define _WIDGET_VIEWER_DALI_CONST_H_

#include <dali/dali.h>
#include <dali-toolkit/dali-toolkit.h>

#define BASE_HEIGHT		CWidgetDaliConst::GetBaseHeight()
#define BASE_WIDTH		CWidgetDaliConst::GetBaseWidth()

#define SCREEN_HEIGHT	CWidgetDaliConst::GetScreenHeight()
#define SCREEN_WIDTH	CWidgetDaliConst::GetScreenWidth()

#define SCREEN_RESOLUTION_TYPE	CWidgetDaliConst::GetScreenResolutionType()

#define INDICATOR_H CWidgetDaliConst::GetIndicatorHeight()

#define WIDGET_LOADING_TEXT_WIDTH		CWidgetDaliConst::GetWidgetLoadingTextWidth()
#define WIDGET_LOADING_TEXT_SIZE			CWidgetDaliConst::GetWidgetLoadingTextSize()
#define WIDGET_DELETE_BUTTON_WIDTH		CWidgetDaliConst::GetWidgetDeleteButtonWidth()
#define WIDGET_DELETE_BUTTON_HEIGHT		CWidgetDaliConst::GetWidgetDeleteButtonHeight()
#define WIDGET_DELETE_BUTTON_X			CWidgetDaliConst::GetWidgetDeleteButtonX()
#define WIDGET_DELETE_BUTTON_Y			CWidgetDaliConst::GetWidgetDeleteButtonY()
#define WIDGET_DELETE_BUTTON_Z			CWidgetDaliConst::GetWidgetDeleteButtonZ()

#define TAPPED_BOUNDARY_WIDTH		CWidgetDaliConst::GetTappedBoundaryWidth()
#define TAPPED_BOUNDARY_HEIGHT		CWidgetDaliConst::GetTappedBoundaryHeight()

using namespace Dali;

namespace WidgetViewerDali
{
class CWidgetDaliConst
{
public:
	typedef enum
	{
		RESOLUTION_BASE_480X640 = 0,	/* 3 : 4 */
		RESOLUTION_BASE_320X480,		/* 2 : 3 */
		RESOLUTION_BASE_480X800,		/* 3 : 5 */
		RESOLUTION_BASE_540X960,		/* 9 : 16 */
		RESOLUTION_BASE_MAX
	} ScreenResolutionType;

public:
	static void SetScreenSize(Vector2 size);
	static const int GetBaseWidth() { return mBaseWidth; }
	static const int GetBaseHeight() { return mBaseHeight; }
	static const float GetScreenWidth() { return mScreenSize.width; }
	static const float GetScreenHeight() { return mScreenSize.height; }
	static const unsigned int GetScreenResolutionType() { return mScreenResolutionType; }

	static const float GetIndicatorHeight();

	static const float GetWidgetLoadingTextWidth();
	static const float GetWidgetLoadingTextSize();

	static const float GetWidgetDeleteButtonWidth();
	static const float GetWidgetDeleteButtonHeight();
	static const float GetWidgetDeleteButtonX();
	static const float GetWidgetDeleteButtonY();
	static const float GetWidgetDeleteButtonZ();

	static const float GetTappedBoundaryWidth();
	static const float GetTappedBoundaryHeight();

private :
	CWidgetDaliConst();
	static void _SetBaseScreenSize(int width, int height);
	static const unsigned int _GetResolutionType(int x, int y);

	static const float _GetX(float xPos) { return floor(xPos * SCREEN_WIDTH / BASE_WIDTH); }
	static const float _GetY(float yPos) { return floor(yPos * SCREEN_HEIGHT / BASE_HEIGHT); }
	static const float _GetZ(float zPos) { return zPos * SCREEN_WIDTH / BASE_WIDTH; }
	static const float _GetWidth(float width) { return width * SCREEN_WIDTH / BASE_WIDTH; }
	static const float _GetHeight(float height) { return height * SCREEN_HEIGHT / BASE_HEIGHT; }

	static Vector2 mScreenSize;
	static int mBaseWidth;
	static int mBaseHeight;
	static unsigned int mScreenResolutionType;

	static float mIndicatorHeight[RESOLUTION_BASE_MAX];

	static float mWidgetLoadingTextWidth[RESOLUTION_BASE_MAX];
	static float mWidgetLoadingTextSize[RESOLUTION_BASE_MAX];
	static float mWidgetDeleteButtonWidth[RESOLUTION_BASE_MAX];
	static float mWidgetDeleteButtonHeight[RESOLUTION_BASE_MAX];
	static float mWidgetDeleteButtonX[RESOLUTION_BASE_MAX];
	static float mWidgetDeleteButtonY[RESOLUTION_BASE_MAX];
	static float mWidgetDeleteButtonZ[RESOLUTION_BASE_MAX];
	static float mTappedBoundaryWidth[RESOLUTION_BASE_MAX];
	static float mTappedBoundaryHeight[RESOLUTION_BASE_MAX];
};
}
#endif /* _WIDGET_VIEWER_DALI_CONST_H_ */


