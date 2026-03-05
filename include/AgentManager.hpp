#pragma once
#include "NEAT.hpp"
#include <vector>

// Forward declarations (Geode/GD types)
class PlayerObject;
class GJGameLevel;
class PlayLayer;

namespace neatmod {

// ─────────────────────────────────────────────
//  Sensor ray directions (angles in degrees)
// ─────────────────────────────────────────────
constexpr int   NUM_INPUTS  = 7; // 5 rays + speed + y-pos
constexpr int   NUM_OUTPUTS = 1; // jump (>0.5 = press)

// ─────────────────────────────────────────────
//  Per-agent simulation state
// ─────────────────────────────────────────────
struct Agent {
    neat::Genome* genome   = nullptr;
    bool          dead     = false;
    float         xPos     = 0.f;   // furthest x reached
    float         fitness  = 0.f;
    int           ticksAlive = 0;

    // Ghost node (thin wrapper – actual CCNode* cast)
    // We store it as void* to avoid including Cocos2d headers here
    void* ghostNode = nullptr;
};

// ─────────────────────────────────────────────
//  AgentManager: drives the population through
//  physics ticks, collects fitness, requests
//  evolution when all agents are dead.
// ─────────────────────────────────────────────
class AgentManager {
public:
    static AgentManager& get();

    // Call once when a level starts / NEAT session begins
    void init(PlayLayer* pl, int populationSize, int maxTicks);

    // Called every physics tick from the hooked update
    // Returns true if the generation is still running
    bool tick(float levelX, float levelSpeed, float playerY,
              const std::vector<float>& sensorInputs);

    // Getters for HUD
    int   generation()   const { return m_pop ? m_pop->generation : 0; }
    float bestFitness()  const { return m_pop ? m_pop->bestFitness : 0.f; }
    int   aliveCount()   const;
    int   totalAgents()  const { return (int)m_agents.size(); }
    bool  isRunning()    const { return m_running; }

    void stop();

// Public for hooks
    std::vector<Agent> m_agents;

private:
    std::unique_ptr<neat::Population> m_pop;
    std::vector<Agent>                m_agents;
    PlayLayer*                        m_playLayer = nullptr;
    int                               m_maxTicks  = 3600;
    bool                              m_running   = false;
    int                               m_currentTick = 0;

    void startGeneration();
    void endGeneration();
    std::vector<float> buildInputs(float speed, float y,
                                   const std::vector<float>& rays) const;
};

} // namespace neatmod
