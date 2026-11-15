#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace tumpuk {
constexpr float blockH = 0.46f;
constexpr float baseSize = 3.70f;

enum class Axis { X, Z };
enum class Land { Perfect, Trimmed, Miss };

struct Block {
    float x = 0, z = 0, w = baseSize, d = baseSize, y = 0;
    int tint = 0;
};

struct Fragment {
    Block block;
    float vx = 0, vy = 0, vz = 0;
    float rotation = 0, spin = 0;
    Axis axis = Axis::X;
};

struct Landing {
    Land kind;
    Block keep;
    Block cut;
    float removedFraction;
};

// Only the active axis changes. On the other axis the footprints are identical.
inline Landing intersect(Block support, Block moving, Axis axis) {
    const bool x = axis == Axis::X;
    const float center = x ? support.x : support.z;
    const float edge = x ? moving.x : moving.z;
    const float size = x ? support.w : support.d;
    const float offset = edge - center;
    if (std::abs(offset) <= std::min(0.085f, size * 0.12f)) {
        if (x) moving.x = center;
        else moving.z = center;
        return {Land::Perfect, moving, {}, 0};
    }
    const float lo = std::max(center, edge) - size * 0.5f;
    const float hi = std::min(center, edge) + size * 0.5f;
    if (hi - lo <= 0.0001f) return {Land::Miss, {}, moving, 1};
    const float cutSize = size - (hi - lo);
    const float cutCenter = offset < 0 ? lo - cutSize * 0.5f : hi + cutSize * 0.5f;
    Block kept = moving, cut = moving;
    if (x) {
        kept.x = (lo + hi) * 0.5f; kept.w = hi - lo;
        cut.x = cutCenter; cut.w = cutSize;
    } else {
        kept.z = (lo + hi) * 0.5f; kept.d = hi - lo;
        cut.z = cutCenter; cut.d = cutSize;
    }
    return {Land::Trimmed, kept, cut, cutSize / size};
}

struct Round {
    std::vector<Block> tower;
    std::vector<Fragment> fragments;
    Block active;
    Axis axis = Axis::X;
    int score = 0, combo = 0, longestCombo = 0;
    float heading = 1, camera = 0, notice = 0, cutPercent = 0;
    float resultTime = 0, dropLock = 0;
    bool alive = true;
    Land lastLand = Land::Perfect;

    Round() { reset(); }
    void reset() {
        tower = {Block{}};
        fragments.clear();
        score = combo = longestCombo = 0;
        camera = notice = cutPercent = resultTime = dropLock = 0;
        alive = true;
        lastLand = Land::Perfect;
        spawn();
    }
    float range() const {
        const auto& top = tower.back();
        return std::max(3.70f, (axis == Axis::X ? top.w : top.d) + 0.55f);
    }
    float speed() const { return std::min(6.90f, 2.55f + score * 0.125f); }
    void spawn() {
        axis = (score % 2 == 0) ? Axis::X : Axis::Z;
        active = tower.back();
        active.y = (score + 1) * blockH;
        active.tint = score + 1;
        heading = score % 4 < 2 ? 1.0f : -1.0f;
        (axis == Axis::X ? active.x : active.z) -= heading * range();
    }
    void update(float dt) {
        dt = std::clamp(dt, 0.0f, 0.05f);
        notice = std::max(0.0f, notice - dt);
        dropLock = std::max(0.0f, dropLock - dt);
        if (alive) {
            const auto& top = tower.back();
            const float center = axis == Axis::X ? top.x : top.z;
            const float limit = range();
            float& pos = axis == Axis::X ? active.x : active.z;
            pos += heading * speed() * dt;
            if (pos > center + limit) {
                pos = 2 * (center + limit) - pos; heading = -1;
            } else if (pos < center - limit) {
                pos = 2 * (center - limit) - pos; heading = 1;
            }
        } else resultTime += dt;
        const float target = std::max(0.0f, tower.back().y - 1.38f);
        camera += (target - camera) * (1.0f - std::exp(-8.0f * dt));
        for (auto& f : fragments) {
            f.vy -= 13.0f * dt;
            f.block.x += f.vx * dt;
            f.block.z += f.vz * dt;
            f.block.y += f.vy * dt;
            f.rotation += f.spin * dt;
        }
        fragments.erase(std::remove_if(fragments.begin(), fragments.end(), [this](const Fragment& f) {
            return f.block.y < camera - 7.0f;
        }), fragments.end());
    }
    void fall(Block part, float sign) {
        fragments.push_back({part, axis == Axis::X ? sign * 0.9f : 0.0f, -0.35f,
                             axis == Axis::Z ? sign * 0.9f : 0.0f,
                             0, sign * 110.0f, axis});
    }
    bool drop() {
        if (!alive || dropLock > 0) return false;
        const Block support = tower.back();
        const auto land = intersect(support, active, axis);
        const float offset = axis == Axis::X ? active.x - support.x : active.z - support.z;
        const float sign = offset < 0 ? -1.0f : 1.0f;
        lastLand = land.kind;
        if (land.kind == Land::Miss) {
            fall(land.cut, sign);
            alive = false;
            combo = 0;
            resultTime = 0;
            return true;
        }
        if (land.kind == Land::Trimmed) {
            fall(land.cut, sign);
            combo = 0;
            cutPercent = land.removedFraction * 100;
        } else {
            ++combo;
            longestCombo = std::max(longestCombo, combo);
        }
        tower.push_back(land.keep);
        ++score;
        notice = 0.90f;
        dropLock = 0.15f;
        spawn();
        return true;
    }
};
}
