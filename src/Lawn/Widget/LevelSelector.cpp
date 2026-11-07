// SPDX-License-Identifier: LGPL-3.0-or-later
// Chapter/card organization and plant composition adapted from lspepinho's
// PvZ-NeverGarden LevelSelector.cpp, commit 3543de26feefc0cd4868e895222684274ba8a489.
#include "LevelSelector.h"
#include "../../LawnApp.h"
#include "../../Resources.h"
#include "../AdventureProgress.h"
#include "../Plant.h"
#include "../System/PlayerInfo.h"
#include "../System/ReanimationLawn.h"
#include "../../PvzpLib/PvzpCommon.h"
#include "graphics/Graphics.h"
#include "graphics/Image.h"
#include "graphics/GLImage.h"
#include "graphics/ImageFont.h"
#include "widget/ButtonWidget.h"
#include "widget/Dialog.h"
#include "widget/WidgetManager.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <SDL_filesystem.h>

using namespace Sexy;

namespace
{
std::string Text(LawnApp* app, const char* chinese, const char* english)
{
    return app->GetString("LOCALE", "").starts_with("Chinese") ? chinese : english;
}

void CenterLabel(Graphics* g, const std::string& text, const Rect& rect, _Font* font, const Color& color)
{
    // The bitmap fonts' baseline includes padding. Use the existing font renderer's
    // glyph bounds rather than centering its line box or guessing a baseline.
    Rect bounds;
    bool haveBounds = false;
    if (auto* imageFont = dynamic_cast<ImageFont*>(font))
    {
        RectList glyphs;
        Graphics measure(*g);
        measure.SetClipRect(0, 0, 0, 0);
        imageFont->DrawStringEx(&measure, 0, 0, text, color, &glyphs, nullptr);
        for (const auto& glyph : glyphs)
        {
            if (glyph.mWidth <= 0 || glyph.mHeight <= 0) continue;
            bounds = haveBounds ? bounds.Union(glyph) : glyph;
            haveBounds = true;
        }
    }
    const int x = rect.mX + rect.mWidth / 2 - (haveBounds ? bounds.mX + bounds.mWidth / 2 : font->StringWidth(text) / 2);
    const int y = rect.mY + rect.mHeight / 2 - (haveBounds ? bounds.mY + bounds.mHeight / 2 : -font->GetAscent() / 2);
    PvzpDrawString(g, text, x, y, font, color, DS_ALIGN_LEFT);
}

std::unique_ptr<Image> LoadSkin(LawnApp* app, const char* filename)
{
    char* path = SDL_GetBasePath();
    const std::string base = path ? path : "";
    SDL_free(path);
    for (const auto& name : {base + "ui/" + filename, base + filename})
        if (auto* image = app->GetImage(name)) return std::unique_ptr<Image>(image);
    return nullptr;
}

void Panel(Graphics* g, Image* image, const Rect& r, bool paper = false)
{
    // The skins are original generated artwork. Crop only their transparent
    // canvas margins in the renderer; leave the source files untouched.
    const Rect src = paper ? Rect(13, 108, 2148, 498) : Rect(28, 36, 1393, 999);
    const int sx = paper ? 90 : 125;
    const int sy = paper ? 70 : 110;
    const int dx = std::min(paper ? 18 : 15, r.mWidth / 3);
    const int dy = std::min(paper ? 13 : 14, r.mHeight / 3);
    int xs[] = {src.mX, src.mX + sx, src.mX + src.mWidth - sx, src.mX + src.mWidth};
    int ys[] = {src.mY, src.mY + sy, src.mY + src.mHeight - sy, src.mY + src.mHeight};
    int xd[] = {r.mX, r.mX + dx, r.mX + r.mWidth - dx, r.mX + r.mWidth};
    int yd[] = {r.mY, r.mY + dy, r.mY + r.mHeight - dy, r.mY + r.mHeight};
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            g->DrawImage(image, Rect(xd[x], yd[y], xd[x+1]-xd[x], yd[y+1]-yd[y]),
                Rect(xs[x], ys[y], xs[x+1]-xs[x], ys[y+1]-ys[y]));
}

void Wood(Graphics* g, const Rect& r, bool selected = false, bool down = false, Image* skin = nullptr)
{
    if (skin)
    {
        g->SetColorizeImages(true);
        g->SetColor(down ? Color(215, 195, 160) : Color::White);
        Panel(g, skin, r);
        g->SetColorizeImages(false);
        if (selected)
        {
            g->SetColor(Color(255, 215, 103));
            g->DrawRect(r.mX + 3, r.mY + 3, r.mWidth - 7, r.mHeight - 7);
            g->SetColor(Color(255, 232, 160, 145));
            g->DrawRect(r.mX + 2, r.mY + 2, r.mWidth - 5, r.mHeight - 5);
        }
        return;
    }
    g->SetColor(Color(48, 29, 18));
    g->FillRect(r);
    g->SetColor(selected ? Color(187, 139, 61) : Color(125, 81, 43));
    g->FillRect(r.mX + 3, r.mY + 3, r.mWidth - 6, r.mHeight - 6);
    // Reuse the original selector's painted wood rather than adding a UI theme.
    if (IMAGE_REANIM_SELECTORSCREEN_WOODSIGN2_PRESS)
    {
        g->SetColorizeImages(true);
        g->SetColor(down ? Color(160, 145, 120) : Color(235, 208, 161));
        const auto* image = IMAGE_REANIM_SELECTORSCREEN_WOODSIGN2_PRESS;
        // The full sign contains baked-in localized text. Reuse its clean left
        // board and right cap, keeping the irregular painted edges without text.
        const int cap = std::min(18, r.mWidth / 5);
        g->DrawImage(IMAGE_REANIM_SELECTORSCREEN_WOODSIGN2_PRESS,
            Rect(r.mX + 3, r.mY + 3, cap, r.mHeight - 6), Rect(0, 8, 64, 46));
        g->DrawImage(IMAGE_REANIM_SELECTORSCREEN_WOODSIGN2_PRESS,
            Rect(r.mX + 3 + cap, r.mY + 3, r.mWidth - 6 - cap * 2, r.mHeight - 6), Rect(45, 8, 18, 46));
        g->DrawImage(IMAGE_REANIM_SELECTORSCREEN_WOODSIGN2_PRESS,
            Rect(r.mX + r.mWidth - 3 - cap, r.mY + 3, cap, r.mHeight - 6), Rect(image->mWidth - 61, 8, 61, 46));
        g->SetColorizeImages(false);
    }
    g->SetColor(Color(68, 42, 24));
    g->DrawLine(r.mX + 4, r.mY + r.mHeight - 5, r.mX + r.mWidth - 5, r.mY + r.mHeight - 5);
    g->SetColor(Color(194, 145, 85));
    g->DrawLine(r.mX + 5, r.mY + 4, r.mX + r.mWidth - 6, r.mY + 4);
    if (selected)
    {
        g->SetColor(Color(255, 211, 87));
        g->DrawRect(r.mX + 1, r.mY + 1, r.mWidth - 3, r.mHeight - 3);
        g->DrawRect(r.mX + 2, r.mY + 2, r.mWidth - 5, r.mHeight - 5);
    }
}

class WoodButton : public ButtonWidget
{
public:
    WoodButton(int id, ButtonListener* listener, const std::string& label)
        : ButtonWidget(id, listener) { mLabel = label; mDoFinger = true; }
    void Draw(Graphics* g) override
    {
        Wood(g, Rect(0, 0, mWidth, mHeight), mIsOver && !mDisabled, mIsDown, mButtonImage);
        CenterLabel(g, mLabel, Rect(mIsDown ? 1 : 0, mIsDown ? 1 : 0, mWidth, mHeight), FONT_BRIANNETOD16,
            mDisabled ? Color(150, 137, 110) : Color(255, 240, 194));
    }
};

// The main menu uses hanging, irregular signs rather than framed card buttons.
// Compose only clean portions of its existing sign, leaving the corners alpha.
class MenuSignButton : public ButtonWidget
{
public:
    MenuSignButton(int id, ButtonListener* listener, const std::string& label)
        : ButtonWidget(id, listener)
    {
        mLabel = label;
        mDoFinger = true;
        mHasAlpha = true;
        mHasTransparencies = true;
    }
    void Draw(Graphics* g) override
    {
        Image* sign = IMAGE_REANIM_SELECTORSCREEN_WOODSIGN2_PRESS;
        if (!sign) return;
        const bool down = mIsDown && mIsOver;
        const int y = down ? 2 : 0;
        // Two short chains join the new sign to the existing left-hand stack.
        for (int x : {35, mWidth - 34})
        {
            g->SetColor(Color(41, 38, 30));
            g->FillRect(x, 0, 4, 19 + y);
            g->SetColor(Color(146, 139, 110));
            g->DrawLine(x + 1, 1, x + 1, 17 + y);
            g->DrawLine(x - 1, 6, x + 4, 8);
            g->DrawLine(x - 1, 12, x + 4, 14);
        }
        g->SetColorizeImages(true);
        g->SetColor(down ? Color(210, 200, 170) : mIsOver ? Color(255, 245, 210) : Color::White);
        g->DrawImage(sign, Rect(0, 12 + y, 49, 48), Rect(0, 7, 65, 47));
        g->DrawImage(sign, Rect(49, 12 + y, mWidth - 98, 48), Rect(45, 7, 18, 47));
        g->DrawImage(sign, Rect(mWidth - 49, 12 + y, 49, 48), Rect(sign->mWidth - 65, 7, 65, 47));
        g->SetColorizeImages(false);
        CenterLabel(g, mLabel, Rect(12, 17 + y, mWidth - 24, 36), FONT_BRIANNETOD16,
            mIsOver ? Color(255, 245, 200) : Color(245, 245, 235));
    }
};

void Star(Graphics* g, int x, int y, int radius, bool earned)
{
    Point outer[10], inner[10], shine[10];
    for (int i = 0; i < 10; ++i)
    {
        const double a = i * 3.141592653589793 / 5.0 - 3.141592653589793 / 2.0;
        const double r = (i % 2 ? 0.46 : 1.0) * radius;
        outer[i] = Point(x + static_cast<int>(std::round(std::cos(a) * r)),
                         y + static_cast<int>(std::round(std::sin(a) * r)));
        inner[i] = Point(x + static_cast<int>(std::round(std::cos(a) * r * 0.76)),
                         y + static_cast<int>(std::round(std::sin(a) * r * 0.76)));
        shine[i] = Point(x - 1 + static_cast<int>(std::round(std::cos(a) * r * 0.57)),
                         y - 1 + static_cast<int>(std::round(std::sin(a) * r * 0.57)));
    }
    g->SetColor(Color(57, 35, 22)); g->PolyFillAA(outer, 10);
    g->SetColor(earned ? Color(246, 174, 34) : Color(94, 71, 48));
    g->PolyFillAA(inner, 10);
    g->SetColor(earned ? Color(255, 224, 83) : Color(111, 88, 61));
    g->PolyFillAA(shine, 10);
}

void ArrivingStar(Graphics* g, int x, int y, int radius, int age)
{
    if (age < 0) return;
    const float scale = age < 14 ? PvzpAnimateCurveFloat(0, 14, age, 0.35f, 1.2f, CURVE_EASE_OUT) :
        PvzpAnimateCurveFloat(14, 30, age, 1.2f, 1.0f, CURVE_EASE_IN_OUT);
    const int lift = PvzpAnimateCurve(0, 24, age, radius, 0, CURVE_EASE_OUT);
    Star(g, x, y - lift, std::max(1, static_cast<int>(radius * scale)), true);
    if (age < 42)
    {
        const int alpha = PvzpAnimateCurve(8, 42, age, 230, 0, CURVE_LINEAR);
        const float spread = PvzpAnimateCurveFloat(0, 42, age, 1.2f, 1.85f, CURVE_EASE_OUT);
        g->SetColor(Color(255, 236, 137, alpha));
        for (int n = 0; n < 6; ++n)
        {
            const float a = n * 3.141592653589793f / 3.0f;
            const float dx = std::cos(a), dy = std::sin(a);
            g->DrawLineAA(x + dx * radius * spread, y + dy * radius * spread,
                x + dx * (radius * spread + 4), y + dy * (radius * spread + 4));
        }
    }
}

// The grey seal reserves the extra challenge location. The gold seal records post-clear defense.
void Medal(Graphics* g, int x, int y, bool defenseBadge = false)
{
    Point ribbons[] = {{x - 7, y + 6}, {x + 7, y + 6}, {x + 9, y + 22},
                       {x, y + 17}, {x - 9, y + 22}};
    g->SetColor(defenseBadge ? Color(123, 53, 30) : Color(65, 62, 56)); g->PolyFill(ribbons, 5);
    Point ring[20], disk[20];
    for (int i = 0; i < 20; ++i)
    {
        double a = i * 3.141592653589793 / 10;
        ring[i] = Point(x + static_cast<int>(std::cos(a) * 14), y + static_cast<int>(std::sin(a) * 14));
        disk[i] = Point(x + static_cast<int>(std::cos(a) * 11), y + static_cast<int>(std::sin(a) * 11));
    }
    g->SetColor(defenseBadge ? Color(93, 53, 19) : Color(47, 46, 43)); g->PolyFillAA(ring, 20, true);
    g->SetColor(defenseBadge ? Color(246, 191, 65) : Color(167, 165, 148)); g->PolyFillAA(disk, 20, true);
    g->SetColor(defenseBadge ? Color(255, 239, 155) : Color(222, 219, 198)); g->DrawLineAA(x - 7, y - 8, x + 2, y - 10);
    Point leaf[] = {{x - 5, y + 5}, {x - 6, y}, {x - 1, y - 5}, {x + 6, y - 7}, {x + 5, y + 2}, {x, y + 6}};
    g->SetColor(defenseBadge ? Color(58, 101, 43) : Color(92, 98, 83)); g->PolyFillAA(leaf, 6, true);
    g->SetColor(defenseBadge ? Color(224, 234, 135) : Color(196, 197, 175)); g->DrawLine(x - 4, y + 5, x + 4, y - 5);
}

void Disk(Graphics* g, int x, int y, int radius, const Color& color)
{
    Point points[24];
    for (int i = 0; i < 24; ++i)
    {
        const double a = i * 3.141592653589793 / 12;
        points[i] = Point(x + static_cast<int>(std::round(std::cos(a) * radius)),
            y + static_cast<int>(std::round(std::sin(a) * radius)));
    }
    g->SetColor(color); g->PolyFillAA(points, 24, true);
}

void ChapterIcon(Graphics* g, int chapter, int x, int y, bool selected, Image* icons)
{
    if (icons)
    {
        // Hand-painted objects: preserve each silhouette's aspect ratio rather
        // than turning the roof and pool into generic geometric symbols.
        static const Rect sources[] = {
            {30, 184, 380, 358}, {478, 206, 320, 329}, {842, 253, 423, 267},
            {1298, 248, 420, 257}, {1744, 200, 416, 324}
        };
        const auto& source = sources[chapter];
        const float scale = std::min(39.0f / source.mWidth, 32.0f / source.mHeight);
        const int width = static_cast<int>(std::round(source.mWidth * scale));
        const int height = static_cast<int>(std::round(source.mHeight * scale));
        g->DrawImage(icons, Rect(x - width / 2, y - height / 2, width, height), source);
        return;
    }
    if (chapter == 0)
    {
        g->SetColor(Color(244, 185, 44));
        for (int n = 0; n < 8; ++n)
        {
            double a = n * 3.141592653589793 / 4;
            g->DrawLineAA(x + static_cast<int>(std::cos(a)*11), y + static_cast<int>(std::sin(a)*11),
                x + static_cast<int>(std::cos(a)*15), y + static_cast<int>(std::sin(a)*15));
        }
        Disk(g, x, y, 10, Color(93, 65, 29));
        Disk(g, x, y, 8, Color(255, 216, 68));
    }
    else if (chapter == 1)
    {
        Disk(g, x, y, 11, Color(185, 184, 181));
        Disk(g, x+5, y-4, 10, selected ? Color(239, 216, 161) : Color(117, 72, 35));
    }
    else if (chapter == 2)
    {
        g->SetColor(Color(143, 203, 211));
        for (int row = -1; row <= 1; ++row)
            for (int col = -2; col < 2; ++col)
                g->DrawLineAA(x+col*6, y+row*6+(col%2 ? 1 : -1), x+(col+1)*6, y+row*6+(col%2 ? -1 : 1));
    }
    else if (chapter == 3)
    {
        for (int n = -1; n <= 1; ++n) Disk(g, x+n*7, y-std::abs(n)*3, 7, Color(185, 188, 180));
        g->SetColor(Color(153, 163, 155)); g->DrawLineAA(x-12,y+9,x+12,y+9);
    }
    else
    {
        Point roof[] = {{x-13,y+9},{x-8,y-10},{x+8,y-10},{x+13,y+9}};
        g->SetColor(Color(172, 137, 119)); g->PolyFillAA(roof,4,true);
        g->SetColor(Color(84, 59, 45));
        g->DrawLine(x,y-10,x,y+9); g->DrawLine(x-10,y-1,x+10,y-1);
        g->DrawLine(x-13,y+9,x+13,y+9);
    }
}

struct Cover { SeedType plant; ZombieType zombie; int minigame = -1; };
// Each cover describes the level in play, never its end-of-level reward.
constexpr Cover Covers[50] = {
    {SEED_PEASHOOTER, ZOMBIE_NORMAL}, {SEED_SUNFLOWER, ZOMBIE_NORMAL},
    {SEED_CHERRYBOMB, ZOMBIE_TRAFFIC_CONE}, {SEED_WALLNUT, ZOMBIE_NORMAL},
    {SEED_WALLNUT, ZOMBIE_NORMAL, 6}, {SEED_POTATOMINE, ZOMBIE_POLEVAULTER},
    {SEED_SNOWPEA, ZOMBIE_POLEVAULTER}, {SEED_CHOMPER, ZOMBIE_PAIL},
    {SEED_REPEATER, ZOMBIE_PAIL}, {SEED_WALLNUT, ZOMBIE_PAIL},
    {SEED_PUFFSHROOM, ZOMBIE_NORMAL}, {SEED_SUNSHROOM, ZOMBIE_TRAFFIC_CONE},
    {SEED_FUMESHROOM, ZOMBIE_DOOR}, {SEED_GRAVEBUSTER, ZOMBIE_DOOR},
    {SEED_GRAVEBUSTER, ZOMBIE_NORMAL, 16}, {SEED_HYPNOSHROOM, ZOMBIE_FOOTBALL},
    {SEED_SCAREDYSHROOM, ZOMBIE_FOOTBALL}, {SEED_ICESHROOM, ZOMBIE_DANCER},
    {SEED_DOOMSHROOM, ZOMBIE_DANCER}, {SEED_FUMESHROOM, ZOMBIE_FOOTBALL},
    {SEED_LILYPAD, ZOMBIE_DUCKY_TUBE}, {SEED_SQUASH, ZOMBIE_DUCKY_TUBE},
    {SEED_THREEPEATER, ZOMBIE_SNORKEL}, {SEED_TANGLEKELP, ZOMBIE_SNORKEL},
    {SEED_PEASHOOTER, ZOMBIE_NORMAL, 12}, {SEED_JALAPENO, ZOMBIE_ZAMBONI},
    {SEED_SPIKEWEED, ZOMBIE_ZAMBONI}, {SEED_TORCHWOOD, ZOMBIE_DOLPHIN_RIDER},
    {SEED_TALLNUT, ZOMBIE_DOLPHIN_RIDER}, {SEED_TALLNUT, ZOMBIE_ZAMBONI},
    {SEED_SEASHROOM, ZOMBIE_JACK_IN_THE_BOX}, {SEED_PLANTERN, ZOMBIE_JACK_IN_THE_BOX},
    {SEED_CACTUS, ZOMBIE_BALLOON}, {SEED_BLOVER, ZOMBIE_BALLOON},
    {SEED_PEASHOOTER, ZOMBIE_NORMAL, -2}, {SEED_SPLITPEA, ZOMBIE_DIGGER},
    {SEED_STARFRUIT, ZOMBIE_DIGGER}, {SEED_PUMPKINSHELL, ZOMBIE_POGO},
    {SEED_MAGNETSHROOM, ZOMBIE_POGO}, {SEED_CACTUS, ZOMBIE_BALLOON},
    {SEED_CABBAGEPULT, ZOMBIE_BUNGEE}, {SEED_FLOWERPOT, ZOMBIE_BUNGEE},
    {SEED_KERNELPULT, ZOMBIE_LADDER}, {SEED_INSTANT_COFFEE, ZOMBIE_LADDER},
    {SEED_CHOMPER, ZOMBIE_BUNGEE}, {SEED_GARLIC, ZOMBIE_CATAPULT},
    {SEED_UMBRELLA, ZOMBIE_CATAPULT}, {SEED_MARIGOLD, ZOMBIE_GARGANTUAR},
    {SEED_MELONPULT, ZOMBIE_GARGANTUAR}, {SEED_NONE, ZOMBIE_BOSS, 19}
};

Rect CardRect(int index) { return Rect(23 + (index % 5) * 153, 121 + (index / 5) * 169, 142, 154); }
const char* BackgroundGroup(int chapter)
{
    constexpr const char* groups[] = {"DelayLoad_Background1", "DelayLoad_Background2", "DelayLoad_Background3",
        "DelayLoad_Background4", "DelayLoad_Background5"};
    return groups[chapter];
}
}

