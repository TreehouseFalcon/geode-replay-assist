#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/LevelLeaderboard.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/ui/LoadingSpinner.hpp>
#include "../AvailableRecallsPopup.hpp"
#include "../utils/NetUtils.hpp"
#include "../utils/Utils.hpp"
#include <algorithm>

using namespace geode::prelude;

class $modify(RecallAssistLevelInfoLayer, LevelInfoLayer) {
    struct Fields {
        bool m_openedFromRecall = false;
        bool m_returnToPreviousLevel = false;
        bool m_leavingRecall = false;
        int m_previousGauntlet = 0;
        CCSprite* m_recallBadge = nullptr;
        CCSprite* m_recallBackground = nullptr;
        CCSprite* m_recallIcon = nullptr;
        std::string m_recallIconName;
        ListenerHandle m_indicatorSettingListener;
    };

    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) {
            return false;
        }

        bool loading = !NetUtils::areInitialRecallLoadsSettled();
        if (!loading && Utils::getAvailableRecallOptions(level).empty()) return true;
        auto menu = this->getChildByID("death-tracker-menu");
        if (!menu) {
            log::warn("could not find mount point for available recalls button. Is geode.node-ids enabled?");
            return true;
        }

        auto buttonSprite = CCSprite::createWithSpriteFrameName("GJ_plainBtn_001.png");
        buttonSprite->setScale(0.5f);
        auto button = CCMenuItemSpriteExtra::create(
            buttonSprite, this, menu_selector(RecallAssistLevelInfoLayer::onRecallAssist)
        );
        button->setID("recall-assist-button"_spr);
        menu->addChild(button);

        if (loading) {
            button->setEnabled(false);
            buttonSprite->setVisible(false);
            auto spinner = geode::LoadingSpinner::create(button->getContentSize().height);
            if (spinner) {
                spinner->setID("recall-assist-loading"_spr);
                button->addChildAtPosition(spinner, Anchor::Center);
            }

            this->addEventListener(NetUtils::InitialRecallLoadSettledEvent(),
                [this, button = WeakRef(button), spinner = WeakRef(spinner)] {
                    if (!NetUtils::areInitialRecallLoadsSettled()) return;

                    auto item = button.lock();
                    if (!item || !item->getParent()) return;
                    if (Utils::getAvailableRecallOptions(m_level).empty()) {
                        auto menu = item->getParent();
                        item->removeFromParent();
                        menu->updateLayout();
                        return;
                    }

                    if (auto loadingSpinner = spinner.lock();
                        loadingSpinner && loadingSpinner->getParent() == item.data()) {
                        loadingSpinner->removeFromParent();
                    }
                    item->setEnabled(true);
                    auto background = static_cast<CCSprite*>(item->getNormalImage());
                    background->setVisible(true);
                    initializeRecallButton(background);
                });
        } else {
            initializeRecallButton(buttonSprite);
        }
        menu->updateLayout();
        return true;
    }

    void initializeRecallButton(CCSprite* buttonSprite) {
        m_fields->m_recallBackground = buttonSprite;
        updateRecallEntryIcon();
        updateRecallIndicator();
        m_fields->m_indicatorSettingListener = SettingChangedEventV3(
            Mod::get(), "show-recall-indicator"
        ).listen([this](std::shared_ptr<SettingV3>) { updateRecallIndicator(); });
    }

    void onEnterTransitionDidFinish() {
        LevelInfoLayer::onEnterTransitionDidFinish();
        updateRecallEntryIcon();
        updateRecallIndicator();
    }

    void updateRecallIndicator() {
        auto background = m_fields->m_recallBackground;
        if (!background) return;

        auto indicatorMode = Mod::get()->getSettingValue<std::string>("show-recall-indicator");
        bool show = indicatorMode == "always" || (indicatorMode == "uncleared" && Utils::hasUncompletedRecalls(m_level));
        if (show && !m_fields->m_recallBadge) {
            auto size = background->getContentSize();

            auto exclamation = CCSprite::createWithSpriteFrameName("exMark_001.png");
            exclamation->setScale(0.7f);
            exclamation->setPosition({size.width - 5, size.height - 5});
            background->addChild(exclamation, 2);

            m_fields->m_recallBadge = exclamation;
        }

        if (m_fields->m_recallBadge) m_fields->m_recallBadge->setVisible(show);
    }

    void updateRecallEntryIcon() {
        auto background = m_fields->m_recallBackground;
        if (!background) return;

        auto options = Utils::getAvailableRecallOptions(m_level);
        if (options.empty()) return;

        auto featuredReplayOption = std::find_if(options.begin(), options.end(), [this](auto const& option) {
            return !Utils::isRecallVariantCompleted(m_level->m_levelID.value(), option.category, option.timedID);
        });
        auto const& entryIcon = featuredReplayOption != options.end() ? *featuredReplayOption : options.front();
        auto iconName = Utils::getRecallSpriteName(entryIcon.category, entryIcon.gauntlet);
        if (iconName == m_fields->m_recallIconName) return;

        // Fit and center the visible artwork, excluding trimmed frame margins //
        auto icon = CCSprite::createWithSpriteFrameName(iconName.c_str());
        auto bounds = Utils::getSpriteVisibleBounds(icon);
        auto size = background->getContentSize();
        float iconScale = std::min({
            size.width * 0.7f / bounds.size.width,
            size.height * 0.7f / bounds.size.height,
            1.f
        });
        icon->setScale(iconScale);
        icon->setAnchorPoint({0.f, 0.f});
        icon->setPosition({
            size.width / 2.f - bounds.getMidX() * iconScale,
            size.height / 2.f - bounds.getMidY() * iconScale
        });
        if (m_fields->m_recallIcon) m_fields->m_recallIcon->removeFromParent();
        background->addChild(icon, 1);

        m_fields->m_recallIcon = icon;
        m_fields->m_recallIconName = std::move(iconName);
    }

    void onRecallAssist(CCObject*) {
        if (auto popup = AvailableRecallsPopup::create(m_level)) {
            popup->show();
        }
    }

    void onBack(CCObject* sender) {
        if (!m_fields->m_returnToPreviousLevel) {
            LevelInfoLayer::onBack(sender);
            return;
        }

        if (m_isBusy || m_fields->m_leavingRecall) return;

        m_fields->m_leavingRecall = true;
        this->setKeypadEnabled(false);
        GameManager::sharedState()->m_currentGauntlet = m_fields->m_previousGauntlet;
        CCDirector::sharedDirector()->popScene();
    }
};

