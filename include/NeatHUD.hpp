#pragma once
#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace neatmod {

// ─────────────────────────────────────────────
//  On-screen HUD showing NEAT stats
// ─────────────────────────────────────────────
class NeatHUD : public CCNode {
public:
    static NeatHUD* create() {
        auto* node = new NeatHUD();
        if (node && node->init()) { node->autorelease(); return node; }
        CC_SAFE_DELETE(node);
        return nullptr;
    }

    bool init() {
        if (!CCNode::init()) return false;

        // Semi-transparent background panel
        m_bg = CCLayerColor::create({0, 0, 0, 140}, 210.f, 70.f);
        m_bg->setPosition({4.f, 4.f});
        this->addChild(m_bg, 0);

        auto makeLabel = [&](float y) -> CCLabelBMFont* {
            auto* lbl = CCLabelBMFont::create("", "bigFont.fnt");
            lbl->setScale(0.35f);
            lbl->setAnchorPoint({0.f, 0.5f});
            lbl->setPosition({10.f, y});
            m_bg->addChild(lbl);
            return lbl;
        };

        m_lblGeneration = makeLabel(52.f);
        m_lblAlive      = makeLabel(36.f);
        m_lblFitness    = makeLabel(20.f);

        setVisible(Mod::get()->getSettingValue<bool>("show-hud"));
        return true;
    }

    void update(int gen, int alive, int total, float bestFit) {
        if (!isVisible()) return;
        m_lblGeneration->setString(
            fmt::format("Generation: {}", gen).c_str());
        m_lblAlive->setString(
            fmt::format("Alive: {} / {}", alive, total).c_str());
        m_lblFitness->setString(
            fmt::format("Best Fitness: {:.1f}", bestFit).c_str());
    }

private:
    CCLayerColor*    m_bg             = nullptr;
    CCLabelBMFont*   m_lblGeneration  = nullptr;
    CCLabelBMFont*   m_lblAlive       = nullptr;
    CCLabelBMFont*   m_lblFitness     = nullptr;
};

} // namespace neatmod
