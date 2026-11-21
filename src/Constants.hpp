#pragma once

#include <map>

enum class LevelCategory {
    Daily,
    Weekly,
    Event,
    Gauntlet,
    Default
};

inline const std::map<LevelCategory, const char*> LEVEL_CATEGORY_TO_SPRITE = {
    {LevelCategory::Daily, "gj_dailyCrown_001.png"},
    {LevelCategory::Weekly, "gj_weeklyCrown_001.png"},
    {LevelCategory::Event, "gj_eventCrown_001.png"},
    {LevelCategory::Gauntlet, "theTowerDoor_001.png"},
    {LevelCategory::Default, "island_new31_001.png"}
};

inline constexpr auto RECALL_SPREADSHEET_URL =
    "https://docs.google.com/spreadsheets/d/"
    "1BBx9X8IYBtr7dA5cWu_smM2XBkLZzCXy7TjeEWRgag0/gviz/tq";

inline constexpr auto CHAOS_GAUNTLET_KEY_POSSESSED = "19";
inline constexpr auto DEMON_GAUNTLET_KEY_ITEM_ID = 5;