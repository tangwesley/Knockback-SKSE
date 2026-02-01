#pragma once

#include <RE/Skyrim.h>
#include <cstdint>

namespace Knockback
{
    void QueuePhysicsShove(RE::ActorHandle aggressorH, RE::ActorHandle targetH, std::int32_t remainingTries, std::int32_t delayFrames);

    void QueueEnforceMinSeparation(RE::ActorHandle aggressorH,
        RE::ActorHandle targetH,
        std::int32_t remainingTries,
        std::int32_t delayFrames,
        float lastDist = -1.0f,
        std::int32_t noProgressCount = 0);
}
