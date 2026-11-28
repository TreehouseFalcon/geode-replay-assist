#include "utils/NetUtils.hpp"
#include "AvailableRecallsPopup.hpp"
#include "utils/Utils.hpp"
#include <Geode/ui/Label.hpp>
#include <Geode/ui/ProgressBar.hpp>
#include <Geode/ui/LoadingSpinner.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/GameStatsManager.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/GJSearchObject.hpp>
#include <Geode/binding/LevelInfoLayer.hpp>
#include <Geode/binding/CCSpriteGrayscale.hpp>
#include <optional>
#include <algorithm>
#include <string_view>

namespace {
    void showTimedSkipPrompt(LevelCategory category) {
        std::string_view categoryName =
            category == LevelCategory::Daily ? "<cc>Daily</c>"
            : category == LevelCategory::Weekly ? "<cl>Weekly</c>"
            : category == LevelCategory::Event ? "<cd>Event</c>" : "";
        if (!categoryName.empty()) {
            geode::createQuickPopup("level locked",
                fmt::format("Open the {} page and skip the level, or claim its reward first", categoryName),
                "OK");
        }
    }
}

CCNode* AvailableRecallsPopup::createAvailableRecallContainer(LevelCategory levelCategory, std::optional<GauntletType> gauntlet, int timedID) {
    // Gauntlet containers require a specific gauntlet, other categories don't //
    if (levelCategory == LevelCategory::Gauntlet && !gauntlet) return nullptr;

    bool isTimed = levelCategory == LevelCategory::Daily || levelCategory == LevelCategory::Weekly || levelCategory == LevelCategory::Event;
    if (isTimed && timedID <= 0) return nullptr;

    const char* categoryName = "browse";
    switch (levelCategory) {
        case LevelCategory::Daily: categoryName = "daily"; break;
        case LevelCategory::Weekly: categoryName = "weekly"; break;
        case LevelCategory::Event: categoryName = "event"; break;
        case LevelCategory::Gauntlet: categoryName = "gauntlet"; break;
        case LevelCategory::Default: break;
    }
    auto label = geode::Label::create(categoryName, "bigFont.fnt");
    label->setScale(0.5f);
    if (label->getContentSize().width * label->getScale() > 80.f) {
        label->setScale(80.f / label->getContentSize().width);
    }

    auto spriteName = Utils::getRecallSpriteName(m_level, levelCategory, gauntlet);
    bool undiscoveredGauntlet = levelCategory == LevelCategory::Gauntlet &&
        !Utils::isGauntletRecallUnlocked(m_level, *gauntlet);
    bool completed = Utils::isRecallVariantCompleted(m_level->m_levelID.value(), levelCategory, timedID);
    bool dimArtwork = completed || undiscoveredGauntlet;
    CCSprite* sprite = dimArtwork
        ? CCSpriteGrayscale::createWithSpriteFrameName(spriteName)
        : CCSprite::createWithSpriteFrameName(spriteName.c_str());
    if (dimArtwork) {
        sprite->setColor({128, 128, 128});
    }
    sprite->setScale(0.75);

    auto playSprite = Utils::createRecallPlaySprite();
    // Reserve the same space for both play and lock icons //
    auto buttonSize = playSprite->getContentSize() * playSprite->getScale();
    bool locked = (isTimed && Utils::getTimedRecallAccess(timedID) != Utils::TimedRecallAccess::Playable) ||
        (levelCategory == LevelCategory::Gauntlet && !Utils::isGauntletRecallUnlocked(m_level, *gauntlet));
    if (locked) {
        playSprite = CCSprite::createWithSpriteFrameName("GJLargeLock_001.png");
        playSprite->setScale(buttonSize.height / playSprite->getContentSize().height);
    }

    auto playButton = CCMenuItemSpriteExtra::create(
        playSprite, this, menu_selector(AvailableRecallsPopup::onPlayRecall));
    playButton->setTag(levelCategory == LevelCategory::Gauntlet ? static_cast<int>(*gauntlet) : timedID);
    playButton->setUserObject(CCInteger::create(static_cast<int>(levelCategory)));
    auto playMenu = CCMenu::create();
    playMenu->ignoreAnchorPointForPosition(false);
    playMenu->setContentSize(buttonSize);
    playButton->setPosition({buttonSize.width / 2.f, buttonSize.height / 2.f});
    playMenu->addChild(playButton);

    constexpr float containerWidth = 90.f;
    auto bounds = Utils::getSpriteVisibleBounds(sprite);
    float spriteSlotHeight = bounds.size.height * sprite->getScaleY();
    constexpr float gap = 2.f;
    auto spriteSlot = CCNode::create();
    spriteSlot->setAnchorPoint({0.5f, 0.5f});
    spriteSlot->setContentSize({containerWidth, spriteSlotHeight});
    sprite->setAnchorPoint({0.f, 0.f});
    sprite->setPosition({
        containerWidth / 2.f - bounds.getMidX() * sprite->getScaleX(),
        -bounds.getMinY() * sprite->getScaleY()
    });
    spriteSlot->addChild(sprite);
    if (undiscoveredGauntlet) {
        CCSpriteFrameCache::sharedSpriteFrameCache()->addSpriteFramesWithFile("GJ_GameSheet03.plist");
        auto lockOverlay = CCSpriteGrayscale::createWithSpriteFrameName("GJLargeLock_001.png");
        lockOverlay->setColor({128, 128, 128});
        lockOverlay->setScale(0.4f);
        lockOverlay->setPosition({containerWidth / 2.f, spriteSlotHeight / 2.f + 12});
        spriteSlot->addChild(lockOverlay, 1);
    } else if (completed) {
        CCSpriteFrameCache::sharedSpriteFrameCache()->addSpriteFramesWithFile("GJ_GameSheet03.plist");
        auto completedIcon = CCSprite::createWithSpriteFrameName("GJ_completesIcon_001.png");
        completedIcon->setPosition({containerWidth / 2.f, spriteSlotHeight / 2.f});
        spriteSlot->addChild(completedIcon, 1);
    }

    auto labelSize = label->getContentSize() * label->getScale();
    constexpr float buttonOffsetFromTop = 90.f;
    constexpr float progressBarWidth = containerWidth - 3;
    constexpr float progressBarHeight = 6.f;
    constexpr float progressBarGap = 5.f;
    constexpr float bottomPadding = 3.f;
    float containerHeight = buttonOffsetFromTop + buttonSize.height / 2.f +
        progressBarGap + progressBarHeight + bottomPadding;
    auto container = CCNode::create();
    container->setContentSize({containerWidth, containerHeight});

    // Keep the artwork in a vertical layout, independently of the button position //
    auto artwork = CCNode::create();
    artwork->setContentSize({containerWidth, labelSize.height + spriteSlotHeight + gap});
    artwork->setAnchorPoint({0.5f, 1.f});
    artwork->setPosition({containerWidth / 2.f, containerHeight});
    artwork->addChild(label);
    artwork->addChild(spriteSlot);
    artwork->setLayout(ColumnLayout::create()
        ->setAxisReverse(true)
        ->setAxisAlignment(AxisAlignment::Center)
        ->setCrossAxisAlignment(AxisAlignment::Center)
        ->setCrossAxisLineAlignment(AxisAlignment::Center)
        ->setCrossAxisOverflow(false)
        ->setAutoScale(false)
        ->setGap(gap));
    container->addChild(artwork);

    playMenu->setPosition({containerWidth / 2.f, containerHeight - buttonOffsetFromTop});
    container->addChild(playMenu);

    int progress = Utils::getRecallProgress(m_level, levelCategory, timedID);
    auto progressBar = geode::ProgressBar::create(static_cast<float>(progress), geode::ProgressBar::Style::Solid);
    progressBar->setAnchorPoint({0.5f, 0.5f});
    progressBar->setScaleX(progressBarWidth / progressBar->getContentSize().width);
    progressBar->setScaleY(progressBarHeight / progressBar->getContentSize().height);
    progressBar->setFillColor({0, 255, 0});
    progressBar->setPosition({containerWidth / 2.f, bottomPadding + progressBarHeight / 2.f});
    container->addChild(progressBar);

    auto progressLabel = geode::Label::create(fmt::format("{}%", progress), "bigFont.fnt");
    progressLabel->setScale(0.2f);
    progressLabel->setPosition(progressBar->getPosition());
    container->addChild(progressLabel, 1);
    return container;
}

