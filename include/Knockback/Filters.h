#pragma once

#include <RE/Skyrim.h>

namespace Knockback
{
    void InitKeywords();

    bool IsPlayer(RE::Actor* a);

    // Actor::IsDead() reports true for live actors on some runtimes; read the
    // life state bitfield instead of dispatching through the vtable.
    bool IsAlive(RE::Actor* a);
    // Player aggressor in first person (DisableInFirstPerson).
    bool ShouldDisableDueToFirstPerson(RE::Actor* aggressor);
    // Player target in first person (DisablePlayerKnockbackInFirstPerson).
    bool ShouldDisablePlayerKnockbackDueToFirstPerson(RE::Actor* target);

    bool IsValidKnockbackTarget(const RE::Actor* target);

    float GetWeaponMultiplier(const RE::TESObjectWEAP* weap);
    bool IsMeleeWeapon(const RE::TESObjectWEAP* weap);
    bool IsMagicSource(RE::FormID sourceID);
    const RE::TESObjectWEAP* ResolveWeaponFromEventOrEquipped(const RE::TESHitEvent& evt, RE::Actor* aggressor);
    bool GetIsAttacking(RE::Actor* a);

    // True while the actor's movement is being written by animation root motion
    // (attacks, staggers, some idles). In that state the character controller's
    // velocity is overwritten every frame.
    bool IsAnimDrivenOrAttacking(RE::Actor* a);
}
