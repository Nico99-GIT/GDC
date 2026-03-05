#pragma once
#include <vector>
#include <unordered_map>
#include <random>
#include <cmath>
#include <algorithm>
#include <functional>
#include <string>

namespace neat {

// ─────────────────────────────────────────────
//  Innovation tracking (global counter)
// ─────────────────────────────────────────────
struct InnovationDB {
    static InnovationDB& get() {
        static InnovationDB inst;
        return inst;
    }
    int next(int from, int to) {
        auto key = std::make_pair(from, to);
        auto it = m_map.find(key);
        if (it != m_map.end()) return it->second;
        m_map[key] = ++m_counter;
        return m_counter;
    }
    void reset() { m_map.clear(); m_counter = 0; }
private:
    std::unordered_map<
        std::pair<int,int>,
        int,
        // pair hash
        decltype([](const std::pair<int,int>& p){
            return std::hash<long long>()((long long)p.first << 32 | (unsigned)p.second);
        })
    > m_map;
    int m_counter = 0;
};

// ─────────────────────────────────────────────
//  Connection gene
// ─────────────────────────────────────────────
struct ConnectionGene {
    int     in;
    int     out;
    float   weight;
    bool    enabled;
    int     innovation;
};

// ─────────────────────────────────────────────
//  Genome
// ─────────────────────────────────────────────
struct Genome {
    int numInputs;
    int numOutputs;
    int nextNodeId;                          // auto-incrementing node ID
    std::vector<ConnectionGene> connections; // all connection genes
    float fitness = 0.f;

    Genome() = default;
    Genome(int inputs, int outputs)
        : numInputs(inputs), numOutputs(outputs), nextNodeId(inputs + outputs + 1) {}

    // ── Activation ──────────────────────────────
    static float sigmoid(float x) { return 1.f / (1.f + std::exp(-4.9f * x)); }

    // ── Forward pass ────────────────────────────
    // Returns output activations given input values.
    std::vector<float> activate(const std::vector<float>& inputs) const {
        // Collect all node IDs
        std::unordered_map<int, float> activation;

        // Bias node = 0, inputs = 1..numInputs
        activation[0] = 1.f;
        for (int i = 0; i < (int)inputs.size(); ++i)
            activation[i + 1] = inputs[i];

        // Outputs: numInputs+1 .. numInputs+numOutputs
        // Simple iterative evaluation (good enough for small networks)
        // Run several passes so signals propagate through hidden nodes
        for (int pass = 0; pass < 10; ++pass) {
            for (auto& conn : connections) {
                if (!conn.enabled) continue;
                auto it = activation.find(conn.in);
                float inVal = (it != activation.end()) ? it->second : 0.f;
                activation[conn.out] += inVal * conn.weight;
            }
        }

        std::vector<float> out;
        out.reserve(numOutputs);
        for (int i = 0; i < numOutputs; ++i)
            out.push_back(sigmoid(activation[numInputs + 1 + i]));
        return out;
    }

    // ── Mutation: perturb weights ────────────────
    void mutateWeights(float rate, float perturbStrength, std::mt19937& rng) {
        std::uniform_real_distribution<float> chance(0.f, 1.f);
        std::normal_distribution<float> perturb(0.f, perturbStrength);
        std::uniform_real_distribution<float> newW(-1.f, 1.f);

        for (auto& c : connections) {
            if (chance(rng) < rate) {
                if (chance(rng) < 0.9f)
                    c.weight += perturb(rng);
                else
                    c.weight = newW(rng);
                c.weight = std::clamp(c.weight, -4.f, 4.f);
            }
        }
    }

    // ── Mutation: add connection ─────────────────
    void mutateAddConnection(std::mt19937& rng) {
        // Gather all node IDs
        std::vector<int> nodes;
        nodes.push_back(0); // bias
        for (int i = 1; i <= numInputs; ++i) nodes.push_back(i);
        for (int i = 0; i < numOutputs; ++i) nodes.push_back(numInputs + 1 + i);
        for (auto& c : connections)
            for (int n : {c.in, c.out})
                if (std::find(nodes.begin(), nodes.end(), n) == nodes.end())
                    nodes.push_back(n);

        if (nodes.size() < 2) return;

        std::uniform_int_distribution<int> pick(0, (int)nodes.size() - 1);
        int from = nodes[pick(rng)];
        int to   = nodes[pick(rng)];
        if (from == to) return;

        // Check if already exists
        for (auto& c : connections)
            if (c.in == from && c.out == to) { c.enabled = true; return; }

        std::uniform_real_distribution<float> wDist(-1.f, 1.f);
        int innov = InnovationDB::get().next(from, to);
        connections.push_back({from, to, wDist(rng), true, innov});
    }

