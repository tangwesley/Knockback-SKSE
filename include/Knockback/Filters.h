#pragma once

#include <RE/Skyrim.h>

namespace Knockback
{
    void InitKeywords();

    bool IsPlayer(RE::Actor* a);

    // Actor::IsDead() reports true for live actors on some runtimes; read the
    // life state bitfield instead of dispatching through the vtable.
    bool IsAlive(RE::Actor* a);
    bool ShouldDisableDueToFirstPerson(RE::Actor* aggressor);

    bool IsValidKnockbackTarget(const RE::Actor* target);

    float GetWeaponMultiplier(const RE::TESObjectWEAP* weap);
    bool IsMeleeWeapon(const RE::TESObjectWEAP* weap);
    bool IsMagicSource(RE::FormID sourceID);
    const RE::TESObjectWEAP* ResolveWeaponFromEventOrEquipped(const RE::TESHitEvent& evt, RE::Actor* aggressor);
    bool GetIsAttacking(RE::Actor* a);
}
