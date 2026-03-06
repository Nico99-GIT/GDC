#pragma once
#include "NEAT.hpp"
#include <vector>
#include <string>

class PlayLayer;

namespace neatmod {

constexpr int NUM_INPUTS  = 7;
constexpr int NUM_OUTPUTS = 1;

struct Agent {
    neat::Genome* genome     = nullptr;
    bool          dead       = false;
    float         xPos       = 0.f;
    float         fitness    = 0.f;
    int           ticksAlive = 0;
};

class AgentManager {
public:
    static AgentManager& get();
    void  init(PlayLayer* pl, int populationSize, int maxTicks, const std::string& savePath);
    bool  tick(float x, float speed, float y, const std::vector<float>& rays);
    bool  getJump(int agentIndex, const std::vector<float>& inputs);
    int   generation()  const { return m_pop ? m_pop->generation : 0; }
    float bestFitness() const { return m_pop ? m_pop->bestFitness : 0.f; }
    int   aliveCount()  const;
    int   totalAgents() const { return (int)m_agents.size(); }
    bool  isRunning()   const { return m_running; }
    void  killAll();
    void  stop();
    std::vector<Agent> m_agents;
private:
    std::unique_ptr<neat::Population> m_pop;
    PlayLayer*  m_playLayer    = nullptr;
    std::string m_savePath;
    int         m_maxTicks     = 3600;
    int         m_currentTick  = 0;
    bool        m_running      = false;
    void startGeneration();
    void endGeneration();
    std::vector<float> buildInputs(float speed, float y, const std::vector<float>& rays) const;
};

} // namespace neatmod
