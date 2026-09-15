#pragma once

#include <RE/Skyrim.h>

namespace Knockback
{
    float HorizontalDistance(RE::Actor* a, RE::Actor* b);

    // Best-effort ApplyCurrent push. The engine ignores currents below roughly 4 m/s, so a
    // weaker request is raised to that speed and shortened by the same ratio internally;
    // callers pass the configured magnitude and duration as written.
    bool ApplyPhysicsShove(RE::Actor* aggressor, RE::Actor* target, float magnitude, float duration);

    // Writes the shove velocity straight onto the target's character controller for
    // this frame (horizontal only; vertical velocity is preserved). Unlike ApplyCurrent
    // this is not persistent: root motion overwrites it next frame, so call it per frame.
    bool ApplyControllerVelocity(RE::Actor* aggressor, RE::Actor* target, float magnitude);

    bool ApplyVelocityAwayFrom(RE::Actor* from, RE::Actor* who, float magnitude, float duration);
}
