#include <Knockback/FrameTick.h>

#include <Knockback/Config.h>
#include <Knockback/Filters.h>
#include <Knockback/Physics.h>

#include "SKSE/SKSE.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <vector>
#include <xmmintrin.h>

namespace logger = SKSE::log;

namespace Knockback
{
    namespace
    {
        struct VelocityOverride
        {
            RE::ActorHandle aggressor;
            RE::ActorHandle target;
            float magnitude{ 0.0f };
            std::int32_t framesLeft{ 0 };
            std::int32_t totalFrames{ 0 };
            bool easeOut{ false };
            bool ignoreAnimState{ false };

            // Refreshed each frame from the Update hook; consumed by the velocity hook.
            RE::bhkCharacterController* controller{ nullptr };
            float dirX{ 0.0f };
            float dirY{ 0.0f };
            float currentMagnitude{ 0.0f };

            float MagnitudeForFrame() const
            {
                if (!easeOut || totalFrames <= 0) {
                    return magnitude;
                }
                const float t = static_cast<float>(framesLeft) / static_cast<float>(totalFrames);
                return magnitude * (t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t));
            }
        };

        struct SeparationJob
        {
            RE::ActorHandle player;
            RE::ActorHandle target;
            std::int32_t delayFrames{ 0 };
            std::int32_t retriesLeft{ 0 };
            std::int32_t pushFramesLeft{ 0 };
            float lastDist{ -1.0f };
            std::int32_t noProgressCount{ 0 };
        };

        // Recursive: ApplyControllerVelocity (called under the lock) goes through the
        // hooked SetLinearVelocityImpl, which takes the lock again.
        std::recursive_mutex g_mutex;
        std::vector<VelocityOverride> g_overrides;
        std::atomic<std::size_t> g_overrideCount{ 0 };
        std::vector<SeparationJob> g_separation;
        std::atomic<std::size_t> g_separationCount{ 0 };
        std::atomic<std::uint64_t> g_frame{ 0 };

        // Game units per Havok metre. Distances from GetPosition() are game units; the
        // controller velocities are Havok metres per second.
        constexpr float kHavokWorldScale = 0.0142875f;

        enum : int { kHooksUnattempted = 0, kHooksInstalled = 1, kHooksUnavailable = -1 };
        std::atomic<int> g_hookState{ kHooksUnattempted };

        // Actor::Update, documented slot for SE/AE. VR is not built.
        constexpr std::size_t kUpdateSlot = 0xAD;
        // bhkCharacterController::SetLinearVelocityImpl, documented slot 07.
        constexpr std::size_t kSetLinearVelocitySlot = 0x07;

        using UpdateFn = void (*)(RE::Actor*, float);
        using SetLinearVelocityFn = void (*)(RE::bhkCharacterController*, const RE::hkVector4&);

        UpdateFn g_origCharacterUpdate{ nullptr };
        UpdateFn g_origPlayerUpdate{ nullptr };
        SetLinearVelocityFn g_origProxySetLinearVelocity{ nullptr };
        SetLinearVelocityFn g_origRigidBodySetLinearVelocity{ nullptr };

        bool RefreshDirection(VelocityOverride& ov, RE::Actor* aggressor, RE::Actor* target)
        {
            const auto aPos = aggressor->GetPosition();
            const auto tPos = target->GetPosition();
            float dx = tPos.x - aPos.x;
            float dy = tPos.y - aPos.y;
            const float lenSq = dx * dx + dy * dy;
            if (lenSq < 1e-6f) {
                return false;
            }
            const float invLen = 1.0f / std::sqrt(lenSq);
            ov.dirX = dx * invLen;
            ov.dirY = dy * invLen;
            ov.controller = target->GetCharController();
            return ov.controller != nullptr;
        }