std::unique_ptr<ButtonWidget> MakeAdventureWoodButton(int id, ButtonListener* listener, const std::string& label)
{
    return std::make_unique<WoodButton>(id, listener, label);
}

std::unique_ptr<ButtonWidget> MakeAdventureMenuButton(int id, ButtonListener* listener, const std::string& label)
{
    return std::make_unique<MenuSignButton>(id, listener, label);
}

LevelSelector::LevelSelector(LawnApp* app, int selected) : mApp(app)
{
    mWidth = BOARD_WIDTH; mHeight = BOARD_HEIGHT;
    mPlayerId = app->mPlayerInfo->mId;
    mArrivalDelay.fill(-1);
    mSelectedLevel = std::clamp(selected > 0 ? selected : app->mPlayerInfo->mLevel, 1, 50);
    mSelectedChapter = (mSelectedLevel - 1) / 10;
    mLoadedResourceNames.push_back("DelayLoad_ChallengeScreen");
    mLoadedResourceNames.push_back(BackgroundGroup(0));
    if (mSelectedChapter != 0) mLoadedResourceNames.push_back(BackgroundGroup(mSelectedChapter));
    for (const auto& name : mLoadedResourceNames) PvzpLoadResources(name.c_str());
    mWoodSkin = LoadSkin(app, "level-select-wood.png");
    mPaperSkin = LoadSkin(app, "level-select-paper.png");
    mBackdrop = LoadSkin(app, "level-select-garden.png");
    mChapterIcons = LoadSkin(app, "level-select-chapters.png");
    for (int i = 0; i < 50; ++i)
    {
        mStars[i] = app->GetAdventureStars(i + 1);
        mDefenseBadges[i] = app->HasAdventureDefenseBadge(i + 1);
    }
    mButtons[0] = MakeAdventureWoodButton(Back, this, Text(app, "返回", "Back"));
    mButtons[0]->Resize(20, 560, 105, 34);
    mButtons[1] = MakeAdventureWoodButton(Play, this, "");
    mButtons[1]->Resize(598, 486, 178, 54);
    mButtons[2] = MakeAdventureWoodButton(Rules, this, Text(app, "星级规则", "Star rules"));
    mButtons[2]->Resize(648, 560, 132, 34);
    for (int i = 0; i < 3; ++i) mButtons[i]->mButtonImage = mWoodSkin.get();
    for (int i = 0; i < 5; ++i)
    {
        mButtons[3 + i] = std::make_unique<ButtonWidget>(Chapter + i, this);
        mButtons[3 + i]->Resize(23 + i * 153, 70, 142, 42);
        mButtons[3 + i]->mBtnNoDraw = true;
        mButtons[3 + i]->mDoFinger = true;
    }
    for (int i = 0; i < 10; ++i)
    {
        mButtons[8 + i] = std::make_unique<ButtonWidget>(Level + i, this);
        const auto r = CardRect(i);
        mButtons[8 + i]->Resize(r.mX, r.mY, r.mWidth, r.mHeight);
        mButtons[8 + i]->mBtnNoDraw = true;
        mButtons[8 + i]->mDoFinger = true;
    }
    SyncButtons();
    PrepareStarArrival();
}

