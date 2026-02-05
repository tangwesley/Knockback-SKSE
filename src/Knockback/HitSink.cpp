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

            RE::Actor* target = a_event->target ? a_event->target->As<RE::Actor>() : nullptr;
            RE::Actor* aggressor = a_event->cause ? a_event->cause->As<RE::Actor>() : nullptr;

            if (!target || !aggressor) return RE::BSEventNotifyControl::kContinue;
            if (target == aggressor) {
                logger::trace("Shove: target == aggressor");
                return RE::BSEventNotifyControl::kContinue;
            }
            if (target->IsDead() || aggressor->IsDead()) return RE::BSEventNotifyControl::kContinue;

            if (ShouldDisableDueToFirstPerson(aggressor)) return RE::BSEventNotifyControl::kContinue;

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

            const float weaponMult = GetWeaponMultiplier(weap);
            if (weaponMult <= 0.0f) {
                logger::trace("Shove: weapon is not configured");
                return RE::BSEventNotifyControl::kContinue;
            }

            const auto& cfg = GetConfig();

            logger::trace(
                "Shove: queue target={:08X} aggressor={:08X} mag={} dur={} retries={} delayFrames={} DisableInFirstPerson={}",
                target->GetFormID(), aggressor->GetFormID(),
                cfg.shoveMagnitude * weaponMult, cfg.shoveDuration,
                cfg.shoveRetries, cfg.shoveRetryDelayFrames,
                cfg.disableInFirstPerson);

            QueuePhysicsShoveWithAttackDeferral(
                aggressor->GetHandle(),
                target->GetHandle(),
                cfg.shoveRetries,
                weaponMult,
				20);
            
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
