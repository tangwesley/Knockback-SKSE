#include <Knockback/Physics.h>
#include <Knockback/Config.h>
#include <Knockback/Filters.h>

#include "SKSE/SKSE.h"
#include <atomic>
#include <xmmintrin.h>
#include <cmath>
#include <algorithm>

namespace logger = SKSE::log;

namespace Knockback
{
    namespace
    {
        // CommonLibSSE NG's TESObjectREFR declaration order disagrees with the vtable
        // the game actually uses: on SE 1.5.97 and AE 1.6.1170 a plain virtual call
        // lands one slot late, so Actor::IsDead() reports true for live actors and
        // Actor::ApplyCurrent() silently does nothing. The header's documented slots
        // (0x99 / 0x9D) are correct on both, but hardcoding them unconditionally would
        // break on any runtime where the declarations do line up.
        //
        // So decide at runtime: dispatch IsDead() both ways against an actor already
        // known to be alive. Disagreement means the compiler-assigned index is wrong.
        // Only then fall back to the documented slot numbers.
        bool UseDocumentedVtableSlots(RE::Actor* a_liveActor)
        {
            enum : int { kUnknown = -1, kCompilerIndices = 0, kDocumentedSlots = 1 };
            static std::atomic<int> cached{ kUnknown };

            int mode = cached.load(std::memory_order_relaxed);
            if (mode == kUnknown) {
                const bool deadViaCompiler = a_liveActor->IsDead();
                const bool deadViaSlot =
                    REL::RelocateVirtual<decltype(&RE::Actor::IsDead)>(0x99, 0x9A, a_liveActor, true);

                mode = (deadViaCompiler && !deadViaSlot) ? kDocumentedSlots : kCompilerIndices;
                cached.store(mode, std::memory_order_relaxed);

                logger::info("Vtable dispatch: {} (live actor IsDead: compiler={}, slot 0x99={})",
                    mode == kDocumentedSlots ? "documented slots" : "compiler indices",
                    deadViaCompiler, deadViaSlot);
            }

            return mode == kDocumentedSlots;
        }
    }

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

        if (aggressor == target) {
            return false;
        }

        // Basic lifecycle sanity (cheap + prevents weird edge cases)
        if (!IsAlive(target) || !IsAlive(aggressor)) {
            return false;
        }

        // Physics/3D validity gates (avoid ApplyCurrent crash paths)
        if (!target->Is3DLoaded()) {
            logger::trace("ApplyPhysicsShove: target not 3D loaded {:08X}", target->GetFormID());
            return false;
        }

        auto* node = target->Get3D();
        if (!node) {
            logger::trace("ApplyPhysicsShove: target has no 3D {:08X}", target->GetFormID());
            return false;
        }

        // controller gate. 
        auto* cc = target->GetCharController();
        if (!cc) {
            logger::trace("ApplyPhysicsShove: no char controller {:08X}", target->GetFormID());
            return false;
        }

        // Direction from aggressor -> target
        const auto aPos = aggressor->GetPosition();
        const auto tPos = target->GetPosition();

        float dx = tPos.x - aPos.x;
        float dy = tPos.y - aPos.y;
        float dz = 0.0f;  // flatten vertical

        const float lenSq = dx * dx + dy * dy;
        if (lenSq < 1e-6f) {
            logger::trace("ApplyPhysicsShove: degenerate dir (aPos=({},{}), tPos=({},{}), lenSq={})",
                aPos.x, aPos.y, tPos.x, tPos.y, lenSq);
            return false;
        }

        const float invLen = 1.0f / std::sqrt(lenSq);
        dx *= invLen;
        dy *= invLen;

        RE::hkVector4 vel{};
        vel.quad = _mm_setr_ps(dx * magnitude, dy * magnitude, dz, 0.0f);

        logger::trace("ApplyPhysicsShove: applying vel=({}, {}, {}) mag={} dur={} to target {:08X}",
            vel.quad.m128_f32[0],
            vel.quad.m128_f32[1],
            vel.quad.m128_f32[2],
            magnitude,
            duration,
			target->GetFormID());
        // target is known alive here, so the dispatch probe is meaningful.
        const bool ok = UseDocumentedVtableSlots(target)
            ? REL::RelocateVirtual<decltype(&RE::Actor::ApplyCurrent)>(0x9D, 0x9E, target, duration, vel)
            : target->ApplyCurrent(duration, vel);

        logger::trace("ApplyPhysicsShove: ApplyCurrent -> {}", ok);
        return ok;
    }


    bool ApplyVelocityAwayFrom(RE::Actor* from, RE::Actor* who, float magnitude, float duration)
    {
        return ApplyPhysicsShove(from, who, magnitude, duration);
    }
}
