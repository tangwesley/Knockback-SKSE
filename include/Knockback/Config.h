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

        // While the target is attacking / animation-driven, root motion overwrites the
        // character controller's velocity every frame and swallows ApplyCurrent. For this
        // many frames after a successful shove, re-assert the shove velocity directly on the
        // controller each frame the target is still animation-driven. 0 disables.
        std::int32_t animDrivenRefreshFrames{ 8 };

        // Player as target. The player's rigid-body controller rewrites its velocity every
        // physics step, so the only push that moves the player is the per-step substitution
        // in FrameTick, and it lasts exactly this window (with a linear ease-out). Distance is
        // roughly magnitude * multiplier * frames / (2 * fps). 0 frames disables.
        float playerShoveMultiplier{ 1.0f };
        std::int32_t playerShoveFrames{ 24 };

        // POV option: suppress when player aggressor in first-person
        bool disableInFirstPerson{ true };

        // Suppress per-hit trace logging (info and above still logged)
        bool disableVerboseLogs{ true };

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
		float powerAttackMultiplier{ 1.2f };
        bool HasAllowList() const { return !allowRaces.empty(); }
    };

    // Accessors
    const Config& GetConfig();
    void LoadConfig();
    void MaybeReloadConfig();
}
