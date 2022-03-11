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

#include "widget_viewer_dali_const.h"

using namespace Dali;
using namespace WidgetViewerDali;

namespace WidgetViewerDali
{

const float SCREEN_RATIO[4] = { (4.0f/3.0f), (3.0f/2.0f), (5.0f/3.0f), (16.0f/9.0f) };
const int SCREEN_RESOLUTION[4][2] = { {480, 640}, {320, 480}, {480, 800}, {540, 960} };

Vector2 CWidgetDaliConst::mScreenSize = Vector2(BASE_WIDTH, BASE_HEIGHT);
int CWidgetDaliConst::mBaseWidth = 480;
int CWidgetDaliConst::mBaseHeight = 800;
unsigned int CWidgetDaliConst::mScreenResolutionType = CWidgetDaliConst::RESOLUTION_BASE_480X800;

/*
 0 : 480x640 - unsupported
 1 : 320x480 - unsupported
 2 : 480x800 - WVGA
 3 : 540x960 - qHD
*/
float CWidgetDaliConst::mIndicatorHeight[CWidgetDaliConst::RESOLUTION_BASE_MAX] = { 0.0f,  0.0f, 33.33f, 40.0f };
float CWidgetDaliConst::mWidgetLoadingTextWidth[CWidgetDaliConst::RESOLUTION_BASE_MAX] = { 0.0f,  0.0f, 470.0f, 528.75f };
float CWidgetDaliConst::mWidgetLoadingTextSize[CWidgetDaliConst::RESOLUTION_BASE_MAX] = { 0.0f,  0.0f, 18.0f, 21.65f };
float CWidgetDaliConst::mWidgetDeleteButtonWidth[CWidgetDaliConst::RESOLUTION_BASE_MAX] = { 0.0f,  0.0f, 40.0f, 45.0f };
float CWidgetDaliConst::mWidgetDeleteButtonHeight[CWidgetDaliConst::RESOLUTION_BASE_MAX] = { 0.0f,  0.0f, 40.0f, 48.0f };
float CWidgetDaliConst::mWidgetDeleteButtonX[CWidgetDaliConst::RESOLUTION_BASE_MAX] = { 0.0f,  0.0f, 15.0f, 16.875f };
float CWidgetDaliConst::mWidgetDeleteButtonY[CWidgetDaliConst::RESOLUTION_BASE_MAX] = { 0.0f,  0.0f, 15.0f, 18.0f };
float CWidgetDaliConst::mWidgetDeleteButtonZ[CWidgetDaliConst::RESOLUTION_BASE_MAX] = { 0.0f,  0.0f, 3.5f, 3.9375f };
float CWidgetDaliConst::mTappedBoundaryWidth[CWidgetDaliConst::RESOLUTION_BASE_MAX] = { 0.0f,  0.0f, 10.0f, 11.5f };
float CWidgetDaliConst::mTappedBoundaryHeight[CWidgetDaliConst::RESOLUTION_BASE_MAX] = { 0.0f,  0.0f, 10.0f, 12.0f };

CWidgetDaliConst::CWidgetDaliConst()
{
}

void CWidgetDaliConst::SetScreenSize(Vector2 size)
{
	mScreenSize = size;

	_SetBaseScreenSize(size.x, size.y);
}

void CWidgetDaliConst::_SetBaseScreenSize(int width, int height)
{
	unsigned int type = _GetResolutionType(width, height);

	if( width < height )
	{
		mBaseWidth = SCREEN_RESOLUTION[type][0];
		mBaseHeight = SCREEN_RESOLUTION[type][1];
	}
	else
	{
		mBaseWidth = SCREEN_RESOLUTION[type][1];
		mBaseHeight = SCREEN_RESOLUTION[type][0];
	}

	mScreenResolutionType = type;
}

const unsigned int CWidgetDaliConst::_GetResolutionType(int x, int y)
{
	if(x > y)
		return CWidgetDaliConst::_GetResolutionType(y, x);

	float ratio = (float) y / (float) x;

	unsigned int type=0;
	for(type=0; type < RESOLUTION_BASE_MAX - 1; type++)
	{
		if( fabs(ratio - SCREEN_RATIO[type]) < fabs(ratio - SCREEN_RATIO[type+1]) )
			break;
	}

	return type;
}

const float CWidgetDaliConst::GetIndicatorHeight()
{
	return _GetHeight(mIndicatorHeight[mScreenResolutionType]);
}

const float CWidgetDaliConst::GetWidgetLoadingTextWidth()
{
	return _GetWidth(mWidgetLoadingTextWidth[mScreenResolutionType]);
}

const float CWidgetDaliConst::GetWidgetLoadingTextSize()
{
	return _GetHeight(mWidgetLoadingTextSize[mScreenResolutionType]);
}

const float CWidgetDaliConst::GetWidgetDeleteButtonWidth()
{
	return _GetWidth(mWidgetDeleteButtonWidth[mScreenResolutionType]);
}

const float CWidgetDaliConst::GetWidgetDeleteButtonHeight()
{
	return _GetHeight(mWidgetDeleteButtonHeight[mScreenResolutionType]);
}

const float CWidgetDaliConst::GetWidgetDeleteButtonX()
{
	return _GetX(mWidgetDeleteButtonX[mScreenResolutionType]);
}

const float CWidgetDaliConst::GetWidgetDeleteButtonY()
{
	return _GetY(mWidgetDeleteButtonY[mScreenResolutionType]);
}

const float CWidgetDaliConst::GetWidgetDeleteButtonZ()
{
	return _GetZ(mWidgetDeleteButtonZ[mScreenResolutionType]);
}

const float CWidgetDaliConst::GetTappedBoundaryWidth()
{
	return _GetWidth(mTappedBoundaryWidth[mScreenResolutionType]);
}

const float CWidgetDaliConst::GetTappedBoundaryHeight()
{
	return _GetHeight(mTappedBoundaryHeight[mScreenResolutionType]);
}

}