LevelSelector::~LevelSelector() = default;

void LevelSelector::AddedToManager(WidgetManager* manager)
{
    Widget::AddedToManager(manager);
    for (const auto& button : mButtons) AddWidget(button.get());
}

void LevelSelector::RemovedFromManager(WidgetManager* manager)
{
    for (const auto& button : mButtons) RemoveWidget(button.get());
    Widget::RemovedFromManager(manager);
}

void LevelSelector::SyncButtons()
{
    const int unlocked = AdventureProgress::UnlockedLevel(mApp->mPlayerInfo->mLevel, mApp->mPlayerInfo->mFinishedAdventure);
    mButtons[1]->SetDisabled(mSelectedLevel > unlocked);
    mButtons[1]->mLabel = mSelectedLevel > unlocked ? Text(mApp, "尚未解锁", "Locked") :
        Text(mApp, mSelectedLevel == mApp->mPlayerInfo->mLevel ? "继续冒险" : "开始重玩",
            mSelectedLevel == mApp->mPlayerInfo->mLevel ? "Continue" : "Replay");
    MarkDirty();
}

void LevelSelector::SelectChapter(int chapter)
{
    mSelectedChapter = std::clamp(chapter, 0, 4);
    const std::string group = BackgroundGroup(mSelectedChapter);
    if (std::find(mLoadedResourceNames.begin(), mLoadedResourceNames.end(), group) == mLoadedResourceNames.end())
    {
        PvzpLoadResources(group.c_str());
        mLoadedResourceNames.push_back(group);
    }
    const int current = mApp->mPlayerInfo->mLevel;
    mSelectedLevel = (current - 1) / 10 == mSelectedChapter ? current : mSelectedChapter * 10 + 1;
    SyncButtons();
    PrepareStarArrival();
}

