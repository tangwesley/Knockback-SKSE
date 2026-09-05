#pragma once

#include <RE/Skyrim.h>

namespace Knockback
{
    // Applies the ApplyCurrent shove at the next SKSE task drain and hands any
    // multi-frame follow-up (refresh, player push, separation) to the frame hooks.
    void QueuePhysicsShove(
        RE::ActorHandle aggressorH,
        RE::ActorHandle targetH,
        float weaponMult);
}