bool AvailableRecallsPopup::init(GJGameLevel* level) {
    if (!level) {
        return false;
    }
    if (!Popup::init(320.f, 170.f)) {
        return false;
    }
    m_level = level;

    setTitle("replays available");

    auto body = CCNode::create();
    body->setID("available-recalls-popup"_spr);
    body->setAnchorPoint({0.5f, 0.5f});
    body->setContentSize({m_size.width - 40.f, m_size.height - 60.f});
    body->setPosition({m_size.width / 2.f, m_size.height / 2.f - 15.f});
    m_mainLayer->addChild(body);

    std::vector<Utils::RecallOption> availableRecallOptions = Utils::getAvailableRecallOptions(level);
    for (auto const& option : availableRecallOptions) {
        body->addChild(createAvailableRecallContainer(option.category, option.gauntlet, option.timedID));
    }

    body->setLayout(RowLayout::create()
        ->setAxisAlignment(AxisAlignment::Center)
        ->setCrossAxisAlignment(AxisAlignment::End)
        ->setCrossAxisLineAlignment(AxisAlignment::End)
        ->setGap(12.f));

    return true;
}

AvailableRecallsPopup* AvailableRecallsPopup::create(GJGameLevel* level) {
    auto popup = new AvailableRecallsPopup();
    if (popup->init(level)) {
        popup->autorelease();
        return popup;
    }

    delete popup;
    return nullptr;
}