void markGauntletRecallReturn(LevelInfoLayer* layer, int previousGauntlet) {
    auto recallLayer = static_cast<RecallAssistLevelInfoLayer*>(layer);
    recallLayer->m_fields->m_returnToPreviousLevel = true;
    recallLayer->m_fields->m_previousGauntlet = previousGauntlet;
}

void markRecallPage(LevelInfoLayer* layer) {
    static_cast<RecallAssistLevelInfoLayer*>(layer)->m_fields->m_openedFromRecall = true;
}

/*
    This is a bugfix to allow Online <-> Timed variant leaderboard opens to
    publish & fetch scores without manually refreshing the leaderboard.
    In 2.209, these leaderboards are separate.
*/
class $modify(RecallAssistLevelLeaderboard, LevelLeaderboard) {
    void loadScores() {
        if (m_level && m_type != LevelLeaderboardType::Local) {
            auto manager = GameLevelManager::sharedState();
            std::string key = manager->getLevelLeaderboardKey(m_level->m_levelID.value(), m_type, m_mode);
            /*
                Observe all pages, but only invalidate from a page opened by the
                recall popup. Ordinary level pages retain their native behavior.
                ll_<levelID>_<type>_<mode> keys are shared by all appearances
            */
            static std::unordered_map<std::string, std::pair<int, bool>> cacheVariants;
            auto variant = std::pair{m_level->m_dailyID.value(), m_level->m_gauntletLevel};
            auto [entry, inserted] = cacheVariants.try_emplace(key, variant);
            if (!inserted && entry->second != variant) {
                auto scene = CCDirector::sharedDirector()->getRunningScene();
                auto page = scene ? scene->getChildByType<LevelInfoLayer>(0) : nullptr;
                if (page && page->m_level == m_level &&
                    static_cast<RecallAssistLevelInfoLayer*>(page)->m_fields->m_openedFromRecall) {
                    manager->m_storedLevels->removeObjectForKey(key);
                    manager->resetTimerForKey(key.c_str());
                }
                entry->second = variant;
            }
        }
        LevelLeaderboard::loadScores();
    }
};