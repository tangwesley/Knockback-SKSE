#pragma once

#include <RE/Skyrim.h>

namespace Knockback
{
    void InitKeywords();

    bool IsPlayer(RE::Actor* a);
    bool ShouldDisableDueToFirstPerson(RE::Actor* aggressor);

    bool IsHumanoidAllowed(const RE::Actor* target);

    bool IsMeleeWeapon(const RE::TESObjectWEAP* weap);
    bool IsMagicSource(RE::FormID sourceID);

    const RE::TESObjectWEAP* ResolveWeaponFromEventOrEquipped(const RE::TESHitEvent& evt, RE::Actor* aggressor);
}
