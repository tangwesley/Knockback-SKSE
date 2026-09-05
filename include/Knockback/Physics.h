#pragma once

#include <RE/Skyrim.h>

namespace Knockback
{
    float HorizontalDistance(RE::Actor* a, RE::Actor* b);

    // Runtime probe: true when the header's documented vtable slot numbers match the
    // game's real vtable (and the compiler-assigned indices do not). Cached after the
    // first call. Needs an actor already known to be alive.
    bool UseDocumentedVtableSlots(RE::Actor* a_liveActor);

    void ShapeForApplyCurrent(float& mag, float& dur);

    bool ApplyPhysicsShove(RE::Actor* aggressor, RE::Actor* target, float magnitude, float duration);

    // Writes the shove velocity straight onto the target's character controller for
    // this frame (horizontal only; vertical velocity is preserved). Unlike ApplyCurrent
    // this is not persistent: root motion overwrites it next frame, so call it per frame.
    bool ApplyControllerVelocity(RE::Actor* aggressor, RE::Actor* target, float magnitude);

    bool ApplyVelocityAwayFrom(RE::Actor* from, RE::Actor* who, float magnitude, float duration);
}
