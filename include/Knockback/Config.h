#pragma once

#include <RE/Skyrim.h>
#include <cstdint>
#include <unordered_set>

namespace Knockback
{
    struct Config
    {
        // Interpreted as "speed" for ApplyCurrent (units are game/Havok-y; tune by feel).
        float shoveMagnitude{ 2.5f };
        float shoveDuration{ 0.12f };

        // acceptance helper (not a gameplay min)
        float applyCurrentMinVelocity{ 4.0f };
        float minDurationScale{ 0.15f };

        // Attempts to apply shove
        std::int32_t shoveRetries{ 3 };
        std::int32_t shoveRetryDelayFrames{ 1 };

        // Delay before first shove attempt (helps avoid same-tick controller clobber)
        std::int32_t shoveInitialDelayFrames{ 1 };

        // If after a shove the target hasn't separated by at least this many units, reapply shove.
        float minShoveSeparationDelta{ 8.0f };

        // POV option: suppress when player aggressor in first-person
        bool disableInFirstPerson{ true };

        // Race allow/deny lists
        std::unordered_set<RE::FormID> allowRaces;
        std::unordered_set<RE::FormID> denyRaces;

        // Separation enforcement (player aggressor only)
        bool enforceMinSeparation{ true };
        float minSeparationDistance{ 110.0f };
        float separationPushDuration{ 0.10f };
        float separationMaxVelocity{ 10.0f };
        std::int32_t separationRetries{ 6 };
        std::int32_t separationInitialDelayFrames{ 1 };
        std::int32_t separationRetryDelayFrames{ 1 };

        // WeaponType magnitude multipliers
        std::unordered_map<RE::FormID, float> weaponTypeMultipliers;
		std::unordered_map<RE::BGSKeyword*, float> weaponTypeKeywordMultipliers;
		float unarmedMultiplier{ 0.85f };

        bool HasAllowList() const { return !allowRaces.empty(); }
    };

    // Accessors
    const Config& GetConfig();
    void LoadConfig();
}