        void ServiceOverride(RE::Actor* a_actor)
        {
            if (!a_actor || g_overrideCount.load(std::memory_order_relaxed) == 0) {
                return;
            }

            std::lock_guard lock(g_mutex);

            for (auto it = g_overrides.begin(); it != g_overrides.end(); ++it) {
                auto tPtr = it->target.get();
                if (!tPtr || tPtr.get() != a_actor) {
                    continue;
                }

                auto aPtr = it->aggressor.get();
                RE::Actor* aggressor = aPtr ? aPtr.get() : nullptr;

                // An entry at 0 has already had its last write; it stayed alive only so
                // the velocity hook could cover the physics step after that write.
                bool keep = false;
                if (it->framesLeft > 0 &&
                    aggressor && aggressor != a_actor &&
                    IsAlive(aggressor) && IsAlive(a_actor) &&
                    IsValidKnockbackTarget(a_actor)) {
                    if (!it->ignoreAnimState && !IsAnimDrivenOrAttacking(a_actor)) {
                        // Root motion let go; the ApplyCurrent velocity takes over on its own.
                        logger::trace("VelocityRefresh[frame {}]: target {:08X} no longer animation-driven, stopping ({} frames left)",
                            g_frame.load(std::memory_order_relaxed), a_actor->GetFormID(), it->framesLeft);
                    }
                    else if (RefreshDirection(*it, aggressor, a_actor)) {
                        it->currentMagnitude = it->MagnitudeForFrame();
                        const bool ok = ApplyControllerVelocity(aggressor, a_actor, it->currentMagnitude);
                        it->framesLeft -= 1;
                        logger::trace("VelocityRefresh[frame {}]: ok={} mag={} framesLeft={} target={:08X}",
                            g_frame.load(std::memory_order_relaxed), ok, it->currentMagnitude, it->framesLeft, a_actor->GetFormID());
                        keep = true;
                    }
                }

                if (!keep) {
                    g_overrides.erase(it);
                    g_overrideCount.store(g_overrides.size(), std::memory_order_relaxed);
                }
                return;
            }
        }

        // Called from the engine's own controller update. Replace the horizontal part of
        // whatever it is about to integrate with the shove, keep its vertical part.
        RE::hkVector4 SubstituteVelocity(RE::bhkCharacterController* a_cc, const RE::hkVector4& a_vel)
        {
            if (!a_cc || g_overrideCount.load(std::memory_order_relaxed) == 0) {
                return a_vel;
            }

            std::lock_guard lock(g_mutex);

            for (const auto& ov : g_overrides) {
                if (ov.controller != a_cc) {
                    continue;
                }

                const float inX = a_vel.quad.m128_f32[0];
                const float inY = a_vel.quad.m128_f32[1];
                const float inZ = a_vel.quad.m128_f32[2];
                const float inW = a_vel.quad.m128_f32[3];

                RE::hkVector4 out{};
                out.quad = _mm_setr_ps(ov.dirX * ov.currentMagnitude, ov.dirY * ov.currentMagnitude, inZ, inW);

                auto tPtr = ov.target.get();
                logger::trace("VelocityHook[frame {}]: engine wrote ({}, {}, {}) for target {:08X}; substituted ({}, {})",
                    g_frame.load(std::memory_order_relaxed), inX, inY, inZ,
                    tPtr ? tPtr->GetFormID() : 0,
                    out.quad.m128_f32[0], out.quad.m128_f32[1]);
                return out;
            }

            return a_vel;
        }

