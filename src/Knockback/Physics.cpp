#include <Knockback/Physics.h>
#include <Knockback/Config.h>

#include "SKSE/SKSE.h"
#include <xmmintrin.h>
#include <cmath>
#include <algorithm>

namespace logger = SKSE::log;

namespace Knockback
{
    float HorizontalDistance(RE::Actor* a, RE::Actor* b)
    {
        if (!a || !b) return 0.0f;
        const auto ap = a->GetPosition();
        const auto bp = b->GetPosition();
        const float dx = bp.x - ap.x;
        const float dy = bp.y - ap.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    void ShapeForApplyCurrent(float& mag, float& dur)
    {
        const auto& cfg = GetConfig();

        if (cfg.applyCurrentMinVelocity > 0.0f && mag > 0.0f) {
            const float peak = std::max(mag, cfg.applyCurrentMinVelocity);
            const float scaled = dur * (mag / peak);
            const float minDur = dur * cfg.minDurationScale;
            mag = peak;
            dur = std::max(scaled, minDur);
        }
    }

    bool ApplyPhysicsShove(RE::Actor* aggressor, RE::Actor* target, float magnitude, float duration)
    {
        if (!aggressor || !target) {
            logger::trace("ApplyPhysicsShove: null aggressor/target");
            return false;
        }

        const auto aPos = aggressor->GetPosition();
        const auto tPos = target->GetPosition();

        float dx = tPos.x - aPos.x;
        float dy = tPos.y - aPos.y;
        float dz = tPos.z - aPos.z;

        dz = 0.0f;

        const float lenSq = dx * dx + dy * dy + dz * dz;
        if (lenSq < 1e-6f) {
            logger::trace("ApplyPhysicsShove: degenerate dir (aPos=({},{}), tPos=({},{}), lenSq={})",
                aPos.x, aPos.y, tPos.x, tPos.y, lenSq);
            return false;
        }

        const float invLen = 1.0f / std::sqrt(lenSq);
        dx *= invLen;
        dy *= invLen;
        dz *= invLen;

        const float vx = dx * magnitude;
        const float vy = dy * magnitude;
        const float vz = dz * magnitude;

        RE::hkVector4 vel{};
        vel.quad = _mm_setr_ps(vx, vy, vz, 0.0f);

        return target->ApplyCurrent(duration, vel);
    }

    bool ApplyVelocityAwayFrom(RE::Actor* from, RE::Actor* who, float magnitude, float duration)
    {
        return ApplyPhysicsShove(from, who, magnitude, duration);
    }
}
