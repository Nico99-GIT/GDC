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
        {60.f,  0.f},
        {60.f,  30.f},
        {60.f, -30.f},
        {30.f,  50.f},
        {100.f, 0.f},
    }};

    std::vector<float> rays;
    rays.reserve(5);

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
                if (dist < 80.f)
                    minDist = std::min(minDist, dist / 80.f);
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
        bool     shouldJump  = false;
        bool     jumpHeld    = false;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        auto* fields = m_fields.self();

        auto* hud = NeatHUD::create();
        hud->setPosition({0.f, 0.f});
        hud->setZOrder(100);
        this->addChild(hud);
        fields->hud = hud;

        int popSize  = (int)Mod::get()->getSettingValue<int64_t>("population-size");
        int maxTicks = (int)Mod::get()->getSettingValue<int64_t>("max-ticks");
        AgentManager::get().init(this, popSize, maxTicks);

        // Delay NEAT takeover so level fully loads first
        this->scheduleOnce(schedule_selector(NEATPlayLayer::startNeat), 0.5f);

        log::info("[NEAT] Initialized for level: {}", level->m_levelName);
        return true;
    }

    void startNeat(float) {
        m_fields.self()->neatRunning = true;
        log::info("[NEAT] Taking control!");
    }

    void update(float dt) {
        PlayLayer::update(dt);

        auto* fields = m_fields.self();
        if (!fields->neatRunning) return;
        if (!m_player1) return;

        float px    = m_player1->getPositionX();
        float py    = m_player1->getPositionY();
        float speed = m_player1->m_playerSpeed;

        auto rays = castSensorRays(this);
        AgentManager::get().tick(px, speed, py, rays);

        // Find best alive agent
        auto& mgr = AgentManager::get();
        float bestFit = -1.f;
        neat::Genome* bestGenome = nullptr;
        for (auto& agent : mgr.m_agents) {
            if (!agent.dead && agent.genome && agent.fitness > bestFit) {
                bestFit    = agent.fitness;
                bestGenome = agent.genome;
            }
        }

        if (bestGenome) {
            std::vector<float> inputs;
            inputs.reserve(NUM_INPUTS);
            inputs.push_back(std::clamp(speed / 20.f, 0.f, 1.f));
            inputs.push_back(std::clamp(py / 320.f, 0.f, 1.f));
            for (int i = 0; i < 5; ++i)
                inputs.push_back(i < (int)rays.size() ? rays[i] : 0.f);

            auto out = bestGenome->activate(inputs);
            fields->shouldJump = (!out.empty() && out[0] > 0.5f);
        }

        // Inject jump
        if (fields->shouldJump && !fields->jumpHeld) {
            m_player1->pushButton(PlayerButton::Jump);
            fields->jumpHeld = true;
        } else if (!fields->shouldJump && fields->jumpHeld) {
            m_player1->releaseButton(PlayerButton::Jump);
            fields->jumpHeld = false;
        }

        // Update HUD
        if (fields->hud) {
            fields->hud->update(
                mgr.generation(),
                mgr.aliveCount(),
                mgr.totalAgents(),
                mgr.bestFitness()
            );
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        auto* fields = m_fields.self();
        if (!fields->neatRunning || !object) {
            PlayLayer::destroyPlayer(player, object);
            return;
        }
        auto& mgr = AgentManager::get();
        for (auto& agent : mgr.m_agents)
            agent.dead = true;
        this->resetLevel();
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
