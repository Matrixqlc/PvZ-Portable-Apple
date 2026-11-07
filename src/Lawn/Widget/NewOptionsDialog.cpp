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

#include "../Board.h"
#include "GameButton.h"
#include "../Cutscene.h"
#include "AlmanacDialog.h"
#include "../LawnCommon.h"
#include "../../LawnApp.h"
#include "../System/Music.h"
#include "../../Resources.h"
#include "NewOptionsDialog.h"
#include "../../ConstEnums.h"
#include "../../PvzpLib/PvzpFoley.h"
#include "widget/Slider.h"
#include "widget/Checkbox.h"
#include "graphics/Graphics.h"
#include "../../PvzpLib/PvzpStringFile.h"

using namespace Sexy;

namespace
{
#if defined(__IPHONEOS__) || defined(__APPLE__)
class GameSpeedButton : public LawnStoneButton
{
public:
	GameSpeedButton(int theId, ButtonListener* theListener) : LawnStoneButton(nullptr, theId, theListener) { }
	void Draw(Graphics* g) override
	{
		// The original stone art is 46 units high. Fit it to this option row
		// instead of clipping its lower edge or overlapping the next control.
		Graphics aGraphics(*g);
#if defined(__IPHONEOS__)
		const int aStoneWidth = mWidth;
#else
		// Keep a full center tile between the original stone end caps.
		const int aStoneWidth = IMAGE_BUTTON_LEFT->mWidth + IMAGE_BUTTON_MIDDLE->mWidth + IMAGE_BUTTON_RIGHT->mWidth;
#endif
		aGraphics.SetScale(static_cast<float>(mWidth) / aStoneWidth, static_cast<float>(mHeight) / 46.0f, 0.0f, 0.0f);
		DrawStoneButton(&aGraphics, 0, 0, aStoneWidth, 46, (mIsDown && mIsOver && !mDisabled) ^ mInverted, mIsOver, mLabel);
	}
};

std::string_view GameSpeedLabel(int theHalfUnits)
{
	switch (theHalfUnits)
	{
	case 1: return "0.5x";
	case 3: return "1.5x";
	case 4: return "2x";
	default: return "1x";
	}
}
#endif
}

