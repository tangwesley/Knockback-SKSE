#pragma once

#include <RE/Skyrim.h>

namespace Knockback
{
    float HorizontalDistance(RE::Actor* a, RE::Actor* b);

    void ShapeForApplyCurrent(float& mag, float& dur);

    bool ApplyPhysicsShove(RE::Actor* aggressor, RE::Actor* target, float magnitude, float duration);

    bool ApplyVelocityAwayFrom(RE::Actor* from, RE::Actor* who, float magnitude, float duration);
}
