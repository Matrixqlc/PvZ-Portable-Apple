/*
 * Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of PvZ-Portable.
 *
 * PvZ-Portable is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PvZ-Portable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with PvZ-Portable. If not, see <https://www.gnu.org/licenses/>.
 */

#include <time.h>
#include <format>
#include "Board.h"
#include "Plant.h"
#include "../LawnApp.h"
#include "LawnCommon.h"
#include "../Resources.h"
#include "../GameConstants.h"
#include "../PvzpLib/PvzpCommon.h"
#include "graphics/Font.h"
#include "widget/Dialog.h"
#include "misc/SexyMatrix.h"
#include "widget/Checkbox.h"
#include "../PvzpLib/PvzpStringFile.h"

void DrawHealthBar(Graphics* g, int theCenterX, int theY, int64_t theHealth, int64_t theMaxHealth)
{
	if (theMaxHealth <= 0 || theHealth <= 0)
		return;
	const int64_t aHealth = std::min(theHealth, theMaxHealth);
	const float aFraction = static_cast<float>(aHealth) / static_cast<float>(theMaxHealth);
	Graphics aGraphics(*g);
	const int x = theCenterX - 25;
	aGraphics.SetColor(Color(25, 25, 25, 220));
	aGraphics.FillRect(x, theY, 50, 7);
	aGraphics.SetColor(aFraction > 0.5f ? Color(75, 210, 70) : aFraction > 0.25f ? Color(245, 190, 40) : Color(235, 65, 50));
	aGraphics.FillRect(x + 1, theY + 1, std::max(1, static_cast<int>(48 * aFraction)), 5);
	const auto aText = std::format("{}/{}", aHealth, theMaxHealth);
	PvzpDrawString(&aGraphics, aText, theCenterX + 1, theY - 1, FONT_BRIANNETOD12, Color(20, 20, 20), DrawStringJustification::DS_ALIGN_CENTER);
	PvzpDrawString(&aGraphics, aText, theCenterX, theY - 2, FONT_BRIANNETOD12, Color::White, DrawStringJustification::DS_ALIGN_CENTER);
}

static constexpr EditWidgetColorScheme gLawnEditWidgetColors{
	.mBkg = Color(0, 0, 0, 0),
	.mOutline = Color(0, 0, 0, 0),
	.mText = Color(240, 240, 255, 255),
	.mHilite = Color(255, 255, 255, 255),
	.mHiliteText = Color(0, 0, 0, 255),
};

// returns whether [theNumber - theRange, theNumber + theRange] contains a multiple of theMod
bool ModInRange(int theNumber, int theMod, int theRange)
{
	theRange = abs(theRange);
	for (int i = theNumber - theRange; i <= theNumber + theRange; i++)
		if (i % theMod == 0) return true;
	return false;
}

// returns whether (x1, y1) is within (theRangeX, theRangeY) of (x2, y2)
bool GridInRange(int x1, int y1, int x2, int y2, int theRangeX, int theRangeY)
{
	return x1 >= x2 - theRangeX && x1 <= x2 + theRangeX && y1 >= y2 - theRangeY && y1 <= y2 + theRangeY;
}

void TileImageHorizontally(Graphics* g, Image* theImage, int theX, int theY, int theWidth)
{
	while (theWidth > 0)
	{
		int aImageWidth = std::min(theWidth, theImage->GetWidth());
		g->DrawImage(theImage, theX, theY, Rect(0, 0, aImageWidth, theImage->GetHeight()));
		theX += aImageWidth;
		theWidth -= aImageWidth;
	}
}

void TileImageVertically(Graphics* g, Image* theImage, int theX, int theY, int theHeight)
{
	while (theHeight > 0)
	{
		int aImageHeight = std::min(theHeight, theImage->GetHeight());
		g->DrawImage(theImage, theX, theY, Rect(0, 0, theImage->GetWidth(), aImageHeight));
		theY += aImageHeight;
		theHeight -= aImageHeight;
	}
}

