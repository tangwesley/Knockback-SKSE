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
    // CommonLibSSE NG's TESObjectREFR declaration order disagrees with the vtable the
    // game actually uses: on SE 1.5.97 and AE 1.6.1170 a plain virtual call lands one
    // slot late, so Actor::ApplyCurrent() would silently call the wrong function. The
    // header's documented slot numbers are the ones taken from the real vtable, so every
    // virtual this plugin touches (ApplyCurrent here, Update and SetLinearVelocityImpl
    // in FrameTick) is dispatched by documented slot via REL::RelocateVirtual or a
    // vtable write, never through the compiler-assigned index.
    //
    // An earlier version probed this at runtime by calling IsDead() both ways on a live
    // actor. That probe was unreliable: the mis-indexed call lands on a function that
    // returns a pointer, so its "true" depended on which actor was probed first, and the
    // same machine could get different verdicts on different sessions.

    float HorizontalDistance(RE::Actor* a, RE::Actor* b)
    {
        if (!a || !b) return 0.0f;
        const auto ap = a->GetPosition();
        const auto bp = b->GetPosition();
        const float dx = bp.x - ap.x;
        const float dy = bp.y - ap.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    namespace
    {
        // ApplyCurrent refuses slow currents (observed threshold between 3.5 and 4 m/s).
        // Raise the speed to clear it and shorten the duration by the same ratio so the
        // distance is unchanged, but never below one physics step or the engine drops it.
        constexpr float kApplyCurrentMinVelocity = 4.0f;
        constexpr float kApplyCurrentMinDuration = 1.0f / 60.0f;

        void ShapeForApplyCurrent(float& mag, float& dur)
        {
            if (mag > 0.0f && mag < kApplyCurrentMinVelocity) {
                const float scaled = dur * (mag / kApplyCurrentMinVelocity);
                mag = kApplyCurrentMinVelocity;
                dur = std::max(scaled, kApplyCurrentMinDuration);
            }
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

        ShapeForApplyCurrent(magnitude, duration);

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
        const float velocityTimeBefore = cc->velocityTime;

        const bool returned =
            REL::RelocateVirtual<decltype(&RE::Actor::ApplyCurrent)>(0x9D, 0x9E, target, duration, vel);

        // Independent check that the slot we called really is ApplyCurrent: it writes
        // the controller's velocityMod / velocityTime. If those now hold what we passed,
        // the push landed regardless of the return value. If they do not and the call
        // returned false, the slot is wrong on this runtime and nothing else that relies
        // on documented slot numbers (the frame hooks) should be trusted either.
        const float velocityTimeAfter = cc->velocityTime;
        const float modX = cc->velocityMod.quad.m128_f32[0];
        const float modY = cc->velocityMod.quad.m128_f32[1];
        const bool fieldsUpdated =
            std::fabs(velocityTimeAfter - duration) < 1e-3f &&
            std::fabs(modX - vel.quad.m128_f32[0]) < 1e-3f &&
            std::fabs(modY - vel.quad.m128_f32[1]) < 1e-3f;

        const bool ok = returned || fieldsUpdated;

        if (!returned) {
            logger::trace("ApplyPhysicsShove: ApplyCurrent returned false; velocityTime {} -> {} (expected {}), velocityMod=({}, {}) fieldsUpdated={} runtime={}",
                velocityTimeBefore, velocityTimeAfter, duration, modX, modY, fieldsUpdated,
                REL::Module::get().version().string());
        }
        else {
            logger::trace("ApplyPhysicsShove: ApplyCurrent -> true (fieldsUpdated={}, velocityTime before={})",
                fieldsUpdated, velocityTimeBefore);
        }
        return ok;
    }


    bool ApplyControllerVelocity(RE::Actor* aggressor, RE::Actor* target, float magnitude)
    {
        if (!aggressor || !target || aggressor == target) {
            return false;
        }
        if (!IsAlive(target) || !IsAlive(aggressor)) {
            return false;
        }
        if (!target->Is3DLoaded() || !target->Get3D()) {
            return false;
        }

        auto* cc = target->GetCharController();
        if (!cc) {
            logger::trace("ApplyControllerVelocity: no char controller {:08X}", target->GetFormID());
            return false;
        }

        const auto aPos = aggressor->GetPosition();
        const auto tPos = target->GetPosition();

        float dx = tPos.x - aPos.x;
        float dy = tPos.y - aPos.y;
        const float lenSq = dx * dx + dy * dy;
        if (lenSq < 1e-6f) {
            return false;
        }
        const float invLen = 1.0f / std::sqrt(lenSq);
        dx *= invLen;
        dy *= invLen;

        // Keep whatever vertical velocity the controller has (gravity, steps) and
        // replace only the horizontal component with the shove.
        RE::hkVector4 cur{};
        cc->GetLinearVelocityImpl(cur);
        const float vz = cur.quad.m128_f32[2];

        RE::hkVector4 vel{};
        vel.quad = _mm_setr_ps(dx * magnitude, dy * magnitude, vz, 0.0f);
        cc->SetLinearVelocityImpl(vel);

        // Read-back of the previous frame's velocity tells whether the last write survived.
        logger::trace("ApplyControllerVelocity: was=({}, {}, {}) set=({}, {}, {}) mag={} on target {:08X}",
            cur.quad.m128_f32[0], cur.quad.m128_f32[1], vz,
            vel.quad.m128_f32[0], vel.quad.m128_f32[1], vz, magnitude, target->GetFormID());
        return true;
    }

    bool ApplyVelocityAwayFrom(RE::Actor* from, RE::Actor* who, float magnitude, float duration)
    {
        return ApplyPhysicsShove(from, who, magnitude, duration);
    }
}