void LevelSelector::PrepareStarArrival()
{
    mAnimationTick = 0;
    mArrivalDelay.fill(-1);
    int delay = 20;
    for (int i = mSelectedChapter * 10; i < mSelectedChapter * 10 + 10; ++i)
    {
        const int from = mApp->GetPendingAdventureStars(i + 1);
        if (from >= 0 && from < mStars[i])
        {
            mArrivalFrom[i] = from;
            mArrivalDelay[i] = delay;
            delay += (mStars[i] - from) * 22 + 12;
        }
    }
}

void LevelSelector::ShowStarResult(int stars, int previous, int mowers, bool replay, bool newDefenseBadge)
{
    mResultStars = std::clamp(stars, 1, 3);
    mPreviousBest = std::clamp(previous, 0, 3);
    mResultMowers = std::max(mowers, 0);
    mResultReplay = replay;
    mResultNewDefenseBadge = newDefenseBadge;
    mAnimationTick = 0;
    for (auto& button : mButtons)
    {
        button->SetVisible(false);
        button->mMouseVisible = false;
    }
    mButtons[1]->SetVisible(true);
    mButtons[1]->mMouseVisible = true;
    mButtons[1]->SetDisabled(false);
    mButtons[1]->Resize(275, 477, 250, 55);
    mButtons[1]->mLabel = Text(mApp, replay ? "返回选关" : "继续", replay ? "Level selection" : "Continue");
    MarkDirty();
}

