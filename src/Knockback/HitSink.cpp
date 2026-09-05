#include <Knockback/HitSink.h>

#include <Knockback/Config.h>
#include <Knockback/Filters.h>
#include <Knockback/Tasks.h>

#include <RE/S/ScriptEventSourceHolder.h>
#include <RE/T/TESHitEvent.h>

#include <filesystem>
#include <chrono>
#include <mutex>

namespace logger = SKSE::log;

namespace Knockback
{
    class HitEventSink : public RE::BSTEventSink<RE::TESHitEvent>
    {
    public:
        static HitEventSink* GetSingleton()
        {
            static HitEventSink s;
            return std::addressof(s);
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event,
            RE::BSTEventSource<RE::TESHitEvent>*) override
        {
            if (!a_event) {
                return RE::BSEventNotifyControl::kContinue;
            }

            Knockback::MaybeReloadConfig();

            logger::trace("Hit: target={:08X} cause={:08X} source={:08X} projectile={:08X} flags={:02X}",
                a_event->target ? a_event->target->GetFormID() : 0,
                a_event->cause ? a_event->cause->GetFormID() : 0,
                a_event->source,
                a_event->projectile,
                a_event->flags.underlying());

            RE::Actor* target = a_event->target ? a_event->target->As<RE::Actor>() : nullptr;
            RE::Actor* aggressor = a_event->cause ? a_event->cause->As<RE::Actor>() : nullptr;

            if (!target || !aggressor) {
                logger::trace("Shove: skipped (target or cause is not an actor)");
                return RE::BSEventNotifyControl::kContinue;
            }
            if (target == aggressor) {
                logger::trace("Shove: target == aggressor");
                return RE::BSEventNotifyControl::kContinue;
            }
            if (!IsAlive(target) || !IsAlive(aggressor)) {
                logger::trace("Shove: skipped (target or aggressor is not alive)");
                return RE::BSEventNotifyControl::kContinue;
            }

            if (ShouldDisableDueToFirstPerson(aggressor)) {
                logger::trace("Shove: skipped (player aggressor in first person)");
                return RE::BSEventNotifyControl::kContinue;
            }

            if (!IsValidKnockbackTarget(target)) {
                logger::trace("Shove: target not allowed (humanoid filter)");
                return RE::BSEventNotifyControl::kContinue;
            }

            if (a_event->projectile != 0) {
                logger::trace("Shove: skipped (projectile hit) projectile={:08X}", a_event->projectile);
                return RE::BSEventNotifyControl::kContinue;
            }

            if (IsMagicSource(a_event->source)) {
                logger::trace("Shove: skipped (magic source) source={:08X}", a_event->source);
                return RE::BSEventNotifyControl::kContinue;
            }

            const auto* weap = ResolveWeaponFromEventOrEquipped(*a_event, aggressor);

            // If source is set but didn't resolve to a weapon (e.g. dragon breath
            // explosion, ability effect), it isn't a melee hit — don't fall back
            // to the unarmed multiplier.
            if (a_event->source != 0 && !weap) {
                logger::trace("Shove: skipped (non-weapon source) source={:08X}", a_event->source);
                return RE::BSEventNotifyControl::kContinue;
            }

            const float weaponMult = GetWeaponMultiplier(weap);
            if (weaponMult <= 0.0f) {
                logger::trace("Shove: weapon is not configured");
                return RE::BSEventNotifyControl::kContinue;
            }

            const auto& cfg = GetConfig();

            float powerMult = 1.0f;
            if (a_event->flags.any(RE::TESHitEvent::Flag::kPowerAttack)) {
                powerMult = cfg.powerAttackMultiplier;  // add this to config/ini
            }

            logger::trace(
                "Shove: queue target={:08X} aggressor={:08X} mag={} dur={} DisableInFirstPerson={}",
                target->GetFormID(), aggressor->GetFormID(),
                cfg.shoveMagnitude * weaponMult * powerMult, cfg.shoveDuration,
                cfg.disableInFirstPerson);

            QueuePhysicsShove(
                aggressor->GetHandle(),
                target->GetHandle(),
                weaponMult * powerMult);
            
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    void RegisterHitSink()
    {
        InitKeywords();
        LoadConfig();

        auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
        if (!holder) {
            logger::error("ScriptEventSourceHolder not available");
            return;
        }

        holder->AddEventSink(HitEventSink::GetSingleton());
        logger::info("Registered TESHitEvent sink");
    }

    void OnSKSEMessage(SKSE::MessagingInterface::Message* msg)
    {
        if (!msg) {
            return;
        }

        if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
            RegisterHitSink();
        }
    }
}
