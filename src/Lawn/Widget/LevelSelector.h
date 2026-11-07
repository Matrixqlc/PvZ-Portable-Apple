// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "widget/Widget.h"
#include "widget/ButtonListener.h"
#include <array>
#include <memory>

class LawnApp;
namespace Sexy { class ButtonWidget; class Image; }

// The chapter/level organization and DrawSeedType composition are adapted from
// lspepinho/PvZ-NeverGarden, LevelSelector.cpp (3543de26), LGPL-3.0-or-later.
// Layout, progress ownership and replay storage are specific to this port.
class LevelSelector : public Sexy::Widget, public Sexy::ButtonListener
{
public:
    explicit LevelSelector(LawnApp* theApp, int theSelectedLevel = 0);
    ~LevelSelector() override;
    void AddedToManager(Sexy::WidgetManager* theManager) override;
    void RemovedFromManager(Sexy::WidgetManager* theManager) override;
    void Draw(Sexy::Graphics* g) override;
    void Update() override;
    void KeyDown(Sexy::KeyCode theKey) override;
    void ButtonPress(int theId) override;
    void ButtonDepress(int theId) override;
    void ButtonDownTick(int) override {}
    void ButtonMouseEnter(int) override {}
    void ButtonMouseLeave(int) override {}
    void ButtonMouseMove(int, int, int) override {}
    void ShowStarResult(int theStars, int thePreviousBest, int theMowers, bool theReplay, bool theNewDefenseBadge);
    bool IsShowingStarResult() const { return mResultStars > 0; }

private:
    enum { Back = 100, Play, Rules, Chapter = 200, Level = 300 };
    LawnApp* mApp;
    int mSelectedChapter;
    int mSelectedLevel;
    std::array<int, 50> mStars{};
    std::array<bool, 50> mDefenseBadges{};
    std::array<int, 50> mArrivalFrom{};
    std::array<int, 50> mArrivalDelay{};
    int mAnimationTick = 0;
    int mPlayerId;
    int mResultStars = 0;
    int mPreviousBest = 0;
    int mResultMowers = 0;
    bool mResultReplay = false;
    bool mResultNewDefenseBadge = false;
    std::unique_ptr<Sexy::Image> mWoodSkin;
    std::unique_ptr<Sexy::Image> mPaperSkin;
    std::unique_ptr<Sexy::Image> mBackdrop;
    std::unique_ptr<Sexy::Image> mChapterIcons;
    std::array<std::unique_ptr<Sexy::ButtonWidget>, 18> mButtons;
    void SyncButtons();
    void SelectChapter(int theChapter);
    void PrepareStarArrival();
    void DrawStarResult(Sexy::Graphics* g);
    void DrawThumbnail(Sexy::Graphics* g, int theLevel, const Sexy::Rect& theRect);
};

std::unique_ptr<Sexy::ButtonWidget> MakeAdventureWoodButton(
    int theId, Sexy::ButtonListener* theListener, const std::string& theLabel);
std::unique_ptr<Sexy::ButtonWidget> MakeAdventureMenuButton(
    int theId, Sexy::ButtonListener* theListener, const std::string& theLabel);