void LevelSelector::DrawStarResult(Graphics* g)
{
    g->SetColor(Color(15, 26, 12, 90)); g->FillRect(0, 0, BOARD_WIDTH, BOARD_HEIGHT);
    Wood(g, Rect(215, 64, 370, 66), false, false, mWoodSkin.get());
    CenterLabel(g, Text(mApp, "关卡完成", "Level complete"), Rect(230, 75, 340, 43),
        FONT_DWARVENTODCRAFT24, Color(255, 235, 175));
    if (mPaperSkin) Panel(g, mPaperSkin.get(), Rect(135, 146, 530, 302), true);
    else { g->SetColor(Color(235, 214, 163)); g->FillRect(135, 146, 530, 302); }
    CenterLabel(g, std::format("{} {}-{}", Text(mApp, "关卡", "Level"), (mSelectedLevel - 1) / 10 + 1,
        (mSelectedLevel - 1) % 10 + 1), Rect(200, 166, 400, 32), FONT_BRIANNETOD16, Color(74, 45, 24));
    for (int n = 0; n < 3; ++n)
    {
        const int x = 285 + n * 115;
        Star(g, x, 267, 48, false);
        if (n < mResultStars) ArrivingStar(g, x, 267, 48, mAnimationTick - 20 - n * 28);
    }
    CenterLabel(g, std::format("{}  {} / 3", Text(mApp, "本次星级", "This run"), mResultStars),
        Rect(200, mResultNewDefenseBadge ? 320 : 326, 400, 28), FONT_BRIANNETOD16, Color(74, 45, 24));
    const auto defense = mResultMowers == 0 ? Text(mApp, "未触发救场车", "No rescue mower used") :
        std::format("{} {}", Text(mApp, "触发救场车：", "Rescue mowers used:"), mResultMowers);
    CenterLabel(g, defense, Rect(180, mResultNewDefenseBadge ? 348 : 362, 440, 25), FONT_BRIANNETOD12, Color(105, 83, 53));
    if (mResultNewDefenseBadge)
    {
        const auto badge = Text(mApp, "获得再战防守标记", "Replay defense badge earned");
        const int x = (BOARD_WIDTH - FONT_BRIANNETOD12->StringWidth(badge) - 34) / 2;
        Medal(g, x + 14, 387, true);
        PvzpDrawString(g, badge, x + 34, 392, FONT_BRIANNETOD12, Color(103, 66, 25), DS_ALIGN_LEFT);
    }
    const auto best = std::format("{}{}  {} / 3", mResultStars > mPreviousBest ? Text(mApp, "新记录！", "New best! ") : "",
        Text(mApp, "最高星级", "Best"), std::max(mPreviousBest, mResultStars));
    CenterLabel(g, best, Rect(180, mResultNewDefenseBadge ? 414 : 402, 440, 25), FONT_BRIANNETOD16, Color(93, 95, 39));
}