void AvailableRecallsPopup::onPlayRecall(CCObject* sender) {
    if (m_pendingTimedID != 0 || m_pendingGauntlet || getEventListener("online-recall-search"_spr) ||
        (m_unlockPopup && m_unlockPopup->getParent())) return;

    auto button = static_cast<CCMenuItemSpriteExtra*>(sender);
    auto category = static_cast<LevelCategory>(static_cast<CCInteger*>(button->getUserObject())->getValue());
    if (category == LevelCategory::Default) {
        loadOnlineRecall(button);
        return;
    }

    bool isGauntlet = category == LevelCategory::Gauntlet;
    int timedID = isGauntlet ? 0 : button->getTag();
    auto gauntlet = isGauntlet ? std::optional(static_cast<GauntletType>(button->getTag())) : std::nullopt;
    int levelID = m_level->m_levelID.value();

    if (isGauntlet) {
        auto gauntlets = Utils::getGauntletsForLevel(levelID);
        if (std::find(gauntlets.begin(), gauntlets.end(), *gauntlet) == gauntlets.end()) {
            geode::createQuickPopup("Recall unavailable", "This level was <cy>not found</c> in the selected gauntlet", "OK");
            return;
        }
        if (!Utils::isGauntletRecallUnlocked(m_level, *gauntlet)) {
            geode::createQuickPopup("level locked", "You have not <cy>discovered</c> this <co>gauntlet level</c> yet", "OK");
            return;
        }
    } else {
        auto const& recalls = NetUtils::getSafeRecallEntries();
        auto found = recalls.find(levelID);
        if (found == recalls.end() || std::none_of(found->second.begin(), found->second.end(),
            [timedID, category](auto const& entry) { return entry.timedID == timedID && entry.levelCategory == category; })) {
            geode::createQuickPopup("Recall unavailable", "This timed appearance was not found in <cl>The Safe</c> cache for this level", "OK");
            return;
        }
        auto access = Utils::getTimedRecallAccess(timedID);
        if (access == Utils::TimedRecallAccess::RequiresSkip) {
            showTimedSkipPrompt(category);
            return;
        }
        if (access == Utils::TimedRecallAccess::Unavailable) return;
        if (access == Utils::TimedRecallAccess::RequiresKeys) {
            int price = GameStatsManager::dailyRecallPrice(timedID);
            m_unlockPopup = geode::createQuickPopup("Unlock Recall",
                fmt::format("Unlock this recall for {} demon key{}?", price, price == 1 ? "" : "s"),
                "Cancel", "Unlock", [timedID, category, button = Ref(button)](auto, bool confirmed) {
                    if (!confirmed) return;
                    if (!Utils::unlockTimedRecall(timedID)) {
                        if (Utils::getTimedRecallAccess(timedID) == Utils::TimedRecallAccess::RequiresSkip) {
                            showTimedSkipPrompt(category);
                            return;
                        }
                        geode::createQuickPopup("Not enough keys", "You need more demon keys to unlock this recall.", "OK");
                        return;
                    }
                    button->setSprite(Utils::createRecallPlaySprite());
                });
            return;
        }
    }

    auto manager = GameLevelManager::sharedState();
    auto savedLevel = isGauntlet ?
        manager->getSavedGauntletLevel(levelID) :
        manager->getSavedDailyLevel(timedID);
    auto matches = [&](GJGameLevel* level) {
        return level && level->m_levelID.value() == levelID &&
            level->m_dailyID.value() == timedID && level->m_gauntletLevel == isGauntlet;
    };
    for (auto level : {savedLevel, m_level.data()}) {
        if (matches(level)) {
            openRecall(level, gauntlet);
            return;
        }
    }

    m_pendingTimedID = timedID;
    m_pendingGauntlet = isGauntlet;
    m_pendingGauntletType = gauntlet;
    m_previousDownloadDelegate = manager->m_levelDownloadDelegate;
    manager->m_levelDownloadDelegate = this;
    showDownloadIndicator(button);
    manager->downloadLevel(levelID, isGauntlet, timedID);
}

