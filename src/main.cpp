#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include "AgentManager.hpp"
#include "NeatHUD.hpp"
#include <array>

using namespace geode::prelude;
using namespace neatmod;

static std::string getSavePath() {
    auto dir = Mod::get()->getSaveDir();
    return (dir / "best_genome.txt").string();
}

static std::vector<float> castRays(PlayLayer* pl) {
    auto* player = pl->m_player1;
    if (!player) return std::vector<float>(5, 1.f);
    float px = player->getPositionX();
    float py = player->getPositionY();
    constexpr std::array<std::tuple<float,float,float>, 5> rays = {{
        {80.f,   0.f,  80.f},
        {80.f,  40.f,  90.f},
        {80.f, -20.f,  90.f},
        {40.f,  60.f,  70.f},
        {150.f,  0.f, 150.f},
    }};
    std::vector<float> result;
    for (auto [dx, dy, range] : rays) {
        float tx = px + dx, ty = py + dy, best = 1.f;
        if (pl->m_objects) {
            int n = pl->m_objects->count();
            for (int i = 0; i < n; ++i) {
                auto* obj = static_cast<GameObject*>(pl->m_objects->objectAtIndex(i));
                if (!obj) continue;
                float d = std::sqrt(std::pow(obj->getPositionX()-tx,2)+std::pow(obj->getPositionY()-ty,2));
                if (d < range) best = std::min(best, d / range);
            }
        }
        result.push_back(best);
    }
    return result;
}

class $modif
cat > /workspaces/GDC/src/main.cpp << 'CPPEOF'
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include "AgentManager.hpp"
#include "NeatHUD.hpp"
#include <array>

using namespace geode::prelude;
using namespace neatmod;

static std::string getSavePath() {
    auto dir = Mod::get()->getSaveDir();
    return (dir / "best_genome.txt").string();
}

static std::vector<float> castRays(PlayLayer* pl) {
    auto* player = pl->m_player1;
    if (!player) return std::vector<float>(5, 1.f);
    float px = player->getPositionX();
    float py = player->getPositionY();
    constexpr std::array<std::tuple<float,float,float>, 5> rays = {{
        {80.f,   0.f,  80.f},
        {80.f,  40.f,  90.f},
        {80.f, -20.f,  90.f},
        {40.f,  60.f,  70.f},
        {150.f,  0.f, 150.f},
    }};
    std::vector<float> result;
    for (auto [dx, dy, range] : rays) {
        float tx = px + dx, ty = py + dy, best = 1.f;
        if (pl->m_objects) {
            int n = pl->m_objects->count();
            for (int i = 0; i < n; ++i) {
                auto* obj = static_cast<GameObject*>(pl->m_objects->objectAtIndex(i));
                if (!obj) continue;
                float d = std::sqrt(std::pow(obj->getPositionX()-tx,2)+std::pow(obj->getPositionY()-ty,2));
                if (d < range) best = std::min(best, d / range);
            }
        }
        result.push_back(best);
    }
    return result;
}

class $modify(NEATPlayLayer, PlayLayer) {
    struct Fields {
        NeatHUD* hud      = nullptr;
        bool     active   = false;
        bool     jumpHeld = false;
        int      bestAgent = -1;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        auto* hud = NeatHUD::create();
        hud->setZOrder(200);
        hud->setPosition({0.f, 0.f});
        addChild(hud);
        m_fields.self()->hud = hud;
        int pop  = (int)Mod::get()->getSettingValue<int64_t>("population-size");
        int tick = (int)Mod::get()->getSettingValue<int64_t>("max-ticks");
        AgentManager::get().init(this, pop, tick, getSavePath());
        scheduleOnce(schedule_selector(NEATPlayLayer::activateNEAT), 1.0f);
        return true;
    }

    void activateNEAT(float) {
        m_fields.self()->active = true;
        log::info("[NEAT] Active!");
    }

    void update(float dt) {
        PlayLayer::update(dt);
        auto* f = m_fields.self();
        if (!f->active || !m_player1) return;

        float px = m_player1->getPositionX();
        float py = m_player1->getPositionY();
        float speed = m_player1->m_playerSpeed;
        auto rays = castRays(this);

        std::vector<float> inputs;
        inputs.push_back(std::clamp(speed / 20.f, 0.f, 1.f));
        inputs.push_back(std::clamp(py / 320.f, 0.f, 1.f));
        for (int i = 0; i < 5; ++i)
            inputs.push_back(i < (int)rays.size() ? rays[i] : 0.f);

        auto& mgr = AgentManager::get();
        mgr.tick(px, speed, py, rays);

        f->bestAgent = -1;
        float bestFit = -1.f;
        for (int i = 0; i < (int)mgr.m_agents.size(); ++i) {
            auto& a = mgr.m_agents[i];
            if (!a.dead && a.fitness > bestFit) {
                bestFit = a.fitness;
                f->bestAgent = i;
            }
        }

        bool jump = false;
        if (f->bestAgent >= 0) jump = mgr.getJump(f->bestAgent, inputs);

        if (jump && !f->jumpHeld) {
            m_player1->pushButton(PlayerButton::Jump);
            f->jumpHeld = true;
        } else if (!jump && f->jumpHeld) {
            m_player1->releaseButton(PlayerButton::Jump);
            f->jumpHeld = false;
        }

        if (f->hud)
            f->hud->update(mgr.generation(), mgr.aliveCount(), mgr.totalAgents(), mgr.bestFitness());
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        auto* f = m_fields.self();
        if (!f->active) { PlayLayer::destroyPlayer(player, object); return; }
        AgentManager::get().killAll();
        f->active = false;
        f->jumpHeld = false;
        if (m_player1) m_player1->releaseButton(PlayerButton::Jump);
        scheduleOnce(schedule_selector(NEATPlayLayer::doReset), 0.05f);
    }

    void doReset(float) {
        resetLevel();
        scheduleOnce(schedule_selector(NEATPlayLayer::activateNEAT), 1.0f);
    }
};

class $modify(NEATMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        auto* menu = getChildByID("bottom-menu");
        if (!menu) return true;
        auto* btn = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png"),
            this, menu_selector(NEATMenuLayer::onNeatInfo));
        btn->setID("neat-info-btn");
        menu->addChild(btn);
        menu->updateLayout();
        return true;
    }
    void onNeatInfo(CCObject*) {
        FLAlertLayer::create("NEAT AI Player",
            "<cg>NEAT Neural Network</c> is active!\n\n"
            "Enter any level and the AI will train\n"
            "using <cy>evolutionary generations</c>.\n\n"
            "Progress is <co>saved automatically</c>\n"
            "between sessions.", "OK")->show();
    }
};