NewOptionsDialog::NewOptionsDialog(LawnApp* theApp, bool theFromGameSelector) :
	Dialog(nullptr, nullptr, Dialogs::DIALOG_NEWOPTIONS, true, "Options", "", "", Dialog::BUTTONS_NONE)
{
	mApp = theApp;
	mFromGameSelector = theFromGameSelector;
	SetButtonTextColor(Color(255, 255, 100));
	mAlmanacButton = MakeButton(NewOptionsDialog::NewOptionsDialog_Almanac, this, "[VIEW_ALMANAC_BUTTON]");
	mRestartButton = MakeButton(NewOptionsDialog::NewOptionsDialog_Restart, this,
		theApp->GetString("RESTART_BUTTON", "Restart"));
	mBackToMainButton = MakeButton(NewOptionsDialog::NewOptionsDialog_MainMenu, this, "[MAIN_MENU_BUTTON]");

	mBackToGameButton = MakeNewButton(
		Dialog::ID_OK,
		this,
		"[BACK_TO_GAME]",
		nullptr,
		IMAGE_OPTIONS_BACKTOGAMEBUTTON0,
		IMAGE_OPTIONS_BACKTOGAMEBUTTON0,
		IMAGE_OPTIONS_BACKTOGAMEBUTTON2
	);
	mBackToGameButton->mTranslateX = 0;
	mBackToGameButton->mTranslateY = 0;
	mBackToGameButton->mTextOffsetX = -2;
	mBackToGameButton->mTextOffsetY = -5;
	mBackToGameButton->mTextDownOffsetX = 0;
	mBackToGameButton->mTextDownOffsetY = 1;
	mBackToGameButton->SetFont(FONT_DWARVENTODCRAFT36GREENINSET);
	mBackToGameButton->SetLabelColor(Color::White);
	mBackToGameButton->SetLabelHiliteColor(Color::White);
	mBackToGameButton->mHiliteFont = FONT_DWARVENTODCRAFT36BRIGHTGREENINSET;

	mMusicVolumeSlider = std::make_unique<Slider>(IMAGE_OPTIONS_SLIDERSLOT, IMAGE_OPTIONS_SLIDERKNOB2, NewOptionsDialog::NewOptionsDialog_MusicVolume, this);
	double aMusicVolume = theApp->GetMusicVolume();
	aMusicVolume = std::max(0.0, std::min(1.0, aMusicVolume));
	mMusicVolumeSlider->SetValue(aMusicVolume);

	mSfxVolumeSlider = std::make_unique<Slider>(IMAGE_OPTIONS_SLIDERSLOT, IMAGE_OPTIONS_SLIDERKNOB2, NewOptionsDialog::NewOptionsDialog_SoundVolume, this);
	mSfxVolumeSlider->SetValue(theApp->GetSfxVolume() / 0.65);

	mFullscreenCheckbox = MakeNewCheckbox(NewOptionsDialog::NewOptionsDialog_Fullscreen, this, !theApp->mIsWindowed);
	mHardwareAccelerationCheckbox = MakeNewCheckbox(NewOptionsDialog::NewOptionsDialog_HardwareAcceleration, this, theApp->Is3DAccelerated());

#if defined(__IPHONEOS__)
	// iOS always uses the accelerated fullscreen renderer. Use these two
	// option rows for gameplay settings instead of desktop display switches.
	mHardwareAccelerationCheckbox->SetVisible(false);
	mFullscreenCheckbox->SetVisible(false);
#endif
#if defined(__IPHONEOS__) || defined(__APPLE__)
	mAutoCollectSunCheckbox = MakeNewCheckbox(NewOptionsDialog_AutoCollectSun, this, theApp->mAutoCollectSun);
	mAutoCollectMoneyCheckbox = MakeNewCheckbox(NewOptionsDialog_AutoCollectMoney, this, theApp->mAutoCollectMoney);
	mShowHealthCheckbox = MakeNewCheckbox(NewOptionsDialog_ShowHealth, this, theApp->mShowHealth);
	mGameSpeedButton = std::make_unique<GameSpeedButton>(NewOptionsDialog_GameSpeed, this);
	mGameSpeedButton->SetLabel(GameSpeedLabel(theApp->mGameSpeedHalfUnits));
	mGameSpeedButton->mHasAlpha = true;
	mGameSpeedButton->mHasTransparencies = true;
#endif

	if (mFromGameSelector)
	{
		mRestartButton->SetVisible(false);
		mBackToGameButton->SetLabel("[DIALOG_BUTTON_OK]");
		if (mApp->HasFinishedAdventure() && !mApp->IsTrialStageLocked())
		{
			mBackToMainButton->SetLabel("[CREDITS]");
		}
		else
		{
			mBackToMainButton->SetVisible(false);
		}
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ICE ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		mRestartButton->SetVisible(false);
	}
	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && !mApp->mBoard->mCutScene->IsSurvivalRepick())
	{
		mRestartButton->SetVisible(false);
	}
	if (!mApp->CanShowAlmanac() ||
		mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO ||
		mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
		mFromGameSelector)
	{
		mAlmanacButton->SetVisible(false);
	}
}

NewOptionsDialog::~NewOptionsDialog() = default;

int NewOptionsDialog::GetPreferredHeight([[maybe_unused]] int theWidth)
{
	return IMAGE_OPTIONS_MENUBACK->mHeight
#if defined(__IPHONEOS__)
		+ 82
#elif defined(__APPLE__)
			+ 102
#endif
		;
}

void NewOptionsDialog::AddedToManager(Sexy::WidgetManager* theWidgetManager)
{
	Dialog::AddedToManager(theWidgetManager);
	AddWidget(mAlmanacButton.get());
	AddWidget(mRestartButton.get());
	AddWidget(mBackToMainButton.get());
	AddWidget(mMusicVolumeSlider.get());
	AddWidget(mSfxVolumeSlider.get());
	AddWidget(mHardwareAccelerationCheckbox.get());
	AddWidget(mFullscreenCheckbox.get());
	AddWidget(mBackToGameButton.get());
	if (mAutoCollectSunCheckbox)
		AddWidget(mAutoCollectSunCheckbox.get());
	if (mAutoCollectMoneyCheckbox)
		AddWidget(mAutoCollectMoneyCheckbox.get());
	if (mShowHealthCheckbox)
		AddWidget(mShowHealthCheckbox.get());
	if (mGameSpeedButton)
		AddWidget(mGameSpeedButton.get());
}

