#include "NetUtils.hpp"
#include "Utils.hpp"
#include "../Constants.hpp"
#include <Geode/Geode.hpp>
#include <Geode/binding/GauntletNode.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/GameStatsManager.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GJMapPack.hpp>
#include <algorithm>

using namespace geode::prelude;

namespace Utils {
    GJGameLevel* getSavedGauntletLevel(int levelID) {
        auto manager = GameLevelManager::sharedState();
    #ifdef GEODE_IS_MACOS
        // macOS lacks this binding. The native cache uses decimal level IDs. //
        return static_cast<GJGameLevel*>(manager->m_gauntletLevels->objectForKey(std::to_string(levelID)));
    #else
        return manager->getSavedGauntletLevel(levelID);
    #endif
    }

    CCSprite* createRecallPlaySprite() {
        auto sprite = CCSprite::createWithSpriteFrameName("GJ_playBtn2_001.png");
        sprite->setScale(0.4f);
        return sprite;
    }

    CCRect getSpriteVisibleBounds(CCSprite* sprite) {
        auto const& quad = sprite->getQuad();
        return {quad.bl.vertices.x, quad.bl.vertices.y,
            quad.br.vertices.x - quad.bl.vertices.x,
            quad.tl.vertices.y - quad.bl.vertices.y};
    }

    std::vector<RecallOption> getAvailableRecallOptions(GJGameLevel* level) {
        std::vector<RecallOption> options;
        if (!level) return options;
        int levelID = level->m_levelID.value();
        int currentTimedID = level->m_dailyID.value();
        if (currentTimedID > 0 || level->m_gauntletLevel) {
            options.push_back({LevelCategory::Default});
        }
        auto const& recalls = NetUtils::getSafeRecallEntries();
        auto found = recalls.find(levelID);
        if (found != recalls.end()) {
            for (auto category : {LevelCategory::Daily, LevelCategory::Weekly, LevelCategory::Event}) {
                for (auto const& entry : found->second) {
                    if (entry.levelCategory == category && entry.timedID != currentTimedID) {
                        options.push_back({category, entry.timedID});
                    }
                }
            }
        }
        if (!level->m_gauntletLevel) {
            for (auto gauntlet : NetUtils::getGauntletsForLevel(levelID)) {
                options.push_back({LevelCategory::Gauntlet, 0, gauntlet});
            }
        }
        return options;
    }

    std::string getRecallSpriteName(LevelCategory category, std::optional<GauntletType> gauntlet) {
        if (category == LevelCategory::Gauntlet || category == LevelCategory::Default) {
            CCSpriteFrameCache::sharedSpriteFrameCache()->addSpriteFramesWithFile("GauntletSheet.plist");
        }
        return category == LevelCategory::Gauntlet
            ? GauntletNode::frameForType(gauntlet.value())
            : LEVEL_CATEGORY_TO_SPRITE.at(category);
    }

    bool isTimedRecallUnlocked(int timedID) {
        if (timedID <= 0) return false;

        // The current timed appearance is free, only older recalls require keys //
        auto manager = GameLevelManager::sharedState();
        int offset = timedID > 200000 ? 200000 : timedID > 100000 ? 100000 : 0;
        int currentID = offset == 200000 ? manager->m_currentEventID :
            offset == 100000 ? manager->m_currentWeeklyID : manager->m_currentDailyID;
        if (currentID > 0 && timedID == offset + currentID % 100000) return true;

        auto stats = GameStatsManager::sharedState();
        return stats->isDailyRecall(timedID) || stats->hasCompletedDailyStars(timedID);
    }

    bool unlockTimedRecall(int timedID) {
        if (timedID <= 0) return false;
        if (isTimedRecallUnlocked(timedID)) return true;
        auto stats = GameStatsManager::sharedState();
        int price = GameStatsManager::dailyRecallPrice(timedID);
        auto keyStat = std::to_string(static_cast<int>(StatKey::Keys));
        int keys = stats->getStat(keyStat.c_str());
        if (keys < price) return false;
        // Match the game's recall purchase; unlockDailyRecall has no Windows binding.
        stats->setStat(keyStat.c_str(), keys - price);
        stats->m_unlockedTimelyLevels->setObject(
            CCString::createWithFormat("%i", price), std::to_string(timedID));
        return true;
    }