void LevelSelector::DrawThumbnail(Graphics* g, int level, const Rect& r)
{
    Graphics clip(*g);
    clip.SetClipRect(r);
    Image* backgrounds[] = {IMAGE_BACKGROUND1, IMAGE_BACKGROUND2, IMAGE_BACKGROUND3, IMAGE_BACKGROUND4, IMAGE_BACKGROUND5};
    const int chapter = (level - 1) / 10;
    if (backgrounds[chapter])
        clip.DrawImage(backgrounds[chapter], r, Rect(220, 0, 500, 450));
    if ((chapter == 2 || chapter == 3) && IMAGE_POOL && IMAGE_POOL_NIGHT)
    {
        Image* water = chapter == 3 ? IMAGE_POOL_NIGHT : IMAGE_POOL;
        clip.DrawImage(water, Rect(r.mX - 186 * r.mWidth / 500, r.mY + 278 * r.mHeight / 450,
            water->mWidth * r.mWidth / 500, water->mHeight * r.mHeight / 450),
            Rect(0, 0, water->mWidth, water->mHeight));
    }
    const auto& cover = Covers[level - 1];
    if (cover.minigame >= 0)
    {
        clip.DrawImageCel(IMAGE_CHALLENGE_THUMBNAILS, r, cover.minigame);
        return;
    }
    if (cover.minigame == -2)
    {
        clip.DrawImage(IMAGE_SCARY_POT, r.mX + 14, r.mY + 15, 47, 65);
        clip.DrawImage(IMAGE_SCARY_POT, r.mX + 72, r.mY + 20, 47, 65);
        return;
    }
    const float scale = r.mWidth / 140.0f;
    Graphics plants(clip);
    plants.mScaleX = plants.mScaleY = 0.67f * scale;
    Plant::DrawSeedType(&plants, cover.plant, SEED_NONE, VARIATION_NORMAL, r.mX + 10, r.mY + 30 * scale);
    Graphics zombies(clip);
    zombies.mScaleX = zombies.mScaleY = (cover.zombie == ZOMBIE_GARGANTUAR || cover.zombie == ZOMBIE_ZAMBONI ||
        cover.zombie == ZOMBIE_CATAPULT ? 0.38f : 0.56f) * scale;
    mApp->mReanimatorCache->DrawCachedZombie(&zombies, r.mX + 66 * scale, r.mY + 9 * scale, cover.zombie);
}