void NewOptionsDialog::RemovedFromManager(Sexy::WidgetManager* theWidgetManager)
{
	Dialog::RemovedFromManager(theWidgetManager);
	RemoveWidget(mAlmanacButton.get());
	RemoveWidget(mMusicVolumeSlider.get());
	RemoveWidget(mSfxVolumeSlider.get());
	RemoveWidget(mFullscreenCheckbox.get());
	RemoveWidget(mHardwareAccelerationCheckbox.get());
	RemoveWidget(mBackToMainButton.get());
	RemoveWidget(mBackToGameButton.get());
	RemoveWidget(mRestartButton.get());
	if (mAutoCollectSunCheckbox)
		RemoveWidget(mAutoCollectSunCheckbox.get());
	if (mAutoCollectMoneyCheckbox)
		RemoveWidget(mAutoCollectMoneyCheckbox.get());
	if (mShowHealthCheckbox)
		RemoveWidget(mShowHealthCheckbox.get());
	if (mGameSpeedButton)
		RemoveWidget(mGameSpeedButton.get());
}

void NewOptionsDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
	Dialog::Resize(theX, theY, theWidth, theHeight);
	mMusicVolumeSlider->Resize(199, 116, 135, 40);
	mSfxVolumeSlider->Resize(199, 143, 135, 40);
	mHardwareAccelerationCheckbox->Resize(283, 175, 46, 45);
	mFullscreenCheckbox->Resize(284, 206, 46, 45);
	mAlmanacButton->Resize(107, 241, 209, 46);
	mRestartButton->Resize(mAlmanacButton->mX, mAlmanacButton->mY + 43, 209, 46);
	mBackToMainButton->Resize(mRestartButton->mX, mRestartButton->mY + 43, 209, 46);
	mBackToGameButton->Resize(30, 381, mBackToGameButton->mWidth, mBackToGameButton->mHeight);

	if (mFromGameSelector)
	{
		mMusicVolumeSlider->mY += 5;
		mSfxVolumeSlider->mY += 10;
		mHardwareAccelerationCheckbox->mY += 15;
		mFullscreenCheckbox->mY += 20;
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		mAlmanacButton->mY += 43;
	}

#if defined(__APPLE__) && !defined(__IPHONEOS__)
	// Use the original slider thumb height to make room for a row per gameplay option.
	mMusicVolumeSlider->Resize(199, 106, 135, 29);
	mSfxVolumeSlider->Resize(199, 135, 135, 29);
	mAutoCollectSunCheckbox->Resize(283, 164, 42, 39);
	mGameSpeedButton->Resize(283, 203, 83, 29);
	mShowHealthCheckbox->Resize(283, 232, 42, 39);
	mAutoCollectMoneyCheckbox->Resize(283, 271, 42, 39);
	mHardwareAccelerationCheckbox->Resize(161, 310, 42, 39);
	mFullscreenCheckbox->Resize(305, 310, 42, 39);
	mAlmanacButton->mY += 113;
	mRestartButton->mY += 113;
	mBackToMainButton->mY += 113;
	mBackToGameButton->mY += 105;
#elif defined(__IPHONEOS__)
	if (mAutoCollectSunCheckbox)
	{
		// Keep the hit regions disjoint from each other and the almanac row.
		mAutoCollectSunCheckbox->Resize(283, 170 + (mFromGameSelector ? 15 : 0), 42, 39);
		mGameSpeedButton->Resize(251, 210 + (mFromGameSelector ? 20 : 0), 83, 30);
		mShowHealthCheckbox->Resize(283, 247 + (mFromGameSelector ? 15 : 0), 42, 39);
		mAutoCollectMoneyCheckbox->Resize(283, 287 + (mFromGameSelector ? 15 : 0), 42, 39);
		constexpr int aExtraRowsHeight = 82;
		mAlmanacButton->mY += aExtraRowsHeight;
		mRestartButton->mY += aExtraRowsHeight;
		mBackToMainButton->mY += aExtraRowsHeight;
		mBackToGameButton->mY += aExtraRowsHeight;
		if (!mFromGameSelector)
		{
			mAlmanacButton->mY += 4;
			mRestartButton->mY += 4;
			mBackToMainButton->mY += 4;
		}
	}
#endif
}

