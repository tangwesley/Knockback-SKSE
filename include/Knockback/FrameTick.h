#pragma once

#include <RE/Skyrim.h>
#include <cstdint>

namespace Knockback
{
    // SKSE's task queue drains re-queued tasks in the same frame, so it cannot be used
    // as a per-frame tick. Two sets of hooks:
    //  - Character::Update / PlayerCharacter::Update (documented slot 0xAD): per-frame
    //    bookkeeping plus a direct velocity write right after the actor's own update.
    //  - bhkCharProxyController / bhkCharRigidBodyController::SetLinearVelocityImpl
    //    (documented slot 07): the engine's own per-step velocity write. While an
    //    override is active the horizontal part is replaced with the shove, so the push
    //    lands regardless of where in the frame the engine writes it. The player's
    //    rigid-body controller rewrites its velocity every step, so this is what
    //    actually moves the player.
    //
    // Installation is lazy: it needs a live actor to run the vtable-dispatch probe, and it
    // is skipped (with a warning) on a runtime where the documented slots are not trusted.
    bool EnsureFrameHooks(RE::Actor* a_liveActor);
    bool FrameHooksActive();

    // Registers (or replaces) a per-frame horizontal velocity override on the target,
    // serviced for up to `frames` real frames.
    //  easeOut:         magnitude ramps linearly to zero over the window instead of
    //                   holding constant and stopping dead.
    //  ignoreAnimState: keep pushing even when the target is not animation-driven.
    //                   Off, the override is dropped as soon as root motion lets go so
    //                   the ApplyCurrent velocity can take over (proxy controllers).
    void RegisterVelocityOverride(
        RE::ActorHandle aggressorH,
        RE::ActorHandle targetH,
        float magnitude,
        std::int32_t frames,
        bool easeOut = false,
        bool ignoreAnimState = false);

    // Minimum-separation enforcement for a player aggressor, serviced per frame from the
    // PlayerCharacter::Update hook. Waits SeparationInitialDelayFrames, then repeatedly:
    // measure the horizontal distance to the target; if it is under MinSeparationDistance,
    // push the player away from the target at deficit / SeparationPushDuration (capped at
    // SeparationMaxVelocity) for that duration via the velocity substitution, wait
    // SeparationRetryDelayFrames, and re-measure. Stops when the distance is met, the
    // retries run out, or two consecutive measurements show no progress.
    void RegisterSeparationJob(RE::ActorHandle playerH, RE::ActorHandle targetH);

    // Frame counter incremented by the PlayerCharacter::Update hook. Trace-log aid only.
    std::uint64_t FrameIndex();
}
