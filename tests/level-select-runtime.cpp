// SPDX-License-Identifier: LGPL-3.0-or-later
// Optional native integration harness. Link with the game's native objects in
// place of main.cpp, and pass -resdir and a DISPOSABLE -savedir copied for tests.
// Exercises real Board initialization, V4 storage and completion, not a played win.
#include "LawnApp.h"
#include "Resources.h"
#include "Lawn/Board.h"
#include "Lawn/Coin.h"
#include "Lawn/Plant.h"
#include "Lawn/Zombie.h"
#include "Lawn/LawnCommon.h"
#include "Lawn/System/PlayerInfo.h"
#include "Lawn/Widget/LevelSelector.h"
#include "Lawn/Widget/GameSelector.h"
#include "Lawn/Widget/AwardScreen.h"
#include "Lawn/Widget/NewOptionsDialog.h"
#include "Lawn/Widget/GameButton.h"
#include "PvzpLib/PvzpStringFile.h"
#include "misc/Buffer.h"
#include "widget/Dialog.h"
#include "widget/WidgetManager.h"
#include "widget/Checkbox.h"
#include "graphics/Graphics.h"
#include "graphics/MemoryImage.h"
#include "graphics/ImageFont.h"
#include "imagelib/ImageLib.h"
#include <cassert>
#include <cstdio>
#include <format>
#include <vector>