void AvailableRecallsPopup::loadOnlineRecall(CCMenuItemSpriteExtra* button) {
    auto manager = GameLevelManager::sharedState();
    int levelID = m_level->m_levelID.value();
    auto query = GJSearchObject::create(SearchType::Search, std::to_string(levelID));
    std::string key = query->getKey();

    auto findLevel = [manager, levelID, key](bool searchFinished) -> GJGameLevel* {
        auto hasCreator = [manager](GJGameLevel* level) {
            int userID = level->m_userID.value();
            if (level->m_accountID.value() <= 0 && manager->accountIDForUserID(userID) <= 0) return false;
            if (level->m_creatorName.empty()) level->m_creatorName = manager->userNameForUserID(userID);
            return !level->m_creatorName.empty();
        };
        auto saved = manager->getSavedLevel(levelID);
        if (saved && hasCreator(saved)) return saved;
        if (auto levels = manager->getStoredOnlineLevels(key.c_str())) {
            for (auto level : CCArrayExt<GJGameLevel*>(levels)) {
                if (level->m_levelID.value() == levelID && (searchFinished || hasCreator(level))) return level;
            }
        }
        return searchFinished ? saved : nullptr;
    };

    if (auto level = findLevel(false)) return openRecall(level);
    showDownloadIndicator(button);
    this->addEventListener("online-recall-search"_spr, NetUtils::OnlineLevelSearchFinishedEvent(key), [this, findLevel] {
        removeEventListener("online-recall-search"_spr);
        clearDownloadIndicator();
        if (auto level = findLevel(true)) return openRecall(level);
        geode::createQuickPopup("Recall unavailable", "Could not load the online level. Please try again.", "OK");
    });
    manager->getOnlineLevels(query);
}

void AvailableRecallsPopup::clearUnlockPrompt() {
    auto popup = m_unlockPopup;
    m_unlockPopup = nullptr;
    if (popup) popup->removeFromParentAndCleanup(true);
}

void AvailableRecallsPopup::showDownloadIndicator(CCMenuItemSpriteExtra* button) {
    if (!button || !button->getParent()) return;
    auto spinner = geode::LoadingSpinner::create(button->getContentSize().height);
    if (!spinner) return;

    m_downloadButton = button;
    m_downloadSpinner = spinner;
    spinner->setPosition(button->getPosition());
    button->getParent()->addChild(spinner, button->getZOrder() + 1);
    button->setVisible(false);
    button->setEnabled(false);
}

