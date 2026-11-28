#pragma once
#include "../Constants.hpp"
#include <Geode/loader/Event.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace NetUtils {
struct SafeRecallEntry {
    std::string levelName;
    int levelID;
    LevelCategory levelCategory;
    int timedID; // appearance number plus its category offset
};

// Populated asynchronously on mod load. Access from main thread. //
using SafeRecallCache = std::unordered_map<int, std::vector<SafeRecallEntry>>;

enum class InitialRecallLoad {
    Spreadsheet,
    Gauntlets,
    DailyVault,
    WeeklyVault,
    EventVault,
    Count, // this should always be last to size the bitset
};

// Sent on the game thread 10 seconds after each startup fetch has processed its result.
struct InitialRecallLoadSettledEvent : geode::Event<InitialRecallLoadSettledEvent, bool()> {};

struct OnlineLevelSearchFinishedEvent : geode::Event<OnlineLevelSearchFinishedEvent, bool(), std::string> {
    using Event::Event;
};

bool areInitialRecallLoadsSettled();
void settleInitialRecallLoad(InitialRecallLoad load);

void loadGauntletRecallEntries();
void loadRecentTimedRecallEntries();
void addTimedRecallEntry(SafeRecallEntry entry);
SafeRecallCache const& getSafeRecallEntries();
bool isSafeRecallDataLoaded();
std::optional<int> getTimedIDForLevel(int levelID, std::optional<LevelCategory> category = std::nullopt);

}