LawnEditWidget::LawnEditWidget(int theId, EditListener* theListener, Dialog* theDialog) : EditWidget(theId, theListener)
{
	mDialog = theDialog;
	mAutoCapFirstLetter = true;
}

LawnEditWidget::~LawnEditWidget()
{
}

void LawnEditWidget::KeyDown(KeyCode theKey)
{
	EditWidget::KeyDown(theKey);
	if (theKey == KeyCode::KEYCODE_ESCAPE)
		mDialog->KeyDown(KeyCode::KEYCODE_ESCAPE);
}

// Uppercase ASCII letters in place; locale-independent, safe on UTF-8 bytes.
static bool AutoCapChar(char& theChar)
{
	if (theChar >= 'a' && theChar <= 'z')
	{
		theChar = theChar - 'a' + 'A';
		return true;
	}

	return theChar >= 'A' && theChar <= 'Z';
}

void LawnEditWidget::KeyChar(char theChar)
{
	if (mAutoCapFirstLetter && AutoCapChar(theChar))
		mAutoCapFirstLetter = false;

	EditWidget::KeyChar(theChar);
}

void LawnEditWidget::KeyText(std::string_view theText)
{
	if (!mAutoCapFirstLetter)
	{
		EditWidget::KeyText(theText);
		return;
	}

	std::string aText(theText);
	for (char& aCh : aText)
	{
		if (AutoCapChar(aCh))
		{
			mAutoCapFirstLetter = false;
			break;
		}
	}

	EditWidget::KeyText(aText);
}

std::unique_ptr<LawnEditWidget> CreateEditWidget(int theId, EditListener* theListener, Dialog* theDialog)
{
	auto aEditWidget = std::make_unique<LawnEditWidget>(theId, theListener, theDialog);
	aEditWidget->SetFont(Sexy::FONT_BRIANNETOD16);
	aEditWidget->SetColors(gLawnEditWidgetColors);
	aEditWidget->mBlinkDelay = 14;

	return aEditWidget;
}

void DrawEditBox(Graphics* g, EditWidget* theWidget)
{
	Rect aDest(theWidget->mX - 8, theWidget->mY - 4, theWidget->mWidth + 16, theWidget->mHeight + 8);
	g->DrawImageBox(aDest, IMAGE_EDITBOX);
}

std::unique_ptr<Checkbox> MakeNewCheckbox(int theId, CheckboxListener* theListener, bool theDefault)
{
	auto aCheckbox = std::make_unique<Checkbox>(Sexy::IMAGE_OPTIONS_CHECKBOX0, Sexy::IMAGE_OPTIONS_CHECKBOX1, theId, theListener);
	aCheckbox->mChecked = theDefault;
	aCheckbox->mHasAlpha = true;
	aCheckbox->mHasTransparencies = true;

	return aCheckbox;
}

std::string GetSavedGameName(GameMode theGameMode, int theProfileId)
{
	return GetAppDataPath(std::format("userdata/game{}_{}.v4", theProfileId, static_cast<int>(theGameMode)));
}

std::string GetLegacySavedGameName(GameMode theGameMode, int theProfileId)
{
	return GetAppDataPath(std::format("userdata/game{}_{}.dat", theProfileId, static_cast<int>(theGameMode)));
}

std::string GetAdventureReplaySaveName(int theProfileId, int theLevel)
{
	return GetAppDataPath(std::format("userdata/replay{}_{}.v4", theProfileId, theLevel));
}

int GetCurrentDaysSince2000(time_t theTime)
{
	tm aNowTM = gLawnApp->GetLocalTime(theTime);

	int dy = aNowTM.tm_year - 100;
	return dy * 365 + (dy - 1) / 400 - (dy - 1) / 100 + (dy - 1) / 4 + aNowTM.tm_yday + 1;
}
