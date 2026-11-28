#include <Geode/Geode.hpp>
#include <Geode/binding/GauntletSelectLayer.hpp>
#include <Geode/modify/GameLevelManager.hpp>
#include "../utils/NetUtils.hpp"

using namespace geode::prelude;

namespace {
    constexpr auto GAUNTLET_REQUEST_TAG = "get_gauntlets";
    enum class GauntletLoad { Inactive, Existing, Prefetch };
    GauntletLoad s_gauntletLoad = GauntletLoad::Inactive;
}

void NetUtils::loadGauntletRecallEntries() {
    auto manager = GameLevelManager::sharedState();
    if (manager->m_downloadObjects->objectForKey(GAUNTLET_REQUEST_TAG)) {
        s_gauntletLoad = GauntletLoad::Existing;
        log::info("gauntlet request already in progress");
        return;
    }

    log::info("requesting gauntlets through vanilla loader");
    s_gauntletLoad = GauntletLoad::Prefetch;
    manager->getGauntlets();
}

class $modify(RecallAssistGameLevelManager, GameLevelManager) {
    void onGetGauntletsCompleted(std::string response, std::string tag) {
        auto load = tag == GAUNTLET_REQUEST_TAG
            ? std::exchange(s_gauntletLoad, GauntletLoad::Inactive) : GauntletLoad::Inactive;
        bool prefetch = load == GauntletLoad::Prefetch;

        // Make sure this unrelated request isn't consumed by gauntlet screen //
        auto delegate = m_levelManagerDelegate;
        bool suppressDelegate = prefetch && delegate && !geode::cast::typeinfo_cast<GauntletSelectLayer*>(delegate);
        if (suppressDelegate) m_levelManagerDelegate = nullptr;
        GameLevelManager::onGetGauntletsCompleted(std::move(response), std::move(tag));
        if (suppressDelegate && !m_levelManagerDelegate) m_levelManagerDelegate = delegate;
        if (prefetch) log::info("game gauntlet request completed with {} cached lineups", m_savedGauntlets->count());
        if (load != GauntletLoad::Inactive) {
            NetUtils::settleInitialRecallLoad(NetUtils::InitialRecallLoad::Gauntlets);
        }
    }
};