        void ServiceSeparation(RE::Actor* a_player, float a_delta)
        {
            if (!a_player || g_separationCount.load(std::memory_order_relaxed) == 0) {
                return;
            }

            std::lock_guard lock(g_mutex);
            const auto& cfg = GetConfig();
            const auto frame = g_frame.load(std::memory_order_relaxed);

            // Actor::Update's delta is the frame time in seconds; guard against oddities.
            const float dt = (a_delta > 1e-4f && a_delta < 0.5f) ? a_delta : (1.0f / 60.0f);

            for (std::size_t i = 0; i < g_separation.size();) {
                auto& job = g_separation[i];
                bool keep = false;

                auto pPtr = job.player.get();
                auto tPtr = job.target.get();
                RE::Actor* player = pPtr ? pPtr.get() : nullptr;
                RE::Actor* target = tPtr ? tPtr.get() : nullptr;

                if (player && player == a_player && target && target != player &&
                    cfg.enforceMinSeparation && cfg.minSeparationDistance > 0.0f &&
                    IsAlive(player) && IsAlive(target) &&
                    !ShouldDisableDueToFirstPerson(player) &&
                    IsValidKnockbackTarget(target)) {
                    if (job.pushFramesLeft > 0) {
                        // A push is in flight; the velocity override is doing the work.
                        job.pushFramesLeft -= 1;
                        keep = true;
                    }
                    else if (job.delayFrames > 0) {
                        job.delayFrames -= 1;
                        keep = true;
                    }
                    else {
                        const float dist = HorizontalDistance(player, target);
                        const float minDist = cfg.minSeparationDistance;

                        bool stalled = false;
                        if (job.lastDist >= 0.0f) {
                            if (std::fabs(dist - job.lastDist) < 1.0f) {
                                job.noProgressCount += 1;
                            }
                            else {
                                job.noProgressCount = 0;
                            }
                            stalled = job.noProgressCount >= 2;
                        }
                        job.lastDist = dist;

                        if (dist >= minDist) {
                            logger::trace("Separation[frame {}]: ok dist={} (min={})", frame, dist, minDist);
                        }
                        else if (stalled) {
                            logger::trace("Separation[frame {}]: no progress (dist={}) -> stop", frame, dist);
                        }
                        else if (job.retriesLeft <= 0) {
                            logger::trace("Separation[frame {}]: out of retries (dist={}, min={})", frame, dist, minDist);
                        }
                        else {
                            const float deficitUnits = minDist - dist;
                            const float deficitMetres = deficitUnits * kHavokWorldScale;
                            const float dur = std::max(cfg.separationPushDuration, 1e-3f);

                            float mag = deficitMetres / dur;
                            if (cfg.separationMaxVelocity > 0.0f) {
                                mag = std::min(mag, cfg.separationMaxVelocity);
                            }

                            const auto frames = std::max<std::int32_t>(1, static_cast<std::int32_t>(std::ceil(dur / dt)));

                            // Override on the player, direction target -> player (away from target).
                            // Constant velocity so distance covered is exactly mag * dur.
                            RegisterVelocityOverride(job.target, job.player, mag, frames,
                                /*easeOut*/ false, /*ignoreAnimState*/ true);

                            job.pushFramesLeft = frames;
                            job.retriesLeft -= 1;
                            job.delayFrames = std::max<std::int32_t>(0, cfg.separationRetryDelayFrames);

                            logger::trace("Separation[frame {}]: dist={} deficit={} -> push player mag={} for {} frames (dt={}), retriesLeft={}",
                                frame, dist, deficitUnits, mag, frames, dt, job.retriesLeft);
                            keep = true;
                        }
                    }
                }

                if (keep) {
                    ++i;
                }
                else {
                    g_separation.erase(g_separation.begin() + static_cast<std::ptrdiff_t>(i));
                    g_separationCount.store(g_separation.size(), std::memory_order_relaxed);
                }
            }
        }

        void CharacterUpdateHook(RE::Actor* a_this, float a_delta)
        {
            g_origCharacterUpdate(a_this, a_delta);
            ServiceOverride(a_this);
        }

        void PlayerUpdateHook(RE::Actor* a_this, float a_delta)
        {
            g_frame.fetch_add(1, std::memory_order_relaxed);
            g_origPlayerUpdate(a_this, a_delta);
            // Separation may register an override on the player; service it in the same frame.
            ServiceSeparation(a_this, a_delta);
            ServiceOverride(a_this);
        }

        void ProxySetLinearVelocityHook(RE::bhkCharacterController* a_this, const RE::hkVector4& a_vel)
        {
            const RE::hkVector4 v = SubstituteVelocity(a_this, a_vel);
            g_origProxySetLinearVelocity(a_this, v);
        }

