#include <Knockback/Tasks.h>

#include <Knockback/Config.h>
#include <Knockback/Filters.h>
#include <Knockback/Physics.h>

#include "SKSE/SKSE.h"
#include <algorithm>
#include <cmath>

namespace logger = SKSE::log;

namespace Knockback
{
    static void QueueShoveEffectivenessCheck(
        RE::ActorHandle aggressorH,
        RE::ActorHandle targetH,
        std::int32_t remainingTries,
        float distBefore,
        std::int32_t delayFrames,
        float weaponMult)
    {
        auto taskIf = SKSE::GetTaskInterface();
        if (!taskIf) {
            return;
        }

        taskIf->AddTask([=]() {
            if (delayFrames > 0) {
                QueueShoveEffectivenessCheck(
                    aggressorH, targetH, remainingTries, distBefore, delayFrames - 1, weaponMult);
                return;
            }

            auto aggressorPtr = aggressorH.get();
            auto targetPtr = targetH.get();
            RE::Actor* aggressor = aggressorPtr ? aggressorPtr.get() : nullptr;
            RE::Actor* target = targetPtr ? targetPtr.get() : nullptr;

            if (!aggressor || !target) return;
            if (aggressor == target) return;
            if (aggressor->IsDead() || target->IsDead()) return;

            if (ShouldDisableDueToFirstPerson(aggressor)) return;
            if (!IsHumanoidAllowed(target)) return;

            if (weaponMult <= 0.0f) {
                return;
            }

            const auto& cfg = GetConfig();

            const float distAfter = HorizontalDistance(aggressor, target);
            const float gained = distAfter - distBefore;

            if (gained >= cfg.minShoveSeparationDelta) {
                logger::trace(
                    "ShoveEffect: ok before={} after={} gained={}",
                    distBefore, distAfter, gained);
                return;
            }

            const auto nextTries = remainingTries - 1;
            if (nextTries <= 0) {
                return;
            }

            float mag = cfg.shoveMagnitude * weaponMult;
            float dur = cfg.shoveDuration;
            ShapeForApplyCurrent(mag, dur);

            const bool ok = ApplyPhysicsShove(aggressor, target, mag, dur);
            logger::trace(
                "ShoveEffect: reapply ok={} mag={} dur={} mult={}",
                ok, mag, dur, weaponMult);

            QueueShoveEffectivenessCheck(
                aggressorH,
                targetH,
                nextTries,
                distAfter,
                std::max(1, cfg.shoveRetryDelayFrames),
                weaponMult);
            });
    }


    void QueueEnforceMinSeparation(RE::ActorHandle aggressorH,
        RE::ActorHandle targetH,
        std::int32_t remainingTries,
        std::int32_t delayFrames,
        float lastDist,
        std::int32_t noProgressCount)
    {
        const auto& cfg = GetConfig();

        if (!cfg.enforceMinSeparation || cfg.minSeparationDistance <= 0.0f || remainingTries <= 0) {
            return;
        }

        auto taskIf = SKSE::GetTaskInterface();
        if (!taskIf) {
            logger::trace("Separation: no TaskInterface");
            return;
        }

        taskIf->AddTask([aggressorH, targetH, remainingTries, delayFrames, lastDist, noProgressCount]() mutable {
            const auto& cfg = GetConfig();

            if (delayFrames > 0) {
                QueueEnforceMinSeparation(aggressorH, targetH, remainingTries, delayFrames - 1, lastDist, noProgressCount);
                return;
            }

            auto aggressorPtr = aggressorH.get();
            auto targetPtr = targetH.get();
            RE::Actor* aggressor = aggressorPtr ? aggressorPtr.get() : nullptr;
            RE::Actor* target = targetPtr ? targetPtr.get() : nullptr;

            if (!aggressor || !target) return;
            if (aggressor == target) return;
            if (aggressor->IsDead() || target->IsDead()) return;

            // Separation is only for player aggressor
            if (!IsPlayer(aggressor)) {
                return;
            }

            if (ShouldDisableDueToFirstPerson(aggressor)) return;
            if (!IsHumanoidAllowed(target)) return;

            const float dist = HorizontalDistance(aggressor, target);
            const float minDist = cfg.minSeparationDistance;

            if (lastDist >= 0.0f) {
                const float delta = std::fabs(dist - lastDist);

                if (delta < 1.0f) {
                    noProgressCount++;
                }
                else {
                    noProgressCount = 0;
                }

                if (noProgressCount >= 2) {
                    logger::trace("Separation: no progress (dist={} lastDist={} delta={}) -> stop",
                        dist, lastDist, delta);
                    return;
                }
            }

            if (dist >= minDist) {
                logger::trace("Separation: ok dist={} (min={})", dist, minDist);
                return;
            }

            const float deficit = (minDist - dist);

            float dur = cfg.separationPushDuration;
            float mag = (dur > 1e-4f) ? (deficit / dur) : cfg.separationMaxVelocity;

            if (cfg.separationMaxVelocity > 0.0f) {
                mag = std::min(mag, cfg.separationMaxVelocity);
            }

            ShapeForApplyCurrent(mag, dur);

            const bool ok = ApplyVelocityAwayFrom(/*from=*/target, /*who=*/aggressor, mag, dur);

            logger::trace("Separation: dist={} deficit={} -> pushAggressor mag={} dur={} ok={} triesLeftAfter={}",
                dist, deficit, mag, dur, ok, remainingTries - 1);

            const auto nextTries = remainingTries - 1;
            if (nextTries > 0) {
                QueueEnforceMinSeparation(aggressorH, targetH, nextTries, cfg.separationRetryDelayFrames, dist, noProgressCount);
            }
            });
    }

