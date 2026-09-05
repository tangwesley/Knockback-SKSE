#include <Knockback/Tasks.h>

#include <Knockback/Config.h>
#include <Knockback/Filters.h>
#include <Knockback/FrameTick.h>
#include <Knockback/Physics.h>

#include "SKSE/SKSE.h"

namespace logger = SKSE::log;

namespace Knockback
{
    // Runs once, at the next SKSE task drain after the hit event. Anything that needs to
    // span real frames (attacking-target refresh, player push, separation) is handed to
    // the frame hooks from here; SKSE's task queue cannot delay across frames.
    void QueuePhysicsShove(
        RE::ActorHandle aggressorH,
        RE::ActorHandle targetH,
        float weaponMult)
    {
        auto taskIf = SKSE::GetTaskInterface();
        if (!taskIf) {
            logger::trace("Shove (queued): no TaskInterface");
            return;
        }

        taskIf->AddTask([=]() {
            const auto& cfg = GetConfig();

            auto aggressorPtr = aggressorH.get();
            auto targetPtr = targetH.get();

            RE::Actor* aggressor = aggressorPtr ? aggressorPtr.get() : nullptr;
            RE::Actor* target = targetPtr ? targetPtr.get() : nullptr;

            if (!aggressor || !target) return;
            if (aggressor == target) return;
            if (!IsAlive(aggressor) || !IsAlive(target)) return;

            if (ShouldDisableDueToFirstPerson(aggressor)) {
                logger::trace("Shove (queued): suppressed (player in first-person)");
                return;
            }

            if (!IsValidKnockbackTarget(target)) {
                return;
            }

            // INI is authoritative: multiplier <= 0 means no shove
            if (weaponMult <= 0.0f) {
                logger::trace("Shove (queued): suppressed (weapon not configured)");
                return;
            }

            float mag = cfg.shoveMagnitude * weaponMult;
            float dur = cfg.shoveDuration;
            ShapeForApplyCurrent(mag, dur);

            const bool ok = ApplyPhysicsShove(aggressor, target, mag, dur);
            if (!ok) {
                logger::trace("Shove (queued): failed mag={} dur={} mult={}", mag, dur, weaponMult);
                return;
            }

            logger::trace("Shove (queued): applied mag={} dur={} mult={}", mag, dur, weaponMult);

            if (IsPlayer(target)) {
                // The player's rigid-body controller rewrites its velocity every physics
                // step regardless of animation state, so ApplyCurrent barely registers.
                // The per-step substitution is the whole push here.
                if (cfg.playerShoveFrames > 0 && EnsureFrameHooks(target)) {
                    const float playerMag = mag * cfg.playerShoveMultiplier;
                    logger::trace("Shove (queued): player target, substituting velocity for {} frames (mag={}, ease-out)",
                        cfg.playerShoveFrames, playerMag);
                    RegisterVelocityOverride(aggressorH, targetH, playerMag, cfg.playerShoveFrames,
                        /*easeOut*/ true, /*ignoreAnimState*/ true);
                }
            }
            else if (cfg.animDrivenRefreshFrames > 0 && IsAnimDrivenOrAttacking(target)) {
                if (EnsureFrameHooks(target)) {
                    logger::trace("Shove (queued): target animation-driven, refreshing velocity post-Update for {} frames",
                        cfg.animDrivenRefreshFrames);
                    RegisterVelocityOverride(aggressorH, targetH, mag, cfg.animDrivenRefreshFrames);
                }
                else {
                    // No per-frame tick available: one direct write is still better than none.
                    const bool wrote = ApplyControllerVelocity(aggressor, target, mag);
                    logger::trace("Shove (queued): target animation-driven, single direct velocity write ok={}", wrote);
                }
            }

            if (cfg.enforceMinSeparation && cfg.separationRetries > 0 && IsPlayer(aggressor)) {
                if (EnsureFrameHooks(aggressor)) {
                    RegisterSeparationJob(aggressorH, targetH);
                }
                else {
                    logger::trace("Separation: skipped (frame hooks unavailable)");
                }
            }
            });
    }
}