    bool isGauntletRecallUnlocked(GJGameLevel* sourceLevel, GauntletType gauntlet) {
        if (!sourceLevel) return false;
        auto stats = GameStatsManager::sharedState();
        // Match GauntletSelectLayer's actual gates; isGauntletUnlocked is a stub //
        if (gauntlet == GauntletType::Chaos && !GameManager::sharedState()->getUGV(CHAOS_GAUNTLET_KEY_POSSESSED)) return false;
        if (gauntlet == GauntletType::Demon && !stats->isItemUnlocked(UnlockType::GJItem, DEMON_GAUNTLET_KEY_ITEM_ID)) return false;

        int levelID = sourceLevel->m_levelID.value();
        std::vector<int> levels;
        auto manager = GameLevelManager::sharedState();
        int gauntletID = static_cast<int>(gauntlet);
    #if defined(GEODE_IS_IOS) || defined(GEODE_IS_MACOS)
        // Mimic getSavedGauntlet which has no iOS or macOS binding //
        auto saved = static_cast<GJMapPack*>(manager->m_savedGauntlets->objectForKey(std::to_string(gauntletID)));
    #else
        auto saved = manager->getSavedGauntlet(gauntletID);
    #endif
        if (saved && saved->m_levels && saved->m_levels->count() > 0) {
            // Prefer the game's current order over the sheet, which can list former members //
            for (auto entry : CCArrayExt<CCString*>(saved->m_levels)) {
                levels.push_back(entry->intValue());
            }
        } else {
            levels = NetUtils::getLevelsForGauntlet(gauntlet);
        }
        for (int id : levels) {
            if (id == levelID) return true;
            if (!isRecallVariantCompleted(id, LevelCategory::Gauntlet)) return false;
        }
        return false;
    }

    bool isRecallVariantCompleted(int levelID, LevelCategory category, int timedID) {
        auto stats = GameStatsManager::sharedState();
        switch (category) {
            case LevelCategory::Daily:
            case LevelCategory::Weekly:
            case LevelCategory::Event:
                return timedID > 0 && stats->hasCompletedDailyStars(timedID);
    #if defined(GEODE_IS_IOS) || defined(GEODE_IS_MACOS)
            case LevelCategory::Gauntlet:
            case LevelCategory::Default: {
                auto manager = GameLevelManager::sharedState();
                bool gauntlet = category == LevelCategory::Gauntlet;
                auto level = gauntlet ? Utils::getSavedGauntletLevel(levelID) : manager->getSavedLevel(levelID);
                return level && level->m_dailyID.value() == 0 && level->m_gauntletLevel == gauntlet &&
                    stats->hasCompletedLevel(level);
            }
    #else
            // Bindings not available for iOS/macOS, keep the split and replace with this when available //
            case LevelCategory::Gauntlet:
                return stats->hasCompletedGauntletLevel(levelID);
            case LevelCategory::Default:
                return stats->hasCompletedOnlineLevel(levelID);
    #endif
        }
        return false;
    }

    bool hasUncompletedRecalls(GJGameLevel* level) {
        if (!level) return false;
        auto options = getAvailableRecallOptions(level);
        return std::any_of(options.begin(), options.end(), [level](auto const& option) {
            return !isRecallVariantCompleted(level->m_levelID.value(), option.category, option.timedID);
        });
    }

    int getRecallProgress(GJGameLevel* sourceLevel, LevelCategory category, int timedID) {
        bool isTimed = category == LevelCategory::Daily || category == LevelCategory::Weekly ||
            category == LevelCategory::Event;
        if (isTimed && timedID <= 0) return 0;
        if (!sourceLevel) return 0;
        if (isRecallVariantCompleted(sourceLevel->m_levelID.value(), category, timedID)) return 100;

        int levelID = sourceLevel->m_levelID.value();
        auto manager = GameLevelManager::sharedState();
        GJGameLevel* saved = isTimed ? manager->getSavedDailyLevel(timedID) :
            category == LevelCategory::Gauntlet ? Utils::getSavedGauntletLevel(levelID) :
            manager->getSavedLevel(levelID);
        auto matchesVariant = [&](GJGameLevel* level) {
            if (!level || level->m_levelID.value() != levelID) return false;
            if (isTimed) return level->m_dailyID.value() == timedID && !level->m_gauntletLevel;
            return level->m_dailyID.value() == 0 &&
                level->m_gauntletLevel == (category == LevelCategory::Gauntlet);
        };

        int progress = 0;
        if (matchesVariant(saved)) progress = saved->m_normalPercent.value();
        if (matchesVariant(sourceLevel)) progress = std::max(progress, sourceLevel->m_normalPercent.value());
        return std::clamp(progress, 0, 100);
    }
}
