#include <Geode/Geode.hpp>
#include <Geode/binding/GauntletSelectLayer.hpp>
#include <Geode/modify/GameLevelManager.hpp>
#include "../utils/NetUtils.hpp"

using namespace geode::prelude;

namespace {
    constexpr auto GAUNTLET_REQUEST_TAG = "get_gauntlets";
    bool s_prefetchingGauntlets = false;
}

void NetUtils::loadGauntletRecallEntries() {
    auto manager = GameLevelManager::sharedState();
    if (manager->m_downloadObjects->objectForKey(GAUNTLET_REQUEST_TAG)) {
        log::info("gauntlet request already in progress");
        return;
    }

    log::info("requesting gauntlets through vanilla loader");
    s_prefetchingGauntlets = true;
    manager->getGauntlets();
}

class $modify(RecallAssistGameLevelManager, GameLevelManager) {
    void onGetGauntletsCompleted(std::string response, std::string tag) {
        bool prefetch = tag == GAUNTLET_REQUEST_TAG && std::exchange(s_prefetchingGauntlets, false);

        // Make sure this unrelated request isn't consumed by gauntlet screen //
        auto delegate = m_levelManagerDelegate;
        bool suppressDelegate = prefetch && delegate && !geode::cast::typeinfo_cast<GauntletSelectLayer*>(delegate);
        if (suppressDelegate) m_levelManagerDelegate = nullptr;
        GameLevelManager::onGetGauntletsCompleted(std::move(response), std::move(tag));
        if (suppressDelegate && !m_levelManagerDelegate) m_levelManagerDelegate = delegate;
        if (prefetch) log::info("game gauntlet request completed with {} cached lineups", m_savedGauntlets->count());
    }
};