    void QueuePhysicsShove(
        RE::ActorHandle aggressorH,
        RE::ActorHandle targetH,
        std::int32_t remainingTries,
        std::int32_t delayFrames,
        float weaponMult)
    {
        auto taskIf = SKSE::GetTaskInterface();
        if (!taskIf) {
            logger::trace("Shove (queued): no TaskInterface");
            return;
        }

        taskIf->AddTask([=]() {
            const auto& cfg = GetConfig();

            if (delayFrames > 0) {
                QueuePhysicsShove(aggressorH, targetH, remainingTries, delayFrames - 1, weaponMult);
                return;
            }

            auto aggressorPtr = aggressorH.get();
            auto targetPtr = targetH.get();

            RE::Actor* aggressor = aggressorPtr ? aggressorPtr.get() : nullptr;
            RE::Actor* target = targetPtr ? targetPtr.get() : nullptr;

            if (!aggressor || !target) return;
            if (aggressor == target) return;
            if (aggressor->IsDead() || target->IsDead()) return;

            if (ShouldDisableDueToFirstPerson(aggressor)) {
                logger::trace("Shove (queued): suppressed (player in first-person)");
                return;
            }

            if (!IsHumanoidAllowed(target)) {
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

            const float distBefore = HorizontalDistance(aggressor, target);
            const bool ok = ApplyPhysicsShove(aggressor, target, mag, dur);

            if (ok) {
                logger::trace(
                    "Shove (queued): applied mag={} dur={} mult={} triesLeftAfter={}",
                    mag, dur, weaponMult, remainingTries - 1);

                if (cfg.minShoveSeparationDelta > 0.0f) {
                    QueueShoveEffectivenessCheck(
                        aggressorH,
                        targetH,
                        remainingTries,
                        distBefore,
                        /*delayFrames*/ 1,
                        weaponMult);
                }

                if (cfg.enforceMinSeparation && cfg.separationRetries > 0 && IsPlayer(aggressor)) {
                    QueueEnforceMinSeparation(
                        aggressorH,
                        targetH,
                        cfg.separationRetries,
                        cfg.separationInitialDelayFrames);
                }
                return;
            }

            logger::trace(
                "Shove (queued): failed mag={} dur={} mult={} triesLeftAfter={}",
                mag, dur, weaponMult, remainingTries - 1);

            const auto nextTries = remainingTries - 1;
            if (nextTries > 0) {
                QueuePhysicsShove(aggressorH, targetH, nextTries, cfg.shoveRetryDelayFrames, weaponMult);
            }
            });
    }
}