void LevelSelector::Draw(Graphics* g)
{
    g->SetLinearBlend(true);
    g->DrawImage(mBackdrop ? mBackdrop.get() : IMAGE_BACKGROUND1 ? IMAGE_BACKGROUND1 : IMAGE_CHALLENGE_BACKGROUND,
        0, 0, BOARD_WIDTH, BOARD_HEIGHT);
    g->SetColor(Color(24, 42, 17, 35)); g->FillRect(0, 0, BOARD_WIDTH, BOARD_HEIGHT);
    if (IsShowingStarResult())
    {
        DrawStarResult(g);
        return;
    }
    Wood(g, Rect(231, 10, 338, 49), false, false, mWoodSkin.get());
    CenterLabel(g, Text(mApp, "冒险选关", "Adventure levels"), Rect(245, 14, 310, 39),
        FONT_DWARVENTODCRAFT24, Color(255, 235, 175));
    Wood(g, Rect(632, 17, 147, 36), false, false, mWoodSkin.get());
    Star(g, 652, 35, 13, true);
    int total = 0;
    for (int i = mSelectedChapter * 10; i < mSelectedChapter * 10 + 10; ++i)
    {
        total += mStars[i];
        if (mArrivalDelay[i] >= 0)
            for (int n = mArrivalFrom[i]; n < mStars[i]; ++n)
                if (mAnimationTick < mArrivalDelay[i] + (n - mArrivalFrom[i]) * 22) --total;
    }
    CenterLabel(g, std::format("{} / 30", total), Rect(673, 20, 98, 29), FONT_BRIANNETOD16, Color::White);
    const char* cn[] = {"白天", "夜晚", "泳池", "浓雾", "屋顶"};
    const char* en[] = {"Day", "Night", "Pool", "Fog", "Roof"};
    for (int i = 0; i < 5; ++i)
    {
        const auto& b = mButtons[3 + i];
        Wood(g, Rect(b->mX, b->mY, b->mWidth, b->mHeight), false, b->mIsDown, mWoodSkin.get());
        if (i == mSelectedChapter && mPaperSkin)
            Panel(g, mPaperSkin.get(), Rect(b->mX + 5, b->mY + 4, b->mWidth - 10, b->mHeight - 8), true);
        ChapterIcon(g, i, b->mX + 29, b->mY + 21, i == mSelectedChapter, mChapterIcons.get());
        CenterLabel(g, Text(mApp, cn[i], en[i]), Rect(b->mX + 49, b->mY + 5, 78, 32), FONT_BRIANNETOD16,
            i == mSelectedChapter ? Color(54, 37, 23) : Color(221, 204, 168));
    }
    const auto* player = mApp->mPlayerInfo;
    const int unlocked = AdventureProgress::UnlockedLevel(player->mLevel, player->mFinishedAdventure);
    Wood(g, Rect(14, 115, 772, 337), false, false, mWoodSkin.get());
    for (int i = 0; i < 10; ++i)
    {
        const int level = mSelectedChapter * 10 + i + 1;
        const auto r = CardRect(i);
        Wood(g, r, level == mSelectedLevel || mButtons[8 + i]->mIsOver, false, mWoodSkin.get());
        DrawThumbnail(g, level, Rect(r.mX + 10, r.mY + 10, 122, 100));
        if (level > unlocked)
        {
            g->SetColor(Color(0, 0, 0, 155)); g->FillRect(r.mX + 10, r.mY + 10, 122, 100);
            g->DrawImage(IMAGE_LOCK, r.mX + 50, r.mY + 36, 42, 51);
        }
        else
        {
            if (mDefenseBadges[level - 1]) Medal(g, r.mX + 90, r.mY + 19, true);
            Medal(g, r.mX + 122, r.mY + 19);
        }
        g->SetColor(Color(110, 69, 36, 190));
        g->FillRect(r.mX + 9, r.mY + 115, 124, 31);
        g->SetColor(level == mSelectedLevel ? Color(247, 210, 112) : Color(210, 171, 110));
        g->FillRect(r.mX + 9, r.mY + 118, 44, 27);
        g->SetColor(Color(80, 49, 24)); g->DrawRect(r.mX + 9, r.mY + 118, 44, 27);
        CenterLabel(g, std::format("{}-{}", mSelectedChapter + 1, i + 1), Rect(r.mX + 10, r.mY + 119, 42, 25),
            FONT_BRIANNETOD16, Color(47, 29, 16));
        for (int n = 0; n < 3; ++n)
        {
            const int x = r.mX + 70 + n * 25, y = r.mY + 134;
            const int delay = mArrivalDelay[level - 1];
            const bool arriving = delay >= 0 && n >= mArrivalFrom[level - 1] && n < mStars[level - 1];
            Star(g, x, y, 13, !arriving && n < mStars[level - 1]);
            if (arriving) ArrivingStar(g, x, y, 13, mAnimationTick - delay - (n - mArrivalFrom[level - 1]) * 22);
        }
    }
    // Shallow paper detail strip: rules stay visible without opening a popup.
    if (mPaperSkin) Panel(g, mPaperSkin.get(), Rect(20, 463, 760, 88), true);
    else { g->SetColor(Color(235, 214, 163)); g->FillRect(27, 470, 746, 74); }
    g->SetColor(Color(83, 54, 28)); g->FillRect(31, 474, 88, 66);
    DrawThumbnail(g, mSelectedLevel, Rect(34, 477, 82, 60));
    const int stars = mStars[mSelectedLevel - 1];
    const bool knownComplete = player->mFinishedAdventure > 0 || mSelectedLevel < player->mLevel;
    const auto state = mSelectedLevel > unlocked ? Text(mApp, "尚未解锁", "Locked") :
        stars > 0 ? std::format("{} / 3", stars) : knownComplete ? Text(mApp, "已通关 · 无星星记录", "Cleared / unrated") : Text(mApp, "等待挑战", "Ready to play");
    PvzpDrawString(g, std::format("{} {}-{}", Text(mApp, "关卡", "Level"), mSelectedChapter + 1,
        (mSelectedLevel - 1) % 10 + 1), 129, 496, FONT_BRIANNETOD16, Color(58, 35, 19), DS_ALIGN_LEFT);
    PvzpDrawString(g, state, 284, 496, FONT_BRIANNETOD12, Color(105, 91, 66), DS_ALIGN_LEFT);
    Medal(g, 141, 519);
    PvzpDrawString(g, Text(mApp, "额外挑战：尚未开放", "Extra challenge: coming later"), 161, 526,
        FONT_BRIANNETOD12, Color(94, 83, 62), DS_ALIGN_LEFT);
    if (mDefenseBadges[mSelectedLevel - 1])
    {
        Medal(g, 372, 519, true);
        PvzpDrawString(g, Text(mApp, "再战防守", "Replay defense"), 393, 526,
            FONT_BRIANNETOD12, Color(103, 66, 25), DS_ALIGN_LEFT);
    }
    PvzpDrawString(g, Text(mApp, "三星：通关且未触发救场车", "3 stars: clear without using a rescue mower"), 388, 581,
        FONT_BRIANNETOD12, Color(255, 241, 196), DS_ALIGN_CENTER);
}

