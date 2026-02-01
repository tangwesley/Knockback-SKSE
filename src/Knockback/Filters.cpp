#include <Knockback/Filters.h>
#include <Knockback/Config.h>

#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESRace.h>

namespace Knockback
{
    // Keyword helpers (fallback heuristic)
    constexpr RE::FormID kKW_ActorTypeNPC = 0x00013794;               // ActorTypeNPC
    constexpr RE::FormID kKW_ActorTypeUndead = 0x00013795;            // ActorTypeUndead
    constexpr RE::FormID kKW_ActorTypeDragon = 0x00013796;            // ActorTypeDragon
    constexpr RE::FormID kKW_ActorTypeGiant = 0x00013797;             // ActorTypeGiant
    constexpr RE::FormID kKW_ActorTypeDwarvenAutomaton = 0x00013798;  // ActorTypeDwarvenAutomaton

    struct KeywordCache {
        RE::BGSKeyword* npc{ nullptr };
        RE::BGSKeyword* undead{ nullptr };
        RE::BGSKeyword* dragon{ nullptr };
        RE::BGSKeyword* giant{ nullptr };
        RE::BGSKeyword* dwarvenAuto{ nullptr };

        void Init()
        {
            npc = RE::TESForm::LookupByID<RE::BGSKeyword>(kKW_ActorTypeNPC);
            undead = RE::TESForm::LookupByID<RE::BGSKeyword>(kKW_ActorTypeUndead);
            dragon = RE::TESForm::LookupByID<RE::BGSKeyword>(kKW_ActorTypeDragon);
            giant = RE::TESForm::LookupByID<RE::BGSKeyword>(kKW_ActorTypeGiant);
            dwarvenAuto = RE::TESForm::LookupByID<RE::BGSKeyword>(kKW_ActorTypeDwarvenAutomaton);
        }
    };

    static KeywordCache g_kw;

    void InitKeywords()
    {
        g_kw.Init();
    }

    static bool HasKW(const RE::Actor* actor, const RE::BGSKeyword* kw)
    {
        if (!actor || !kw) {
            return false;
        }

        if (actor->HasKeyword(kw)) {
            return true;
        }

        if (auto race = actor->GetRace(); race && race->HasKeyword(kw)) {
            return true;
        }

        return false;
    }

    bool IsPlayer(RE::Actor* a)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        return a && player && a == player;
    }

    bool ShouldDisableDueToFirstPerson(RE::Actor* aggressor)
    {
        const auto& cfg = GetConfig();

        if (!cfg.disableInFirstPerson) {
            return false;
        }
        if (!aggressor) {
            return false;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || aggressor != player) {
            return false;
        }

        auto* cam = RE::PlayerCamera::GetSingleton();
        if (!cam) {
            return false;
        }

        return cam->IsInFirstPerson();
    }

    bool IsHumanoidAllowed(const RE::Actor* target)
    {
        const auto& cfg = GetConfig();

        if (!target) {
            return false;
        }

        const auto* race = target->GetRace();
        if (!race) {
            return false;
        }

        const RE::FormID raceID = race->GetFormID();

        // deny list wins
        if (cfg.denyRaces.contains(raceID)) {
            return false;
        }

        // allow list enforced if present
        if (cfg.HasAllowList() && !cfg.allowRaces.contains(raceID)) {
            return false;
        }

        // exclude big archetypes
        if (HasKW(target, g_kw.dragon) || HasKW(target, g_kw.giant) || HasKW(target, g_kw.dwarvenAuto)) {
            return false;
        }

        // allow humanoids + undead humanoids
        if (HasKW(target, g_kw.npc) || HasKW(target, g_kw.undead)) {
            return true;
        }

        // if allow list exists we already passed it
        if (cfg.HasAllowList()) {
            return true;
        }

        return false;
    }

    bool IsMeleeWeapon(const RE::TESObjectWEAP* weap)
    {
        if (!weap) {
            return false;
        }

        const auto type = weap->GetWeaponType();
        switch (type) {
        case RE::WEAPON_TYPE::kOneHandSword:
        case RE::WEAPON_TYPE::kOneHandDagger:
        case RE::WEAPON_TYPE::kOneHandAxe:
        case RE::WEAPON_TYPE::kOneHandMace:
        case RE::WEAPON_TYPE::kTwoHandSword:
        case RE::WEAPON_TYPE::kTwoHandAxe:
        case RE::WEAPON_TYPE::kHandToHandMelee:
            return true;
        default:
            return false;
        }
    }

    bool IsMagicSource(RE::FormID sourceID)
    {
        if (sourceID == 0) {
            return false;
        }

        auto* form = RE::TESForm::LookupByID(sourceID);
        if (!form) {
            return false;
        }

        return form->As<RE::MagicItem>() != nullptr;
    }

    const RE::TESObjectWEAP* ResolveWeaponFromEventOrEquipped(const RE::TESHitEvent& evt, RE::Actor* /*aggressor*/)
    {
        if (evt.source != 0) {
            if (auto* form = RE::TESForm::LookupByID(evt.source)) {
                if (auto* weap = form->As<RE::TESObjectWEAP>()) {
                    return weap;
                }
            }
        }
        return nullptr;
    }
}
