#pragma once

#include "../Constants.hpp"
#include <optional>
#include <string>
#include <vector>

class GJGameLevel;
enum class GauntletType;

namespace cocos2d { class CCSprite; class CCRect; }

namespace Utils {
    struct RecallOption {
        LevelCategory category;
        int timedID = 0;
        std::optional<GauntletType> gauntlet;
    };

    std::vector<RecallOption> getAvailableRecallOptions(GJGameLevel* level);

    std::string getRecallSpriteName(GJGameLevel* sourceLevel, LevelCategory category, std::optional<GauntletType> gauntlet = std::nullopt);
    cocos2d::CCRect getSpriteVisibleBounds(cocos2d::CCSprite* sprite);
    cocos2d::CCSprite* createRecallPlaySprite();

    bool isRecallVariantCompleted(int levelID, LevelCategory category, int timedID = 0);
    bool hasUncompletedRecalls(GJGameLevel* level);
    enum class TimedRecallAccess {
        Unavailable,
        Playable,
        RequiresKeys,
        RequiresSkip
    };

    TimedRecallAccess getTimedRecallAccess(int timedID);
    bool unlockTimedRecall(int timedID);

    std::vector<GauntletType> getGauntletsForLevel(int levelID);
    bool isGauntletRecallUnlocked(GJGameLevel* sourceLevel, GauntletType gauntlet);
    int getRecallProgress(GJGameLevel* sourceLevel, LevelCategory category, int timedID = 0);
}