void LevelSelector::Update()
{
    Widget::Update();
    if (!mApp->mPlayerInfo || mApp->mPlayerInfo->mId != mPlayerId) return;
    if (mApp->mActive && !mApp->mMinimized && mApp->GetDialogCount() == 0)
    {
        ++mAnimationTick;
        if (IsShowingStarResult())
        {
            for (int n = 0; n < mResultStars; ++n)
                if (mAnimationTick == 20 + n * 28) mApp->PlaySample(SOUND_CHIME);
        }
        else for (int i = mSelectedChapter * 10; i < mSelectedChapter * 10 + 10; ++i)
        {
            if (mArrivalDelay[i] < 0) continue;
            for (int n = mArrivalFrom[i]; n < mStars[i]; ++n)
                if (mAnimationTick == mArrivalDelay[i] + (n - mArrivalFrom[i]) * 22) mApp->PlaySample(SOUND_POINTS);
            if (mAnimationTick >= mArrivalDelay[i] + (mStars[i] - mArrivalFrom[i] - 1) * 22 + 42)
            {
                mApp->ClearPendingAdventureStars(i + 1, mStars[i]);
                mArrivalDelay[i] = -1;
            }
        }
    }
    // Button hover/press state is painted in the parent along with the cards.
    MarkDirty();
}

void LevelSelector::ButtonPress(int) { mApp->PlaySample(SOUND_BUTTONCLICK); }

void LevelSelector::ButtonDepress(int id)
{
    if (IsShowingStarResult())
    {
        if (id == Play || id == Back) mApp->ContinueAdventureWin(mSelectedLevel, mResultReplay);
        return;
    }
    if (id == Back)
    {
        mApp->KillLevelSelector(); mApp->ShowGameSelector();
    }
    else if (id == Play) mApp->StartAdventureLevel(mSelectedLevel);
    else if (id == Rules)
    {
        const auto title = Text(mApp, "星级与挑战", "Stars and challenges");
        const auto body = Text(mApp,
            "三星：通关，未触发救场车。\n二星：通关，触发一次救场车。\n一星：通关，触发两次或更多。\n\n植物受伤不扣星。只保留最高星级。\n冒险全通后再次挑战，拿到三星可点亮金色标记。\n以往的通关记录不会自动获得标记。\n额外挑战将在规则验证后开放。",
            "3 stars: clear with no rescue mower used.\n2 stars: clear with one used.\n1 star: clear with two or more used.\n\nPlant damage does not reduce stars. Best scores are kept.\nAfter finishing Adventure, clear again with 3 stars to earn the gold badge.\nPast clears do not count toward the badge.\nExtra challenges are coming later.");
        mApp->LawnMessageBox(DIALOG_MESSAGE, title.c_str(), body.c_str(), "[DIALOG_BUTTON_OK]", "", Dialog::BUTTONS_FOOTER);
    }
    else if (id >= Chapter && id < Chapter + 5) SelectChapter(id - Chapter);
    else if (id >= Level && id < Level + 10)
    {
        mSelectedLevel = mSelectedChapter * 10 + id - Level + 1;
        SyncButtons();
    }
}

void LevelSelector::KeyDown(KeyCode key)
{
    if (IsShowingStarResult())
    {
        if (key == KEYCODE_RETURN || key == KEYCODE_SPACE || key == KEYCODE_ESCAPE) ButtonDepress(Play);
        return;
    }
    if (key == KEYCODE_ESCAPE) ButtonDepress(Back);
    else if (key == KEYCODE_RETURN || key == KEYCODE_SPACE) ButtonDepress(Play);
    else if (key == KEYCODE_LEFT || key == KEYCODE_RIGHT || key == KEYCODE_UP || key == KEYCODE_DOWN)
    {
        const int delta = key == KEYCODE_LEFT ? -1 : key == KEYCODE_RIGHT ? 1 : key == KEYCODE_UP ? -5 : 5;
        const int target = std::clamp(mSelectedLevel + delta, 1, 50);
        if ((target - 1) / 10 != mSelectedChapter) SelectChapter((target - 1) / 10);
        mSelectedLevel = target; SyncButtons();
    }
}
