#pragma once

#include <RE/Skyrim.h>

namespace Knockback
{
    void InitKeywords();

    bool IsPlayer(RE::Actor* a);
    bool ShouldDisableDueToFirstPerson(RE::Actor* aggressor);

    bool IsHumanoidAllowed(const RE::Actor* target);

    float GetWeaponMultiplier(const RE::TESObjectWEAP* weap);
    bool IsMeleeWeapon(const RE::TESObjectWEAP* weap);
    bool IsMagicSource(RE::FormID sourceID);

    const RE::TESObjectWEAP* ResolveWeaponFromEventOrEquipped(const RE::TESHitEvent& evt, RE::Actor* aggressor);
}