class LevelSelectTestApp : public LawnApp
{
    bool mRan = false;
    bool mPreview = false;
public:
    LevelSelectTestApp() { mTitle = "PvZ Star Result Check"; }
    void HandleCmdLineParam(std::string_view name, std::string_view value) override
    {
        if (name == "-starpreview") mPreview = true;
        else LawnApp::HandleCmdLineParam(name, value);
    }
    void Snapshot(const std::string& name)
    {
        Sexy::MemoryImage image(this);
        image.Create(800, 600);
        Sexy::Graphics graphics(&image);
        mWidgetManager->DrawWidgetsTo(&graphics);
        ImageLib::Image output;
        output.mWidth = 800; output.mHeight = 600;
        output.mBits = std::make_unique<uint32_t[]>(800 * 600);
        std::copy_n(image.GetBits(), 800 * 600, output.mBits.get());
        assert(ImageLib::WritePNGImage(GetAppDataPath(name), &output));
    }
    void UpdateFrames() override
    {
        LawnApp::UpdateFrames();
        if (mRan || !mLoadingThreadCompleted.load()) return;
        mRan = true;
        if (mTitleScreen) LoadingCompleted();
        assert(mPlayerInfo);
        if (mPreview)
        {
            // Interactive demonstration in an explicitly disposable save copy.
            const int level = mPlayerInfo->mLevel == 1 ? 2 : 1;
            mPlayerInfo->mFinishedAdventure = 1;
            RegistryWriteInteger(std::format("AdventureStarsV1_{}_{}", mPlayerInfo->mId, level), 2);
            ClearPendingAdventureStars(level, 2);
            StartAdventureLevel(level);
            mBoard->mTriggeredLawnMowers = 0;
            mBoard->mLevelComplete = true;
            CheckForGameEnd();
            return;
        }
        mActive = true; // Controlled foreground clock for deterministic UI stepping.
        mMinimized = false;
        for (auto* resource : {Sexy::FONT_BRIANNETOD12, Sexy::FONT_BRIANNETOD16, Sexy::FONT_DWARVENTODCRAFT24})
        {
            auto* font = dynamic_cast<Sexy::ImageFont*>(resource);
            assert(font);
            for (char32_t c : U"关卡完成本次星级未触发救场车最高新记录！")
            {
                if (!c) continue;
                bool found = false;
                for (const auto& layer : font->mFontData->mFontLayerList)
                    found |= layer.mCharDataMap.contains(c);
                if (!found) std::printf("MISSING GLYPH %s U+%04X\n", font->mFontData->mSourceFile.c_str(), unsigned(c));
                assert(found);
            }
        }
        const auto* badgeFont = static_cast<Sexy::ImageFont*>(Sexy::FONT_BRIANNETOD12);
        for (char32_t c : std::u32string_view(U"获得再战防守标记"))
        {
            bool found = false;
            for (const auto& layer : badgeFont->mFontData->mFontLayerList)
                found |= layer.mCharDataMap.contains(c);
            if (!found) std::printf("MISSING BADGE GLYPH U+%04X\n", unsigned(c));
            assert(found);
        }
        const int mainLevel = mPlayerInfo->mLevel;
        const int finished = mPlayerInfo->mFinishedAdventure;
        const auto mainSave = GetSavedGameName(GameMode::GAMEMODE_ADVENTURE, mPlayerInfo->mId);
        const auto legacySave = GetLegacySavedGameName(GameMode::GAMEMODE_ADVENTURE, mPlayerInfo->mId);
        if (!FileExists(mainSave))
        {
            // Create a real primary-board fixture so byte preservation is tested
            // against meaningful data, rather than only the absence of a file.
            KillGameSelector();
            PreNewGame(GameMode::GAMEMODE_ADVENTURE, false);
            mBoard->SaveGame(mainSave);
            mBoardResult = BoardResult::BOARDRESULT_QUIT;
            KillBoard();
        }
        const auto read = [&](const std::string& path) {
            Sexy::Buffer b;
            ReadBufferFromFile(path, &b, false);
            return b.GetDataLen() ? std::vector<uint8_t>(b.GetDataPtr(), b.GetDataPtr() + b.GetDataLen()) : std::vector<uint8_t>{};
        };
        const auto mainBytes = read(mainSave), legacyBytes = read(legacySave);
        // Unlock later chapters in this disposable fixture only.
        mPlayerInfo->mFinishedAdventure = 1;
        for (int candidate : {1, 15, 35, 50})
        {
            const int level = candidate == mainLevel ? (candidate == 50 ? 49 : candidate + 1) : candidate;
            RegistryEraseValue(std::format("AdventureStarsV1_{}_{}", mPlayerInfo->mId, level));
            RegistryEraseValue(std::format("AdventureStarArrivalV1_{}_{}", mPlayerInfo->mId, level));
            ShowLevelSelector(level);
            Snapshot("selector-" + std::to_string(level) + ".png");
            DoDialog(Dialogs::DIALOG_MESSAGE, true, "Rules", "Test", "", Sexy::Dialog::BUTTONS_FOOTER);
            KillDialog(Dialogs::DIALOG_MESSAGE);
            assert(mWidgetManager->mFocusWidget == mLevelSelector.get());
            StartAdventureLevel(level);
            assert(mBoard && mBoard->mLevel == level && mBoard->mIsAdventureReplay);
            assert(mBoard->mAdventurePostClearRun);
            assert(mPlayerInfo->mLevel == mainLevel && mPlayerInfo->mFinishedAdventure == 1);
            const auto replay = GetAdventureReplaySaveName(mPlayerInfo->mId, level);
            mBoard->mTriggeredLawnMowers = 1;
            mBoard->SaveGame(replay);
            mBoardResult = BoardResult::BOARDRESULT_QUIT;
            KillBoard();
            MakeNewBoard();
            assert(mBoard->LoadGame(replay));
            assert(mBoard->mIsAdventureReplay && mBoard->mLevel == level && mBoard->mTriggeredLawnMowers == 1);
            assert(mBoard->mAdventurePostClearRun);
            // Drive the actual completion path with a controlled completed Board.
            mBoard->mLevelComplete = true;
            CheckForGameEnd();
            assert(mLevelSelector && !mBoard && GetAdventureStars(level) >= 2);
            assert(!HasAdventureDefenseBadge(level));
            assert(mLevelSelector->IsShowingStarResult() && GetPendingAdventureStars(level) == 0);
            for (int i = 0; i < 125; ++i) mLevelSelector->Update();
            Snapshot("star-result-" + std::to_string(level) + ".png");
            mLevelSelector->KeyDown(Sexy::KEYCODE_RETURN);
            assert(mLevelSelector && !mLevelSelector->IsShowingStarResult());
            assert(GetPendingAdventureStars(level) == 0);
            mActive = false;
            for (int i = 0; i < 150; ++i) mLevelSelector->Update();
            assert(GetPendingAdventureStars(level) == 0);
            mActive = true;
            Snapshot("star-arrival-before-" + std::to_string(level) + ".png");
            for (int i = 0; i < 34; ++i) mLevelSelector->Update();
            Snapshot("star-arrival-during-" + std::to_string(level) + ".png");
            for (int i = 0; i < 150; ++i) mLevelSelector->Update();
            assert(GetPendingAdventureStars(level) == -1);
            Snapshot("star-arrival-after-" + std::to_string(level) + ".png");
            ShowLevelSelector(level);
            assert(GetPendingAdventureStars(level) == -1);
            assert(mPlayerInfo->mLevel == mainLevel && mPlayerInfo->mFinishedAdventure == 1);
            assert(!FileExists(replay));
            assert(read(mainSave) == mainBytes && read(legacySave) == legacyBytes);
        }
        // A pre-existing V4 main save has no replay flag and must still load normally.
        if (!mainBytes.empty())
        {
            KillLevelSelector();
            MakeNewBoard();
            assert(mBoard->LoadGame(mainSave));
            assert(!mBoard->mIsAdventureReplay && !mBoard->mAdventurePostClearRun);
            mBoardResult = BoardResult::BOARDRESULT_QUIT;
            KillBoard();
        }
        mPlayerInfo->mFinishedAdventure = finished;
        assert(read(mainSave) == mainBytes && read(legacySave) == legacyBytes);
        std::puts("PASS real native game: four chapter/special replay Boards initialized, V4 round trip, completion opens star result, return animates and acknowledges new stars once, main progress/save bytes preserved, primary V4 loads, dialog focus restored");
        // A worse run still has a result, but preserves best stars without a new arrival.
        const int replayLevel = mainLevel == 1 ? 2 : 1;
        mPlayerInfo->mFinishedAdventure = 1;
        // Improve an acknowledged two-star best: only the third slot is new.
        assert(GetAdventureStars(replayLevel) == 2);
        StartAdventureLevel(replayLevel);
        mBoard->mTriggeredLawnMowers = 0;
        mBoard->mLevelComplete = true;
        CheckForGameEnd();
        assert(mLevelSelector->IsShowingStarResult() && GetAdventureStars(replayLevel) == 3);
        assert(HasAdventureDefenseBadge(replayLevel));
        assert(GetPendingAdventureStars(replayLevel) == 2);
        for (int i = 0; i < 150; ++i) mLevelSelector->Update();
        Snapshot("star-result-three.png");
        mLevelSelector->KeyDown(Sexy::KEYCODE_RETURN);
        Snapshot("star-upgrade-before.png");
        ShowLevelSelector(replayLevel); // Leaving early must not lose the arrival.
        assert(GetPendingAdventureStars(replayLevel) == 2);
        for (int i = 0; i < 150; ++i) mLevelSelector->Update();
        assert(GetPendingAdventureStars(replayLevel) == -1);
        Snapshot("selector-defense-badge.png");
        RegistryWriteInteger(std::format("AdventureStarsV1_{}_{}", mPlayerInfo->mId, replayLevel), 3);
        ClearPendingAdventureStars(replayLevel, 3);
        StartAdventureLevel(replayLevel);
        mBoard->mTriggeredLawnMowers = 2;
        mBoard->mLevelComplete = true;
        CheckForGameEnd();
        assert(mLevelSelector->IsShowingStarResult() && GetAdventureStars(replayLevel) == 3);
        assert(GetPendingAdventureStars(replayLevel) == -1 && HasAdventureDefenseBadge(replayLevel));
        for (int i = 0; i < 125; ++i) mLevelSelector->Update();
        Snapshot("star-result-best-preserved.png");
        mLevelSelector->KeyDown(Sexy::KEYCODE_RETURN);
        // Main-line progression still grants the original plant reward after stars.
        KillLevelSelector();
        mPlayerInfo->mLevel = 1;
        mPlayerInfo->mFinishedAdventure = 0;
        PreNewGame(GameMode::GAMEMODE_ADVENTURE, false);
        mBoard->mTriggeredLawnMowers = 0;
        mBoard->mLevelComplete = true;
        CheckForGameEnd();
        assert(mLevelSelector->IsShowingStarResult() && mPlayerInfo->mLevel == 2);
        assert(!mAwardScreen);
        mLevelSelector->KeyDown(Sexy::KEYCODE_RETURN);
        assert(!mLevelSelector && mAwardScreen && mAwardScreen->mAwardType == AwardType::AWARD_FORLEVEL);
        KillAwardScreen();
        mPlayerInfo->mLevel = 50;
        PreNewGame(GameMode::GAMEMODE_ADVENTURE, false);
        assert(!mBoard->mAdventurePostClearRun && !HasAdventureDefenseBadge(50));
        mBoard->mTriggeredLawnMowers = 0;
        mBoard->mLevelComplete = true;
        CheckForGameEnd();
        assert(mPlayerInfo->mFinishedAdventure == 1 && GetAdventureStars(50) == 3 && !HasAdventureDefenseBadge(50));
        KillLevelSelector();
        mPlayerInfo->mLevel = mainLevel;
        mPlayerInfo->mFinishedAdventure = finished;
        std::puts("PASS defense badge: old V4 remains ineligible, post-clear V4 retains eligibility, mower use blocks the badge, first final win awards only stars");
        std::puts("PASS star results: two-to-three improvement animates only new third star, early leave/inactive window retains arrival, lower run preserves best, first main-line win advances once and continues to original plant reward");
#if defined(__APPLE__) && !defined(__IPHONEOS__)
		// Gameplay assists run only in the disposable integration profile.
		mPlayerInfo->mFinishedAdventure = 1;
		StartAdventureLevel(5);
		assert(mBoard);
		mGameScene = GameScenes::SCENE_PLAYING;
		mBoard->mPaused = false;
		mBoard->mX = 0;
		const int originalCoins = mPlayerInfo->mCoins;
		SetAutoCollectSun(false);
		SetAutoCollectMoney(true);
		int expectedCoins = originalCoins;
		for (auto type : {CoinType::COIN_SILVER, CoinType::COIN_GOLD, CoinType::COIN_DIAMOND})
		{
			auto* coin = mBoard->AddCoin(320, 250, type, CoinMotion::COIN_MOTION_COIN);
			coin->Update();
			assert(coin->mIsBeingCollected);
			for (int i = 0; i < 300 && !coin->mDead; ++i) coin->Update();
			expectedCoins += Coin::GetCoinValue(type);
			assert(coin->mDead && mPlayerInfo->mCoins == expectedCoins);
			// Board skips dead coins; exercise its real dispatcher after credit.
			mBoard->UpdateGameObjects();
			assert(mPlayerInfo->mCoins == expectedCoins);
		}
		assert(mBoard->mDiamondsCollected == 1 && mBoard->mLevelCoinsCollected == 2);
		auto* manualCoin = mBoard->AddCoin(400, 250, CoinType::COIN_GOLD, CoinMotion::COIN_MOTION_COIN);
		SetAutoCollectMoney(false);
		manualCoin->Update();
		assert(!manualCoin->mIsBeingCollected);
		manualCoin->MouseDown(0, 0, 1);
		assert(manualCoin->mIsBeingCollected);
		SetShowHealth(true);
		auto* plant = mBoard->AddPlant(2, 1, SeedType::SEED_WALLNUT);
		plant->mPlantHealth = plant->mPlantMaxHealth / 3;
		auto* zombie = mBoard->AddZombieInRow(ZombieType::ZOMBIE_TRAFFIC_CONE, 1, 0);
		zombie->mX = 550; zombie->mPosX = 550;
		zombie->mHelmHealth /= 2;
		zombie->UpdateReanim();
		Snapshot("gameplay-health-on.png");
		SetShowHealth(false);
		Snapshot("gameplay-health-off.png");
		DoNewOptions(false);
		Snapshot("gameplay-options.png");
		KillNewOptionsDialog();
		mBoardResult = BoardResult::BOARDRESULT_QUIT;
		KillBoard();
		mPlayerInfo->mCoins = originalCoins;
		mPlayerInfo->mLevel = mainLevel;
		mPlayerInfo->mFinishedAdventure = finished;
		std::puts("PASS real native assists: silver/gold/diamond animated collection credits exactly once, counters retained, off/manual path, health on/off rendering");
        ShowGameSelector();
        for (int i = 0; i < 250; ++i) mGameSelector->Update();
        Snapshot("main-menu.png");
        const bool autoSun = mAutoCollectSun;
        const int speed = mGameSpeedHalfUnits;
        DoNewOptions(true);
        auto* options = dynamic_cast<NewOptionsDialog*>(GetDialog(Dialogs::DIALOG_NEWOPTIONS));
        assert(options && options->mAutoCollectSunCheckbox && options->mGameSpeedButton);
        assert(options->mFullscreenCheckbox->mVisible && options->mHardwareAccelerationCheckbox->mVisible);
		assert(options->mAutoCollectMoneyCheckbox && options->mShowHealthCheckbox);
		const auto* optionsFont = dynamic_cast<Sexy::ImageFont*>(Sexy::FONT_DWARVENTODCRAFT18);
		assert(optionsFont);
		for (char32_t c : U"显示生命自动收集金币")
		{
			if (!c) continue;
			bool found = false;
			for (const auto& layer : optionsFont->mFontData->mFontLayerList)
				found |= layer.mCharDataMap.contains(c);
			assert(found);
		}
        assert(options->mY >= 0 && options->mY + options->mHeight <= 600);
		for (const auto& entry : {std::pair{options->mAutoCollectMoneyCheckbox.get(), "AutoCollectMoney"},
			std::pair{options->mShowHealthCheckbox.get(), "ShowHealth"}})
		{
			entry.first->SetChecked(true, true);
			bool saved = false;
			assert(RegistryReadBoolean(entry.second, &saved) && saved);
			entry.first->SetChecked(false, true);
			assert(RegistryReadBoolean(entry.second, &saved) && !saved);
		}
        Snapshot("mac-options.png");
        options->mAutoCollectSunCheckbox->SetChecked(!autoSun, true);
        assert(mAutoCollectSun != autoSun);
        for (int i = 0; i < 4; ++i)
        {
            const int previous = mGameSpeedHalfUnits;
            options->ButtonDepress(options->mGameSpeedButton->mId);
            assert(mGameSpeedHalfUnits == previous % 4 + 1);
        }
        assert(mGameSpeedHalfUnits == speed);
        int savedSpeed = 0;
        bool savedAuto = autoSun;
        assert(RegistryReadInteger("GameSpeedHalfUnits", &savedSpeed) && savedSpeed == speed);
        assert(RegistryReadBoolean("AutoCollectSun", &savedAuto) && savedAuto != autoSun);
        options->mAutoCollectSunCheckbox->SetChecked(autoSun, true);
        KillNewOptionsDialog();
        // Exercise the original Mac exit confirmation and graceful close request.
        mGameSelector->ButtonDepress(mGameSelector->mQuitButton->mId);
        assert(GetDialog(Dialogs::DIALOG_QUIT));
        LawnApp::ButtonDepress(Dialogs::DIALOG_QUIT + 2000);
        assert(mCloseRequest.load());
        std::puts("PASS real Mac menu: gameplay controls present alongside display settings, auto-sun change and four speed clicks saved, original quit confirmation requests graceful shutdown");
#endif
        mShutdown = true;
    }
};

int main(int argc, char** argv)
{
    bool isolated = false;
    for (int i = 1; i < argc; ++i)
        if (std::string_view(argv[i]).starts_with("-savedir=")) isolated = true;
    assert(isolated && "Pass an explicit disposable savedir, never a production save directory");
    PvzpStringListSetColors(gLawnStringFormats, gLawnStringFormatCount);
    gExtractResourcesByName = Sexy::ExtractResourcesByName;
    gLawnApp = new LevelSelectTestApp;
    gLawnApp->SetArgs(argc, argv);
    gLawnApp->Init();
    gLawnApp->Start();
    gLawnApp->Shutdown();
    delete gLawnApp;
}