    // ── Mutation: add node ───────────────────────
    void mutateAddNode(std::mt19937& rng) {
        // Pick a random enabled connection to split
        std::vector<int> enabled;
        for (int i = 0; i < (int)connections.size(); ++i)
            if (connections[i].enabled) enabled.push_back(i);
        if (enabled.empty()) return;

        std::uniform_int_distribution<int> pick(0, (int)enabled.size() - 1);
        int idx = enabled[pick(rng)];
        auto& old = connections[idx];
        old.enabled = false;

        int newNode = nextNodeId++;
        int innov1 = InnovationDB::get().next(old.in, newNode);
        int innov2 = InnovationDB::get().next(newNode, old.out);

        connections.push_back({old.in,  newNode, 1.f,        true, innov1});
        connections.push_back({newNode, old.out, old.weight, true, innov2});
    }

    // ── Compatibility distance ───────────────────
    float compatibilityDistance(const Genome& other, float c1, float c2, float c3) const {
        // Align by innovation number
        auto makemap = [](const std::vector<ConnectionGene>& genes) {
            std::unordered_map<int, float> m;
            for (auto& g : genes) m[g.innovation] = g.weight;
            return m;
        };
        auto ma = makemap(connections);
        auto mb = makemap(other.connections);

        int disjoint = 0, excess = 0;
        float wDiff = 0.f;
        int matching = 0;

        int maxA = connections.empty() ? 0 : connections.back().innovation;
        int maxB = other.connections.empty() ? 0 : other.connections.back().innovation;
        int maxInnov = std::max(maxA, maxB);

        for (auto& [innov, w] : ma) {
            auto it = mb.find(innov);
            if (it != mb.end()) {
                ++matching;
                wDiff += std::abs(w - it->second);
            } else {
                if (innov > maxB) ++excess; else ++disjoint;
            }
        }
        for (auto& [innov, w] : mb) {
            if (ma.find(innov) == ma.end()) {
                if (innov > maxA) ++excess; else ++disjoint;
            }
        }

        float N = std::max(1.f, (float)std::max(connections.size(), other.connections.size()));
        float avgW = matching > 0 ? wDiff / matching : 0.f;
        return c1 * excess / N + c2 * disjoint / N + c3 * avgW;
    }
};

// ─────────────────────────────────────────────
//  Crossover
// ─────────────────────────────────────────────
inline Genome crossover(const Genome& parent1, const Genome& parent2, std::mt19937& rng) {
    // parent1 should be the more fit one (caller's responsibility)
    Genome child(parent1.numInputs, parent1.numOutputs);
    child.nextNodeId = std::max(parent1.nextNodeId, parent2.nextNodeId);

    auto makemap = [](const std::vector<ConnectionGene>& g) {
        std::unordered_map<int, const ConnectionGene*> m;
        for (auto& c : g) m[c.innovation] = &c;
        return m;
    };
    auto mp2 = makemap(parent2.connections);

    std::uniform_real_distribution<float> coin(0.f, 1.f);
    for (auto& gene : parent1.connections) {
        auto it = mp2.find(gene.innovation);
        if (it != mp2.end()) {
            // Matching gene – pick randomly
            child.connections.push_back(coin(rng) < 0.5f ? gene : *it->second);
        } else {
            // Disjoint/excess – inherit from fitter parent (parent1)
            child.connections.push_back(gene);
        }
        // Re-enable gene with small probability if either parent had it disabled
        if (!child.connections.back().enabled && coin(rng) < 0.25f)
            child.connections.back().enabled = true;
    }
    return child;
}

// ─────────────────────────────────────────────
//  Species
// ─────────────────────────────────────────────
struct Species {
    std::vector<Genome*> members;
    Genome               representative;
    float                sharedFitness = 0.f;
    int                  staleness     = 0;
    float                bestFitness   = 0.f;
};

// ─────────────────────────────────────────────
//  Population / NEAT controller
// ─────────────────────────────────────────────
class Population {
public:
    // NEAT hyper-parameters
    struct Config {
        int   populationSize   = 50;
        float c1               = 1.f;  // excess coefficient
        float c2               = 1.f;  // disjoint coefficient
        float c3               = 0.4f; // weight diff coefficient
        float compatThreshold  = 3.f;
        float mutateWeightRate = 0.8f;
        float perturbStrength  = 0.1f;
        float addConnRate      = 0.05f;
        float addNodeRate      = 0.03f;
        float crossoverRate    = 0.75f;
        int   maxStaleness     = 15;
    };

    Config                    cfg;
    std::vector<Genome>       genomes;
    std::vector<Species>      species;
    int                       generation = 0;
    float                     bestFitness = 0.f;
    std::mt19937              rng;

    Population(int inputs, int outputs, const Config& c = {})
        : cfg(c), rng(std::random_device{}()) {
        // Build initial fully-connected population
        genomes.reserve(cfg.populationSize);
        std::uniform_real_distribution<float> wDist(-1.f, 1.f);
        for (int i = 0; i < cfg.populationSize; ++i) {
            Genome g(inputs, outputs);
            // Connect every input (+ bias) to every output
            for (int in = 0; in <= inputs; ++in)
                for (int out = 0; out < outputs; ++out) {
                    int outNode = inputs + 1 + out;
                    int innov = InnovationDB::get().next(in, outNode);
                    g.connections.push_back({in, outNode, wDist(rng), true, innov});
                }
            genomes.push_back(g);
        }
    }

