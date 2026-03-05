# GDC

Trains a **NEAT (NeuroEvolution of Augmenting Topologies)** neural network to autonomously beat Geometry Dash levels through evolutionary generations.

## How it works

1. **Population** — A configurable number of AI agents (default 50) are created, each with a small neural network genome.
2. **Sensors** — Each agent reads 7 inputs: 5 ray-cast distances to nearby obstacles, the player's current speed, and the player's Y position.
3. **Decision** — The network outputs a single value; if >0.5 the agent presses Jump.
4. **Fitness** — Agents are scored by how far right they travel before dying.
5. **Evolution** — After all agents die (or the time limit is hit), NEAT selects the best genomes, performs crossover and mutation, and starts a new generation.

## Features

* Full NEAT implementation: innovation tracking, speciation, fitness sharing, crossover, weight \& structural mutations
* On-screen HUD showing generation number, alive count, and best fitness
* Configurable population size, mutation rate, and tick limit via Mod Settings
* Elitism: the best genome from each species is carried unchanged into the next generation

## Settings

|Setting|Description|Default|
|-|-|-|
|Population Size|Agents per generation|50|
|Max Ticks|Ticks before forced death|3600|
|Mutation Rate|Probability of weight change|0.8|
|Show All Agents|Render ghost players|true|
|Show HUD|Display stats overlay|true|



