#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include "AgentManager.hpp"
#include "NeatHUD.hpp"
#include <array>

using namespace geode::prelude;
using namespace neatmod;

static std::vector<float> castSensorRays(PlayLayer* pl) {
    auto* player = pl->m_player1;
    if (!player) return std::vector<float>(5, 1.f);
    float px = player->getPositionX();
    float py = player->getPositionY();
    constexpr std::array<std::pair<float,float>, 5> dirs = {{
        {60.f,  0.f}, {60.f, 30.f}, {60.f, -30.f}, {30.f, 50.f}, {100.f, 0.f},
    }};
    std::vector<float> rays;
    for (auto [dx, dy] : dirs) {
        float sampleX = px + dx;
        float sampleY = py + dy;
        float minDist = 1.f;
        if (pl->m_objects) {
            int count = pl->m_objects->count();
            for (int i = 0; i < count; ++i) {
                auto* obj = static_cast<GameObject*>(pl->m_objects->objectAtIndex(i));
                if (!obj) continue;
                float ox = obj->getPositionX();
                float oy = obj->getPositionY();
                float dist = std::sqrt((ox-sampleX)*(ox-sampleX)+(oy-sampleY)*(oy-sampleY));
                if (dist < 80.f) minDist = std::min(minDist, dist / 80.f);
            }
        }
        rays.push_back(minDist);
    }
    return rays;
}

class $modify(NEATPlayLayer, PlayLayer) {
    struct Fields {
        NeatHUD* hud         = nullptr;
        bool     neatRunning = false;
        bool     jumpHeld    = false;
        int      resetTimer  = 0;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        auto* hud = NeatHUD::create();
        hud->setPosition({0.f, 0.f});
        hud->setZOrder(100);
        this->addChild(hud);
        m_fields.self()->hud = hud;
        int popSize  = (int)Mod::get()->getSettingValue<int64_t>("population-size");
        int maxTicks = (int)Mod::get()->getSettingValue<int64_t>("max-ticks");
        AgentManager::get().init(this, popSize, maxTicks);
        this->scheduleOnce(schedule_selector(NEATPlayLayer::startNeat), 1.0f);
        return true;
    }

    void startNeat(float) {
        m_fields.self()->neatRunning = true;
    }

    void update(float dt) {
        PlayLayer::update(dt);
        auto* fields = m_fields.self();
        if (!fields->neatRunning || !m_player1) return;

        float px    = m_player1->getPositionX();
        float py    = m_player1->getPositionY();
        float speed = m_player1->m_playerSpeed;
        auto rays   = castSensorRays(this);

        auto& mgr = AgentManager::get();
        mgr.tick(px, speed, py, rays);

        // Find best genome
        neat::Genome* bestGenome = nullptr;
        float bestFit = -1.f;
        for (auto& agent : mgr.m_agents) {
            if (!agent.dead && agent.genome && agent.fitness > bestFit) {
                bestFit = agent.fitness;
                bestGenome = agent.genome;
            }
        }

        bool shouldJump = false;
        if (bestGenome) {
            std::vector<float> inputs;
            inputs.push_back(std::clamp(speed / 20.f, 0.f, 1.f));
            inputs.push_back(std::clamp(py / 320.f, 0.f, 1.f));
            for (int i = 0; i < 5; ++i)
                inputs.push_back(i < (int)rays.size() ? rays[i] : 0.f);
            auto out = bestGenome->activate(inputs);
            shouldJump = (!out.empty() && out[0] > 0.5f);
        }

        if (shouldJump && !fields->jumpHeld) {
            m_player1->pushButton(PlayerButton::Jump);
            fields->jumpHeld = true;
        } else if (!shouldJump && fields->jumpHeld) {
            m_player1->releaseButton(PlayerButton::Jump);
            fields->jumpHeld = false;
        }

        if (fields->hud)
            fields->hud->update(mgr.generation(), mgr.aliveCount(), mgr.totalAgents(), mgr.bestFitness());
    }

    // Override destroyPlayer to prevent instant death loop
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        auto* fields = m_fields.self();
        if (!fields->neatRunning) {
            PlayLayer::destroyPlayer(player, object);
            return;
        }
        // Mark all agents dead and schedule a reset after a short delay
        auto& mgr = AgentManager::get();
        for (auto& agent : mgr.m_agents)
            agent.dead = true;
        fields->neatRunning = false;
        this->scheduleOnce(schedule_selector(NEATPlayLayer::doReset), 0.1f);
    }

    void doReset(float) {
        this->resetLevel();
        this->scheduleOnce(schedule_selector(NEATPlayLayer::startNeat), 1.0f);
    }
};

class $modify(NEATMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        auto* menu = this->getChildByID("bottom-menu");
        if (!menu) return true;
        auto* btn = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png"),
            this,
            menu_selector(NEATMenuLayer::onNeatInfo)
        );
        btn->setID("neat-info-btn");
        menu->addChild(btn);
        menu->updateLayout();
        return true;
    }
    void onNeatInfo(CCObject*) {
        FLAlertLayer::create(
            "NEAT AI Player",
            "<cg>NEAT Neural Network</c> is active!\n\n"
            "Enter any level and the AI will begin\n"
            "training with <cy>evolutionary generations</c>.\n\n"
            "Configure population size and mutation\n"
            "rates in the <co>Mod Settings</c>.",
            "OK"
        )->show();
    }
};