void NewOptionsDialog::Draw(Sexy::Graphics* g)
{
#if defined(__IPHONEOS__) || defined(__APPLE__)
	// Extend only the quiet stone middle; preserve the title and footer artwork.
	const int w = IMAGE_OPTIONS_MENUBACK->mWidth;
	const int h = IMAGE_OPTIONS_MENUBACK->mHeight;
	const int extraHeight = GetPreferredHeight(w) - h;
	g->DrawImage(IMAGE_OPTIONS_MENUBACK, 0, 0, Rect(0, 0, w, 180));
	g->DrawImage(IMAGE_OPTIONS_MENUBACK, Rect(0, 180, w, 180 + extraHeight), Rect(0, 180, w, 180));
	g->DrawImage(IMAGE_OPTIONS_MENUBACK, 0, 360 + extraHeight, Rect(0, 360, w, h - 360));
#else
	g->DrawImage(IMAGE_OPTIONS_MENUBACK, 0, 0);
#endif

#if !defined(__APPLE__) || defined(__IPHONEOS__)
	int aMusicOffset = 0;
	int aSfxOffset = 0;
	int a3DAccelOffset = 0;
	int aFullScreenOffset = 0;
	if (mFromGameSelector)
	{
		aMusicOffset = 5;
		aSfxOffset = 10;
		a3DAccelOffset = 15;
		aFullScreenOffset = 20;
	}
#endif
	Sexy::Color aTextColor(107, 109, 145);

	int aSliderLabelsX = mApp->GetInteger("OPTION_DLG_SLIDER_LABELS_OFFSET_X", 186);
	int aCheckboxLabelsX = mApp->GetInteger("OPTION_DLG_CHECKBOX_LABELS_OFFSET_X", 274);
	float aFontScale = static_cast<float>(mApp->GetDouble("OPTION_DLG_LABEL_FONT_SCALE", 1.0));
	if (aFontScale != 1.0f)
		g->SetScale(aFontScale, aFontScale, 0.0f, 0.0f);
#if defined(__APPLE__) && !defined(__IPHONEOS__)
	const bool aChinese = mApp->GetString("LOCALE", "").starts_with("Chinese");
	const auto drawLabel = [&](const std::string& label, int x, int y)
	{
		PvzpDrawString(g, label, x, y, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
	};
	drawLabel(mApp->GetString("OPTIONS_MUSIC_LABEL", "Music"), aSliderLabelsX, mMusicVolumeSlider->mY + 19);
	drawLabel(mApp->GetString("OPTIONS_SOUNDFX", "Sound FX"), aSliderLabelsX, mSfxVolumeSlider->mY + 19);
	drawLabel(mApp->GetString("OPTIONS_AUTO_COLLECT_SUN", aChinese ? "自动收集阳光" : "Collect sun"), aCheckboxLabelsX, mAutoCollectSunCheckbox->mY + 22);
	drawLabel(mApp->GetString("OPTIONS_GAME_SPEED", aChinese ? "游戏速度" : "Game speed"), aCheckboxLabelsX, mGameSpeedButton->mY + 19);
	drawLabel(mApp->GetString("OPTIONS_SHOW_HEALTH", aChinese ? "显示生命" : "Show health"), aCheckboxLabelsX, mShowHealthCheckbox->mY + 22);
	drawLabel(mApp->GetString("OPTIONS_AUTO_COLLECT_MONEY", aChinese ? "自动收集金币" : "Collect coins"), aCheckboxLabelsX, mAutoCollectMoneyCheckbox->mY + 22);
	drawLabel(mApp->GetString("OPTIONS_3D_ACCELERATION", "3D Acceleration"), mHardwareAccelerationCheckbox->mX - 9, mHardwareAccelerationCheckbox->mY + 22);
	drawLabel(mApp->GetString("OPTIONS_FULL_SCREEN", "Full Screen"), mFullscreenCheckbox->mX - 9, mFullscreenCheckbox->mY + 22);
#else
	PvzpDrawString(g, mApp->GetString("OPTIONS_MUSIC_LABEL", "Music"), aSliderLabelsX, 140 + aMusicOffset, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
	PvzpDrawString(g, mApp->GetString("OPTIONS_SOUNDFX", "Sound FX"), aSliderLabelsX, 167 + aSfxOffset, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
#if defined(__IPHONEOS__)
	const bool aChinese = mApp->GetString("LOCALE", "").starts_with("Chinese");
	PvzpDrawString(g, mApp->GetString("OPTIONS_AUTO_COLLECT_SUN", aChinese ? "自动收集阳光" : "Collect sun"), aCheckboxLabelsX, 197 + a3DAccelOffset, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
	PvzpDrawString(g, mApp->GetString("OPTIONS_GAME_SPEED", aChinese ? "游戏速度" : "Game speed"), std::min(aCheckboxLabelsX, 240), 229 + aFullScreenOffset, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
	const int aHealthBaseline = 274 + (mFromGameSelector ? 15 : 0);
	// The original Chinese bitmap font contains no glyph for 血.
	PvzpDrawString(g, mApp->GetString("OPTIONS_SHOW_HEALTH", aChinese ? "显示生命" : "Show health"), aCheckboxLabelsX, aHealthBaseline, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
	PvzpDrawString(g, mApp->GetString("OPTIONS_AUTO_COLLECT_MONEY", aChinese ? "自动收集金币" : "Collect coins"), aCheckboxLabelsX, aHealthBaseline + 40, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
#else
	PvzpDrawString(g, mApp->GetString("OPTIONS_3D_ACCELERATION", "3D Acceleration"), aCheckboxLabelsX, 197 + a3DAccelOffset, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
	PvzpDrawString(g, mApp->GetString("OPTIONS_FULL_SCREEN", "Full Screen"), aCheckboxLabelsX, 229 + aFullScreenOffset, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
#endif
#endif
	if (aFontScale != 1.0f)
		g->SetScale(1.0f, 1.0f, 0.0f, 0.0f);
}

void NewOptionsDialog::SliderVal(int theId, double theVal)
{
	switch (theId)
	{
	case NewOptionsDialog::NewOptionsDialog_MusicVolume:
		mApp->SetMusicVolume(theVal);
		mApp->mSoundSystem->RehookupSoundWithMusicVolume();
		break;

	case NewOptionsDialog::NewOptionsDialog_SoundVolume:
		mApp->SetSfxVolume(theVal * 0.65);
		mApp->mSoundSystem->RehookupSoundWithMusicVolume();
		if (!mSfxVolumeSlider->mDragging)
		{
			mApp->PlaySample(SOUND_BUTTONCLICK);
		}
		break;
	}
}

void NewOptionsDialog::CheckboxChecked(int theId, bool checked)
{
	switch (theId)
	{
	case NewOptionsDialog_AutoCollectSun:
		mApp->SetAutoCollectSun(checked);
		break;
	case NewOptionsDialog_AutoCollectMoney:
		mApp->SetAutoCollectMoney(checked);
		break;
	case NewOptionsDialog_ShowHealth:
		mApp->SetShowHealth(checked);
		break;

	case NewOptionsDialog::NewOptionsDialog_Fullscreen:
		if (!checked && mApp->mForceFullscreen)
		{
			mApp->DoDialog(
				Dialogs::DIALOG_COLORDEPTH_EXP,
				true,
				mApp->GetString("NO_WINDOWED_MODE", "No Windowed Mode"),
				mApp->GetString("INVALID_WINDOWS_MODE",
					"Windowed mode is only available if your desktop was running in either\n"
					"16 bit or 32 bit color mode when you started the game.\n\n"
					"If you'd like to run in Windowed mode then you need to quit the game and switch your desktop to 16 or 32 bit color mode."),
				"[DIALOG_BUTTON_OK]",
				Dialog::BUTTONS_FOOTER
			);

			mFullscreenCheckbox->SetChecked(true, false);
		}
		break;

	case NewOptionsDialog::NewOptionsDialog_HardwareAcceleration:
		if (checked)
		{
			if (!mApp->Is3DAccelerationSupported())
			{
				mHardwareAccelerationCheckbox->SetChecked(false, false);
				mApp->DoDialog(
					Dialogs::DIALOG_INFO,
					true,
					mApp->GetString("NOT_SUPPORTED", "Not Supported"),
					mApp->GetString("HARDWARE_ACCELERATION_NOT_SUPPORTED",
						"Hardware Acceleration cannot be enabled on this computer.\n\n"
						"Your video card does not\n"
						"meet the minimum requirements\n"
						"for this game."),
					"[DIALOG_BUTTON_OK]",
					Dialog::BUTTONS_FOOTER
				);
			}
			else if (!mApp->Is3DAccelerationRecommended())
			{
				mApp->DoDialog(
					Dialogs::DIALOG_INFO,
					true,
					mApp->GetString("DIALOG_WARNING", "Warning"),
					mApp->GetString("SLOW_PERFORMANCE",
						"Your video card may not fully support this feature.\n\n"
						"If you experience slower performance, please disable Hardware Acceleration."),
					"[DIALOG_BUTTON_OK]",
					Dialog::BUTTONS_FOOTER
				);
			}
		}
		break;
	}
}

void NewOptionsDialog::KeyDown(Sexy::KeyCode theKey)
{
	if (mApp->mBoard)
	{
		mApp->mBoard->DoTypingCheck(theKey);
	}

	if (theKey == KeyCode::KEYCODE_SPACE || theKey == KeyCode::KEYCODE_RETURN)
	{
		Dialog::ButtonDepress(Dialog::ID_OK);
	}
	else if (theKey == KeyCode::KEYCODE_ESCAPE)
	{
		Dialog::ButtonDepress(Dialog::ID_CANCEL);
	}
}

void NewOptionsDialog::ButtonPress([[maybe_unused]] int theId)
{
	mApp->PlaySample(SOUND_GRAVEBUTTON);
}

void NewOptionsDialog::ButtonDepress(int theId)
{
#if defined(__IPHONEOS__) || defined(__APPLE__)
	if (theId == NewOptionsDialog_GameSpeed)
	{
		mApp->SetGameSpeedHalfUnits(mApp->mGameSpeedHalfUnits % 4 + 1);
		mGameSpeedButton->SetLabel(GameSpeedLabel(mApp->mGameSpeedHalfUnits));
		return;
	}
#endif
	Dialog::ButtonDepress(theId);

	switch (theId)
	{
	case NewOptionsDialog::NewOptionsDialog_Almanac:
	{
		AlmanacDialog* aDialog = mApp->DoAlmanacDialog(SeedType::SEED_NONE, ZombieType::ZOMBIE_INVALID);
		aDialog->WaitForResult(true);
		break;
	}

	case NewOptionsDialog::NewOptionsDialog_MainMenu:
	{
		if (mFromGameSelector)
		{
			mApp->KillNewOptionsDialog();
			mApp->KillGameSelector();
			mApp->ShowAwardScreen(AwardType::AWARD_CREDITS_ZOMBIENOTE, false);
		}
		else if (mApp->mBoard && mApp->mBoard->NeedSaveGame())
		{
			mApp->DoConfirmBackToMain();
		}
		else if (mApp->mBoard && mApp->mBoard->mCutScene && mApp->mBoard->mCutScene->IsSurvivalRepick())
		{
			mApp->DoConfirmBackToMain();
		}
		else
		{
			mApp->mBoardResult = BoardResult::BOARDRESULT_QUIT;
			mApp->DoBackToMain();
		}
		break;
	}

	case NewOptionsDialog::NewOptionsDialog_Restart:
	{
		if (mApp->mBoard)
		{
			std::string aDialogTitle;
			std::string aDialogMessage;
			if (mApp->IsPuzzleMode())
			{
				aDialogTitle = "[RESTART_PUZZLE_HEADER]";
				aDialogMessage = "[RESTART_PUZZLE_BODY]";
			}
			else if (mApp->IsChallengeMode())
			{
				aDialogTitle = "[RESTART_CHALLENGE_HEADER]";
				aDialogMessage = "[RESTART_CHALLENGE_BODY]";
			}
			else if (mApp->IsSurvivalMode())
			{
				aDialogTitle = "[RESTART_SURVIVAL_HEADER]";
				aDialogMessage = "[RESTART_SURVIVAL_BODY]";
			}
			else
			{
				aDialogTitle = "[RESTART_LEVEL_HEADER]";
				aDialogMessage = "[RESTART_LEVEL_BODY]";
			}

			LawnDialog* aDialog = (LawnDialog*)mApp->DoDialog(Dialogs::DIALOG_CONFIRM_RESTART, true, aDialogTitle, aDialogMessage, "", Dialog::BUTTONS_YES_NO);
			aDialog->mLawnYesButton->mLabel = mApp->GetString("RESTART_LABEL", "RESTART");
			aDialog->mLawnNoButton->mLabel = PvzpStringTranslate("[DIALOG_BUTTON_CANCEL]");

			if (aDialog->WaitForResult(true) == Dialog::ID_YES)
			{
				mApp->mMusic->StopAllMusic();
				mApp->mSoundSystem->CancelPausedFoley();
				mApp->KillNewOptionsDialog();
				mApp->mBoardResult = BoardResult::BOARDRESULT_RESTART;
				mApp->mSawYeti = mApp->mBoard->mKilledYeti;
				mApp->PreNewGame(mApp->mGameMode, false);
			}
		}
		break;
	}

	case NewOptionsDialog::NewOptionsDialog_Update:
		mApp->CheckForUpdates();
		break;
	}
}