        void RigidBodySetLinearVelocityHook(RE::bhkCharacterController* a_this, const RE::hkVector4& a_vel)
        {
            const RE::hkVector4 v = SubstituteVelocity(a_this, a_vel);
            g_origRigidBodySetLinearVelocity(a_this, v);
        }
    }

    bool FrameHooksActive()
    {
        return g_hookState.load(std::memory_order_relaxed) == kHooksInstalled;
    }

    std::uint64_t FrameIndex()
    {
        return g_frame.load(std::memory_order_relaxed);
    }

    bool EnsureFrameHooks(RE::Actor* a_liveActor)
    {
        const int state = g_hookState.load(std::memory_order_acquire);
        if (state != kHooksUnattempted) {
            return state == kHooksInstalled;
        }
        if (!a_liveActor) {
            return false;
        }

        // The Update slot number comes from the same header comments the ApplyCurrent
        // workaround relies on. Only trust it when the probe says those comments match
        // the runtime's real vtable.
        if (!UseDocumentedVtableSlots(a_liveActor)) {
            logger::warn("Frame hooks not installed: runtime vtable matches compiler indices, so the documented Actor::Update slot is unverified. Per-frame velocity refresh disabled.");
            g_hookState.store(kHooksUnavailable, std::memory_order_release);
            return false;
        }

        REL::Relocation<std::uintptr_t> characterVtbl{ RE::VTABLE_Character[0] };
        REL::Relocation<std::uintptr_t> playerVtbl{ RE::VTABLE_PlayerCharacter[0] };

        g_origCharacterUpdate = reinterpret_cast<UpdateFn>(characterVtbl.write_vfunc(kUpdateSlot, CharacterUpdateHook));
        g_origPlayerUpdate = reinterpret_cast<UpdateFn>(playerVtbl.write_vfunc(kUpdateSlot, PlayerUpdateHook));

        // bhkCharProxyController lists hkpCharacterProxyListener first, so its
        // bhkCharacterController vtable is the second one. bhkCharRigidBodyController
        // (the player's) lists bhkCharacterController first.
        REL::Relocation<std::uintptr_t> proxyVtbl{ RE::VTABLE_bhkCharProxyController[1] };
        REL::Relocation<std::uintptr_t> rigidBodyVtbl{ RE::VTABLE_bhkCharRigidBodyController[0] };

        g_origProxySetLinearVelocity = reinterpret_cast<SetLinearVelocityFn>(
            proxyVtbl.write_vfunc(kSetLinearVelocitySlot, ProxySetLinearVelocityHook));
        g_origRigidBodySetLinearVelocity = reinterpret_cast<SetLinearVelocityFn>(
            rigidBodyVtbl.write_vfunc(kSetLinearVelocitySlot, RigidBodySetLinearVelocityHook));

        g_hookState.store(kHooksInstalled, std::memory_order_release);
        logger::info("Installed frame hooks: Character/PlayerCharacter::Update (slot 0x{:X}), bhkCharProxyController/bhkCharRigidBodyController::SetLinearVelocityImpl (slot 0x{:X})",
            kUpdateSlot, kSetLinearVelocitySlot);
        return true;
    }

    void RegisterSeparationJob(RE::ActorHandle playerH, RE::ActorHandle targetH)
    {
        const auto& cfg = GetConfig();
        if (!cfg.enforceMinSeparation || cfg.minSeparationDistance <= 0.0f || cfg.separationRetries <= 0) {
            return;
        }

        std::lock_guard lock(g_mutex);

        // One job per target: a fresh hit restarts the sequence.
        for (auto& job : g_separation) {
            if (job.target == targetH && job.player == playerH) {
                job.delayFrames = std::max<std::int32_t>(0, cfg.separationInitialDelayFrames);
                job.retriesLeft = cfg.separationRetries;
                job.pushFramesLeft = 0;
                job.lastDist = -1.0f;
                job.noProgressCount = 0;
                return;
            }
        }

        SeparationJob job{};
        job.player = playerH;
        job.target = targetH;
        job.delayFrames = std::max<std::int32_t>(0, cfg.separationInitialDelayFrames);
        job.retriesLeft = cfg.separationRetries;
        g_separation.push_back(job);
        g_separationCount.store(g_separation.size(), std::memory_order_relaxed);
    }

    void RegisterVelocityOverride(
        RE::ActorHandle aggressorH,
        RE::ActorHandle targetH,
        float magnitude,
        std::int32_t frames,
        bool easeOut,
        bool ignoreAnimState)
    {
        if (frames <= 0 || magnitude <= 0.0f) {
            return;
        }

        auto aPtr = aggressorH.get();
        auto tPtr = targetH.get();
        RE::Actor* aggressor = aPtr ? aPtr.get() : nullptr;
        RE::Actor* target = tPtr ? tPtr.get() : nullptr;
        if (!aggressor || !target) {
            return;
        }

        std::lock_guard lock(g_mutex);

        // One override per target: a fresh hit restarts the window rather than stacking.
        for (auto& ov : g_overrides) {
            if (ov.target == targetH) {
                ov.aggressor = aggressorH;
                ov.magnitude = magnitude;
                ov.framesLeft = frames;
                ov.totalFrames = frames;
                ov.easeOut = easeOut;
                ov.ignoreAnimState = ignoreAnimState;
                ov.currentMagnitude = magnitude;
                RefreshDirection(ov, aggressor, target);
                return;
            }
        }

        VelocityOverride ov{};
        ov.aggressor = aggressorH;
        ov.target = targetH;
        ov.magnitude = magnitude;
        ov.framesLeft = frames;
        ov.totalFrames = frames;
        ov.easeOut = easeOut;
        ov.ignoreAnimState = ignoreAnimState;
        ov.currentMagnitude = magnitude;
        RefreshDirection(ov, aggressor, target);
        g_overrides.push_back(ov);
        g_overrideCount.store(g_overrides.size(), std::memory_order_relaxed);
    }
}
