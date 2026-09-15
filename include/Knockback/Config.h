#pragma once

#include <RE/Skyrim.h>
#include <cstdint>
#include <unordered_set>

namespace Knockback
{
    struct Config
    {
        // Interpreted as "speed" for ApplyCurrent (units are game/Havok-y; tune by feel).
        float shoveMagnitude{ 3.5f };
        float shoveDuration{ 0.12f };

        // Targets that are mid-attack are animation-driven and resist a short push. A hit
        // on an attacking target is pushed for at least this long (ShoveDuration if longer).
        float attackingTargetMinDuration{ 0.13f };

        // Player as target. The player's rigid-body controller rewrites its velocity every
        // physics step, so the only push that moves the player is the per-step substitution
        // in FrameTick. It lasts this long with a linear ease-out, so distance is roughly
        // magnitude * multiplier * duration / 2. 0 disables.
        float playerShoveMultiplier{ 1.0f };
        float playerShoveDuration{ 0.4f };

        // POV option: suppress when player aggressor in first-person
        bool disableInFirstPerson{ true };

        // POV option: suppress when the player is the target while in first-person
        bool disablePlayerKnockbackInFirstPerson{ false };

        // Suppress per-hit trace logging (info and above still logged)
        bool disableVerboseLogs{ true };

        // Race allow/deny lists
        std::unordered_set<RE::FormID> allowRaces;
        std::unordered_set<RE::FormID> denyRaces;

        // Separation enforcement (player aggressor only)
        bool enforceMinSeparation{ true };
        float minSeparationDistance{ 80.0f };
        float separationPushDuration{ 0.08f };
        float separationMaxVelocity{ 12.0f };
        std::int32_t separationRetries{ 6 };
        std::int32_t separationInitialDelayFrames{ 2 };
        std::int32_t separationRetryDelayFrames{ 1 };

        // WeaponType magnitude multipliers
        std::unordered_map<RE::FormID, float> weaponTypeMultipliers;
		std::unordered_map<RE::BGSKeyword*, float> weaponTypeKeywordMultipliers;
		float unarmedMultiplier{ 0.85f };
		float powerAttackMultiplier{ 1.2f };
        bool HasAllowList() const { return !allowRaces.empty(); }
    };

    // Accessors
    const Config& GetConfig();
    void LoadConfig();
    void MaybeReloadConfig();
}