    // Call this after evaluating all genomes (setting genome.fitness)
    void evolve() {
        // ── 1. Speciate ──────────────────────────
        for (auto& sp : species) sp.members.clear();

        for (auto& g : genomes) {
            bool placed = false;
            for (auto& sp : species) {
                if (g.compatibilityDistance(sp.representative, cfg.c1, cfg.c2, cfg.c3)
                        < cfg.compatThreshold) {
                    sp.members.push_back(&g);
                    placed = true;
                    break;
                }
            }
            if (!placed) {
                Species ns;
                ns.representative = g;
                ns.members.push_back(&g);
                species.push_back(std::move(ns));
            }
        }

        // Remove empty species
        species.erase(
            std::remove_if(species.begin(), species.end(),
                [](const Species& s){ return s.members.empty(); }),
            species.end());

        // ── 2. Fitness sharing & staleness ───────
        float totalShared = 0.f;
        for (auto& sp : species) {
            float sum = 0.f;
            for (auto* g : sp.members)
                sum += g->fitness / (float)sp.members.size();
            sp.sharedFitness = sum;
            totalShared += sum;

            float maxF = 0.f;
            for (auto* g : sp.members) maxF = std::max(maxF, g->fitness);
            if (maxF > sp.bestFitness) { sp.bestFitness = maxF; sp.staleness = 0; }
            else                        ++sp.staleness;

            // Update representative
            std::uniform_int_distribution<int> pick(0, (int)sp.members.size()-1);
            sp.representative = *sp.members[pick(rng)];
        }

        // Remove stale species (but protect champion)
        if (species.size() > 1) {
            species.erase(
                std::remove_if(species.begin(), species.end(),
                    [&](const Species& s){ return s.staleness > cfg.maxStaleness; }),
                species.end());
        }
        if (species.empty()) return; // safety

        // ── 3. Offspring allocation ──────────────
        if (totalShared <= 0.f) totalShared = 1.f;
        std::vector<int> offspring(species.size());
        int allocated = 0;
        for (int i = 0; i < (int)species.size(); ++i) {
            offspring[i] = (int)(species[i].sharedFitness / totalShared * cfg.populationSize);
            allocated += offspring[i];
        }
        // Fill remainder
        for (int i = 0; allocated < cfg.populationSize; ++i % (int)species.size(), ++allocated)
            ++offspring[i % species.size()];

        // ── 4. Breed new generation ──────────────
        std::vector<Genome> next;
        next.reserve(cfg.populationSize);

        std::uniform_real_distribution<float> coin(0.f, 1.f);
        for (int si = 0; si < (int)species.size(); ++si) {
            auto& sp = species[si];
            // Sort members by fitness (best first)
            std::sort(sp.members.begin(), sp.members.end(),
                [](Genome* a, Genome* b){ return a->fitness > b->fitness; });

            // Elitism: keep champion unchanged
            if (!sp.members.empty())
                next.push_back(*sp.members[0]);

            int toBreed = std::max(0, offspring[si] - 1);
            for (int c = 0; c < toBreed; ++c) {
                Genome child;
                if (sp.members.size() > 1 && coin(rng) < cfg.crossoverRate) {
                    // Pick two parents proportional to fitness (simple random from top half)
                    int half = std::max(1, (int)sp.members.size() / 2);
                    std::uniform_int_distribution<int> pp(0, half-1);
                    Genome* p1 = sp.members[pp(rng)];
                    Genome* p2 = sp.members[pp(rng)];
                    if (p1->fitness < p2->fitness) std::swap(p1, p2);
                    child = crossover(*p1, *p2, rng);
                } else {
                    // Clone a random member
                    std::uniform_int_distribution<int> pm(0, (int)sp.members.size()-1);
                    child = *sp.members[pm(rng)];
                }

                // Mutate
                if (coin(rng) < cfg.mutateWeightRate)
                    child.mutateWeights(cfg.mutateWeightRate, cfg.perturbStrength, rng);
                if (coin(rng) < cfg.addConnRate)
                    child.mutateAddConnection(rng);
                if (coin(rng) < cfg.addNodeRate)
                    child.mutateAddNode(rng);

                child.fitness = 0.f;
                next.push_back(std::move(child));
            }
        }

        // Trim/pad if needed
        while ((int)next.size() < cfg.populationSize)
            next.push_back(next[0]);
        next.resize(cfg.populationSize);

        genomes = std::move(next);
        ++generation;

        // Track best
        for (auto& g : genomes)
            bestFitness = std::max(bestFitness, g.fitness);
    }
};

} // namespace neat
