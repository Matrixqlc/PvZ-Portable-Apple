// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <algorithm>

// Adventure has five chapters of ten levels, as in NeverGarden's LevelSelector.
// Keep the selected replay level separate from PlayerInfo::mLevel (main progress).
namespace AdventureProgress
{
constexpr int LevelCount = 50;
constexpr int ChapterSize = 10;

constexpr int UnlockedLevel(int theMainLevel, int theFinishedAdventure)
{
    return theFinishedAdventure > 0 ? LevelCount : std::clamp(theMainLevel, 1, LevelCount);
}

constexpr bool CanSelect(int theLevel, int theMainLevel, int theFinishedAdventure)
{
    return theLevel >= 1 && theLevel <= UnlockedLevel(theMainLevel, theFinishedAdventure);
}

constexpr int StarsForDefense(int theTriggeredMowers)
{
    return theTriggeredMowers <= 0 ? 3 : theTriggeredMowers == 1 ? 2 : 1;
}
}