void AvailableRecallsPopup::clearDownloadIndicator() {
    if (m_downloadSpinner) m_downloadSpinner->removeFromParent();
    if (m_downloadButton) {
        m_downloadButton->setVisible(true);
        m_downloadButton->setEnabled(true);
    }
    m_downloadSpinner = nullptr;
    m_downloadButton = nullptr;
}

void AvailableRecallsPopup::restoreDownloadDelegate() {
    clearDownloadIndicator();
    auto manager = GameLevelManager::sharedState();
    if (manager->m_levelDownloadDelegate == this) {
        manager->m_levelDownloadDelegate = m_previousDownloadDelegate;
    }
    m_previousDownloadDelegate = nullptr;
    m_pendingTimedID = 0;
    m_pendingGauntlet = false;
    m_pendingGauntletType.reset();
}

AvailableRecallsPopup::~AvailableRecallsPopup() {
    clearUnlockPrompt();
    restoreDownloadDelegate();
}

void AvailableRecallsPopup::onClose(CCObject* sender) {
    removeEventListener("online-recall-search"_spr);
    clearUnlockPrompt();
    restoreDownloadDelegate();
    Popup::onClose(sender);
}

void AvailableRecallsPopup::openRecall(GJGameLevel* level, std::optional<GauntletType> gauntlet) {
    auto game = GameManager::sharedState();
    int previousGauntlet = game->m_currentGauntlet;

    // Find the timed ID location to use for caching the level the player is sent to, only if there's not already one cached //
    int timedID = level->m_dailyID.value();
    int* activeTimedID = nullptr;
    if (!level->m_gauntletLevel) {
        auto manager = GameLevelManager::sharedState();
        activeTimedID =
            timedID > EVENT_TIMED_ID_OFFSET ? &manager->m_activeEventID
            : timedID > WEEKLY_TIMED_ID_OFFSET ? &manager->m_activeWeeklyID
            : &manager->m_activeDailyID;
        if (*activeTimedID > 0) activeTimedID = nullptr;
    }
    int previousTimedID = activeTimedID ? *activeTimedID : 0;
    if (activeTimedID) *activeTimedID = timedID;

    // LevelInfoLayer::onBack rebuilds GauntletLayer from this value //
    if (level->m_gauntletLevel && gauntlet) {
        game->m_currentGauntlet = static_cast<int>(*gauntlet);
    }

    auto scene = LevelInfoLayer::scene(level, false);
    if (!scene) {
        // Revert if scene fails for some reason //
        game->m_currentGauntlet = previousGauntlet;
        if (activeTimedID) *activeTimedID = previousTimedID;
        return;
    }

    if (auto layer = scene->getChildByType<LevelInfoLayer>(0)) {
        markRecallPage(layer);
        if (level->m_gauntletLevel && gauntlet) {
            markGauntletRecallReturn(layer, previousGauntlet);
        }
    }

    onClose(nullptr);
    CCDirector::sharedDirector()->pushScene(CCTransitionFade::create(0.5f, scene));
}

void AvailableRecallsPopup::levelDownloadFinished(GJGameLevel* level) {
    bool hasPendingDownload = m_pendingGauntlet || m_pendingTimedID > 0;
    bool matches = level && hasPendingDownload &&
        level->m_levelID.value() == m_level->m_levelID.value() &&
        level->m_gauntletLevel == m_pendingGauntlet &&
        level->m_dailyID.value() == m_pendingTimedID;
    auto gauntlet = m_pendingGauntletType;
    restoreDownloadDelegate();
    if (matches) {
        openRecall(level, gauntlet);
    } else {
        geode::createQuickPopup("Recall unavailable", "Could not load the requested recall variant", "OK");
    }
}

void AvailableRecallsPopup::levelDownloadFailed(int) {
    restoreDownloadDelegate();
    geode::createQuickPopup("Download failed", "Unable to download the recall variant. Please try again.", "OK");
}
