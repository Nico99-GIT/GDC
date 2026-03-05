#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include "AgentManager.hpp"
#include "NeatHUD.hpp"
#include <array>

using namespace geode::prelude;
using namespace neatmod;

// ═══════════════════════════════════════════════════════════════════
//  Sensor helper – casts rays forward/diagonal to detect obstacles
//  Returns distances in 0-1 range (1 = no obstacle in range)
// ═══════════════════════════════════════════════════════════════════
static std::vector<float> castSensorRays(PlayLayer* pl) {
    // In a real implementation you'd iterate over the obstacle
    // arrays in PlayLayer. We approximate with 5 normalized values.
    // Actual reverse-engineered offsets for GD 2.2:
    //   m_groundLayer, m_hazardLayer, m_objectLayer, etc.
    // For a working skeleton we return placeholder data.
    // Replace with real CCArray iteration against m_objects.

    auto* player = pl->m_player1;
    if (!player) return std::vector<float>(5, 1.f);

    float px = player->getPositionX();
    float py = player->getPositionY();

    // Ray offsets (x ahead, y offset) – check 5 directions
    constexpr std::array<std::pair<float,float>, 5> dirs = {{
        {60.f,  0.f},   // straight ahead
        {60.f,  30.f},  // up-right
        {60.f, -30.f},  // down-right
        {30.f,  50.f},  // up-close
        {100.f, 0.f},   // far ahead
    }};

    std::vector<float> rays;
    rays.reserve(5);

    for (auto [dx, dy] : dirs) {
        float sampleX = px + dx;
        float sampleY = py + dy;

        // Walk the active objects list and find closest in range
        // This is a simplified proximity check
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

// ═══════════════════════════════════════════════════════════════════
//  PlayLayer hook
// ═══════════════════════════════════════════════════════════════════
class $modify(NEATPlayLayer, PlayLayer) {

    struct Fields {
        NeatHUD* hud          = nullptr;
        bool     neatRunning  = false;
        int      tickCounter  = 0;
        // Per-tick jump decision for the current best agent
        bool     shouldJump   = false;
    };

    // ── Level init ──────────────────────────────
    bool init(GJGameLevel* level, bool useReplay) {
        if (!PlayLayer::init(level, useReplay, false)) return false;

        auto* fields = m_fields.self();

        // Add HUD
        auto* hud = NeatHUD::create();
        hud->setPosition({0.f, 0.f});
        hud->setZOrder(100);
        this->addChild(hud);
        fields->hud = hud;

        // Start NEAT
        int popSize  = Mod::get()->getSettingValue<int64_t>("population-size");
        int maxTicks = Mod::get()->getSettingValue<int64_t>("max-ticks");
        AgentManager::get().init(this, popSize, maxTicks);
        fields->neatRunning = true;

        log::info("[NEAT] Initialized for level: {}", level->m_levelName);
        return true;
    }

    // ── Physics update ───────────────────────────
    // GD calls this every physics step (240 tps)
    void update(float dt) {
        PlayLayer::update(dt);

        auto* fields = m_fields.self();
        if (!fields->neatRunning) return;
        if (!m_player1) return;

        ++fields->tickCounter;

        float px    = m_player1->getPositionX();
        float py    = m_player1->getPositionY();
        float speed = m_player1->m_playerSpeed;

        auto rays = castSensorRays(this);

        // Tick all agents
        bool stillRunning = AgentManager::get().tick(px, speed, py, rays);

        // ── Best agent controls real player ──────
        // Find the alive agent with highest fitness and use its decision
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

        // ── Inject jump input if needed ───────────
        if (fields->shouldJump)
            m_player1->pushButton(PlayerButton::Jump);
        else
            m_player1->releaseButton(PlayerButton::Jump);

        // ── Update HUD ────────────────────────────
        if (fields->hud) {
            fields->hud->update(
                mgr.generation(),
                mgr.aliveCount(),
                mgr.totalAgents(),
                mgr.bestFitness()
            );
        }

        if (!stillRunning && Mod::get()->getSettingValue<bool>("show-hud")) {
            log::info("[NEAT] Gen {} complete. Best fitness: {:.1f}",
                      mgr.generation(), mgr.bestFitness());
        }
    }

    // ── Player death ─────────────────────────────
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        auto* fields = m_fields.self();

        // Mark the corresponding agent as dead
        auto& mgr = AgentManager::get();
        for (auto& agent : mgr.m_agents) {
            if (!agent.dead)
                agent.dead = true; // All agents share the same player death for now
        }

        // Don't actually destroy – restart for next gen
        // We simply reset the level to re-run the generation
        if (fields->neatRunning) {
            this->resetLevel();
            return;
        }

        PlayLayer::destroyPlayer(player, object);
    }
};

// ═══════════════════════════════════════════════════════════════════
//  MenuLayer hook – adds a "NEAT AI" button to the main menu
// ═══════════════════════════════════════════════════════════════════
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
