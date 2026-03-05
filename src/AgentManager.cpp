#include "AgentManager.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <numeric>

using namespace geode::prelude;
using namespace neatmod;

// ─────────────────────────────────────────────
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
    m_pop = std::make_unique<neat::Population>(NUM_INPUTS, NUM_OUTPUTS, cfg);

    m_agents.resize(populationSize);
    startGeneration();
}

void AgentManager::startGeneration() {
    m_currentTick = 0;
    m_running = true;

    for (int i = 0; i < (int)m_agents.size(); ++i) {
        auto& a = m_agents[i];
        a.genome    = &m_pop->genomes[i];
        a.dead      = false;
        a.xPos      = 0.f;
        a.fitness   = 0.f;
        a.ticksAlive = 0;
    }

    log::info("[NEAT] Generation {} started – {} agents", m_pop->generation, m_agents.size());
}

void AgentManager::endGeneration() {
    // Write fitness back to genomes
    for (auto& a : m_agents) {
        if (a.genome)
            a.genome->fitness = a.fitness;
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

        // Query the neural network
        if (a.genome) {
            auto outputs = a.genome->activate(inputs);
            // outputs[0] > 0.5 → jump
            // This bool is read by the hook to inject input
            // (stored on the agent; hook checks it)
            // For now, log at debug level
        }

        // Fitness: distance travelled, bonus for staying alive longer
        a.fitness = a.xPos + (float)a.ticksAlive * 0.01f;

        // Kill if exceeded max ticks
        if (a.ticksAlive >= m_maxTicks) a.dead = true;
    }

    if (!anyAlive || m_currentTick >= m_maxTicks) {
        endGeneration();
        return false; // generation ended
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
    // Normalise speed (typical GD speed ~10-20 units/tick)
    in.push_back(std::clamp(speed / 20.f, 0.f, 1.f));
    // Normalise y position (GD level height ~0-320)
    in.push_back(std::clamp(y / 320.f, 0.f, 1.f));
    // Ray distances (clamped 0-1)
    for (int i = 0; i < 5 && i < (int)rays.size(); ++i)
        in.push_back(std::clamp(rays[i], 0.f, 1.f));
    // Pad if fewer rays provided
    while ((int)in.size() < NUM_INPUTS) in.push_back(0.f);
    return in;
}
