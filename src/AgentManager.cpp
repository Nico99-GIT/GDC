#include "AgentManager.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <numeric>

using namespace geode::prelude;
using namespace neatmod;

AgentManager& AgentManager::get() {
    static AgentManager inst;
    return inst;
}

void AgentManager::init(PlayLayer* pl, int populationSize, int maxTicks) {
    m_playLayer = pl;
    m_maxTicks  = maxTicks;
    m_running   = false;

    neat::Population::Config cfg;
    cfg.populationSize = populationSize;
    cfg.addConnRate    = 0.5f;  // higher chance of new connections
    cfg.addNodeRate    = 0.2f;
    cfg.perturbStrength = 0.5f;
    cfg.mutateWeightRate = 0.9f;  // higher chance of new nodes
    m_pop = std::make_unique<neat::Population>(NUM_INPUTS, NUM_OUTPUTS, cfg);

    m_agents.resize(populationSize);
    startGeneration();
}

void AgentManager::startGeneration() {
    m_currentTick = 0;
    m_running = true;

    for (int i = 0; i < (int)m_agents.size(); ++i) {
        auto& a = m_agents[i];
        a.genome     = &m_pop->genomes[i];
        a.dead       = false;
        a.xPos       = 0.f;
        a.fitness    = 0.f;
        a.ticksAlive = 0;
    }

    log::info("[NEAT] Generation {} started – {} agents", m_pop->generation, m_agents.size());
}

void AgentManager::endGeneration() {
    for (auto& a : m_agents) {
        if (a.genome)
            // Fitness = distance + time alive bonus
            a.genome->fitness = a.xPos + (float)a.ticksAlive * 0.5f;
    }

    m_pop->evolve();
    m_agents.resize(m_pop->genomes.size());
    startGeneration();
}

bool AgentManager::tick(float levelX, float levelSpeed, float playerY,
                        const std::vector<float>& sensorRays) {
    if (!m_running) return false;

    ++m_currentTick;
    bool anyAlive = false;

    auto inputs = buildInputs(levelSpeed, playerY, sensorRays);

    for (auto& a : m_agents) {
        if (a.dead) continue;
        anyAlive = true;

        ++a.ticksAlive;
        a.xPos = std::max(a.xPos, levelX);
        // Fitness updated live
        a.fitness = a.xPos + (float)a.ticksAlive * 0.5f;

        if (a.ticksAlive >= m_maxTicks) a.dead = true;
    }

    if (!anyAlive || m_currentTick >= m_maxTicks) {
        endGeneration();
        return false;
    }
    return true;
}

int AgentManager::aliveCount() const {
    int c = 0;
    for (auto& a : m_agents) if (!a.dead) ++c;
    return c;
}

void AgentManager::stop() {
    m_running = false;
    log::info("[NEAT] Simulation stopped.");
}

std::vector<float> AgentManager::buildInputs(float speed, float y,
                                              const std::vector<float>& rays) const {
    std::vector<float> in;
    in.reserve(NUM_INPUTS);
    in.push_back(std::clamp(speed / 20.f, 0.f, 1.f));
    in.push_back(std::clamp(y / 320.f, 0.f, 1.f));
    for (int i = 0; i < 5; ++i)
        in.push_back(i < (int)rays.size() ? rays[i] : 0.f);
    while ((int)in.size() < NUM_INPUTS) in.push_back(0.f);
    return in;
}
