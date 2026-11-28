#pragma once

#include "Constants.hpp"
#include <optional>
#include <Geode/ui/Popup.hpp>
#include <Geode/binding/LevelDownloadDelegate.hpp>

using namespace geode::prelude;

void markGauntletRecallReturn(LevelInfoLayer* layer, int previousGauntlet);
void markRecallPage(LevelInfoLayer* layer);

class AvailableRecallsPopup : public Popup, public LevelDownloadDelegate {
    protected:
        Ref<GJGameLevel> m_level;
        LevelDownloadDelegate* m_previousDownloadDelegate = nullptr;
        int m_pendingTimedID = 0;
        bool m_pendingGauntlet = false;
        std::optional<GauntletType> m_pendingGauntletType;
        Ref<FLAlertLayer> m_unlockPopup;
        Ref<CCMenuItemSpriteExtra> m_downloadButton;
        Ref<CCNode> m_downloadSpinner;

        bool init(GJGameLevel* level);
        ~AvailableRecallsPopup() override;
        CCNode* createAvailableRecallContainer(LevelCategory category, std::optional<GauntletType> gauntlet, int timedID);
        void openRecall(GJGameLevel* level, std::optional<GauntletType> gauntlet = std::nullopt);
        void loadOnlineRecall(CCMenuItemSpriteExtra* button);
        void restoreDownloadDelegate();
        void clearUnlockPrompt();
        void showDownloadIndicator(CCMenuItemSpriteExtra* button);
        void clearDownloadIndicator();
    public:
        static AvailableRecallsPopup* create(GJGameLevel* level);
        void onPlayRecall(CCObject* sender);
        void onClose(CCObject* sender) override;
        void levelDownloadFinished(GJGameLevel* level) override;
        void levelDownloadFailed(int response) override;
};
