#pragma once
#include "../Constants.hpp"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace NetUtils {
struct SafeRecallEntry {
    std::string levelName;
    int levelID;
    LevelCategory levelCategory;
    /*
        Daily = N,
        Weekly = 100000 + N,
        Event = 200000 + N
    */
    int timedID;
};

// Populated asynchronously on mod load. Access from main thread. //
using SafeRecallCache = std::unordered_map<int, std::vector<SafeRecallEntry>>;

void loadGauntletRecallEntries();
SafeRecallCache const& getSafeRecallEntries();
bool isSafeRecallDataLoaded();
std::optional<int> getTimedIDForLevel(int levelID, std::optional<LevelCategory> category = std::nullopt);

}
