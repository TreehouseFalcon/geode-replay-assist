#include <Geode/Geode.hpp>
#include <Geode/binding/GJSearchObject.hpp>
#include <Geode/binding/LevelBrowserLayer.hpp>
#include <Geode/modify/GameLevelManager.hpp>
#include "../utils/NetUtils.hpp"

using namespace geode::prelude;

namespace {
    struct TimedSearch {
        SearchType type;
        GJTimedLevelType timedType;
        LevelCategory category;
        int offset;
        NetUtils::InitialRecallLoad load;
    };

    constexpr TimedSearch TIMED_SEARCHES[] = {
        {SearchType::DailyVault, GJTimedLevelType::Daily, LevelCategory::Daily,
            DAILY_TIMED_ID_OFFSET, NetUtils::InitialRecallLoad::DailyVault},
        {SearchType::WeeklyVault, GJTimedLevelType::Weekly, LevelCategory::Weekly,
            WEEKLY_TIMED_ID_OFFSET, NetUtils::InitialRecallLoad::WeeklyVault},
        {SearchType::EventVault, GJTimedLevelType::Event, LevelCategory::Event,
            EVENT_TIMED_ID_OFFSET, NetUtils::InitialRecallLoad::EventVault},
    };

    std::unordered_map<std::string, TimedSearch> s_pendingSearches;

    bool cacheTimedLevel(GJGameLevel* level, int timedID, TimedSearch const& search) {
        int levelID = level->m_levelID.value();
        if (levelID <= 0 || timedID <= search.offset || timedID > search.offset + TIMED_ID_RANGE_SIZE) return false;

        NetUtils::addTimedRecallEntry({level->m_levelName, levelID, search.category, timedID});
        return true;
    }

    size_t cacheTimedPage(GameLevelManager* manager, char const* key, TimedSearch search) {
        auto levels = manager->getStoredOnlineLevels(key);
        if (!levels) return 0;

        size_t count = 0;
        for (auto level : CCArrayExt<GJGameLevel*>(levels)) {
            // The Vault listing stores its timed ID in the recall field.
            if (cacheTimedLevel(level, level->m_recallDailyID, search)) ++count;
        }
        return count;
    }
}

void NetUtils::loadRecentTimedRecallEntries() {
    auto manager = GameLevelManager::sharedState();
    for (auto search : TIMED_SEARCHES) {
        // Creating a search object with just its type selects first page //
        auto query = GJSearchObject::create(search.type);
        std::string key = query->getKey();
        cacheTimedPage(manager, key.c_str(), search);
        // The Vault can omit a level already loaded in the game's timed page.
        int activeID = manager->getActiveDailyID(search.timedType);
        if (activeID > search.offset && activeID <= search.offset + TIMED_ID_RANGE_SIZE) {
            if (auto level = manager->getSavedDailyLevel(activeID); level && level->m_dailyID.value() == activeID) {
                cacheTimedLevel(level, activeID, search);
            }
        }
        if (s_pendingSearches.emplace(key, search).second) {
            manager->getOnlineLevels(query);
        }
    }
}

class $modify(RecallAssistTimedLevelManager, GameLevelManager) {
    void handleIt(bool success, std::string response, std::string tag, GJHttpType type) {
        if (type != GJHttpType::GetOnlineLevels) {
            GameLevelManager::handleIt(success, std::move(response), std::move(tag), type);
            return;
        }

        // If we're not searching then don't do anything //
        auto search = s_pendingSearches.extract(tag);
        if (search.empty()) {
            GameLevelManager::handleIt(success, std::move(response), tag, type);
            NetUtils::OnlineLevelSearchFinishedEvent(std::move(tag)).send();
            return;
        }

        /*
            Only a browser page should consume the result.
            When the player navigates to the safe page for the first time in a session, they shouldn't
            have to refresh to get the latest results.
        */
        auto delegate = m_levelManagerDelegate;
        auto browser = geode::cast::typeinfo_cast<LevelBrowserLayer*>(delegate);
        bool suppressDelegate = delegate && (!browser || !browser->m_searchObject ||
            tag != browser->m_searchObject->getKey());
        if (suppressDelegate) m_levelManagerDelegate = nullptr;
        GameLevelManager::handleIt(success, std::move(response), tag, type);
        if (suppressDelegate && !m_levelManagerDelegate) m_levelManagerDelegate = delegate;

        auto count = success ? cacheTimedPage(this, tag.c_str(), search.mapped()) : 0;
        log::info("indexed {} recalls from Vault search type {}", count, static_cast<int>(search.mapped().type));
        NetUtils::settleInitialRecallLoad(search.mapped().load);
    }
};
