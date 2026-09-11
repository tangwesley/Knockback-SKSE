#include <Knockback/Tasks.h>

#include <Knockback/Config.h>
#include <Knockback/Filters.h>
#include <Knockback/FrameTick.h>
#include <Knockback/Physics.h>

#include "SKSE/SKSE.h"
#include <algorithm>

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

            const float mag = cfg.shoveMagnitude * weaponMult;
            const float dur = cfg.shoveDuration;

            // ApplyCurrent is best-effort. It refuses (returns false, writes nothing) when
            // the controller already has a current active, which some load orders keep
            // permanently true for every actor. The frame hooks do not depend on it.
            const bool applied = ApplyPhysicsShove(aggressor, target, mag, dur);
            const bool hooks = EnsureFrameHooks();

            logger::trace("Shove (queued): mag={} dur={} mult={} applyCurrent={} frameHooks={}",
                mag, dur, weaponMult, applied, hooks);

            if (!hooks) {
                if (!applied) {
                    // Nothing else can move the target. One direct write beats none.
                    const bool wrote = ApplyControllerVelocity(aggressor, target, mag);
                    logger::trace("Shove (queued): no hooks and ApplyCurrent refused; single direct velocity write ok={}", wrote);
                }
                return;
            }

            if (IsPlayer(target)) {
                // The player's rigid-body controller rewrites its velocity every physics
                // step regardless of animation state, so ApplyCurrent barely registers.
                // The per-step substitution is the whole push here.
                if (cfg.playerShoveDuration > 0.0f) {
                    const float playerMag = mag * cfg.playerShoveMultiplier;
                    logger::trace("Shove (queued): player target, substituting velocity for {} s (mag={}, ease-out)",
                        cfg.playerShoveDuration, playerMag);
                    RegisterVelocityOverride(aggressorH, targetH, playerMag, cfg.playerShoveDuration,
                        /*easeOut*/ true, /*ignoreAnimState*/ true);
                }
            }
            else {
                // Constant push for the shove duration in real time. An attacking target
                // gets at least AttackingTargetMinDuration, since root motion would
                // otherwise swallow a short window.
                const bool animDriven = IsAnimDrivenOrAttacking(target);
                const float seconds = animDriven ? std::max(dur, cfg.attackingTargetMinDuration) : dur;
                logger::trace("Shove (queued): npc target, substituting velocity for {} s (animDriven={})",
                    seconds, animDriven);
                RegisterVelocityOverride(aggressorH, targetH, mag, seconds,
                    /*easeOut*/ false, /*ignoreAnimState*/ true);
            }

            if (cfg.enforceMinSeparation && cfg.separationRetries > 0 && IsPlayer(aggressor)) {
                RegisterSeparationJob(aggressorH, targetH);
            }
            });
    }
}
