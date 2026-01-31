// plugin.cpp
// SKSE + CommonLibSSE-NG: physics shove knockback on melee hits, humanoids only.
// Configurable via INI (allow/deny race lists + shove magnitude + duration + retries + first-person toggle).

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

// For _mm_setr_ps
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESHitEvent.h>
#include "RE/P/PlayerCharacter.h"
#include <xmmintrin.h>

#include "RE/S/ScriptEventSourceHolder.h"
#include "RE/Skyrim.h"
#include "RE/T/TESObjectWEAP.h"
#include "RE/T/TESRace.h"
#include "SKSE/SKSE.h"
#include "SimpleIni.h"  // https://github.com/brofield/simpleini (header-only OK)

namespace logger = SKSE::log;

void SetupLog() {
    auto logsFolder = SKSE::log::log_directory();
    if (!logsFolder) SKSE::stl::report_and_fail("SKSE log_directory not provided, logs disabled.");

    auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
    auto logFilePath = *logsFolder / std::format("{}.log", pluginName);

    auto fileLoggerPtr = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
    auto loggerPtr = std::make_shared<spdlog::logger>("log", std::move(fileLoggerPtr));

    spdlog::set_default_logger(std::move(loggerPtr));
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::trace);
}

namespace {
    // -------------------------
    // Config
    // -------------------------
    struct Config {
        // Interpreted as "speed" for ApplyCurrent (units are game/Havok-y; tune by feel).
        float shoveMagnitude{2.5f};

        // How long to apply that velocity burst.
        float shoveDuration{0.12f};

        // acceptance helper (not a gameplay min)
        float applyCurrentMinVelocity{4.0f};   
        // clamp: duration won't go below dur*scale
        float minDurationScale{0.15f};         

        // How many total attempts to apply shove (including first queued attempt).
        std::int32_t shoveRetries{3};

        // How many "task ticks" to wait between attempts (1 ~= next update).
        std::int32_t shoveRetryDelayFrames{1};

        // If true, do not apply knockback when the player (as aggressor) is in first-person.
        bool disableInFirstPerson{true};

        // If allowRaces is non-empty, only races in this set qualify (unless denied).
        // If allowRaces is empty, races are eligible unless denied (and then keyword fallback applies).
        std::unordered_set<RE::FormID> allowRaces;
        std::unordered_set<RE::FormID> denyRaces;

        bool HasAllowList() const { return !allowRaces.empty(); }
    };

    Config g_cfg;

    // -------------------------
    // Helpers
    // -------------------------
    static std::string Trim(std::string s) {
        auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };

        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&](char c) { return !is_space((unsigned char)c); }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [&](char c) { return !is_space((unsigned char)c); }).base(),
                s.end());
        return s;
    }

    static std::string StripIniComment(std::string s) {
        // Common INI comment starts: ';' or '#'
        const auto pos = s.find_first_of(";#");
        if (pos != std::string::npos) {
            s.erase(pos);
        }
        return Trim(std::move(s));
    }

    static std::vector<std::string> SplitCSV(std::string_view csv) {
        std::vector<std::string> out;
        std::string cur;
        for (char c : csv) {
            if (c == ',') {
                cur = StripIniComment(std::move(cur));
                if (!cur.empty()) {
                    out.push_back(std::move(cur));
                }
                cur.clear();
            } else {
                cur.push_back(c);
            }
        }

        cur = StripIniComment(std::move(cur));
        if (!cur.empty()) {
            out.push_back(std::move(cur));
        }

        return out;
    }

    static std::string NormalizeHexToken(std::string hex) {
        hex = Trim(std::move(hex));

        // Accept "FormID:00013796"
        if (hex.rfind("FormID:", 0) == 0) {
            hex = Trim(hex.substr(6));
        }

        // Accept "0x00013796"
        if (hex.rfind("0x", 0) == 0 || hex.rfind("0X", 0) == 0) {
            hex = Trim(hex.substr(2));
        }

        return hex;
    }

    // Parse "PluginName.esm|00ABCDEF" -> full FormID (load-order aware)
    // Returns 0 on failure.
    static RE::FormID ParseFormSpec(const std::string& spec) {
        const auto cleaned = StripIniComment(spec);
        const auto bar = cleaned.find('|');
        if (bar == std::string::npos) {
            return 0;
        }

        std::string file = Trim(cleaned.substr(0, bar));
        std::string hex = NormalizeHexToken(cleaned.substr(bar + 1));

        if (file.empty() || hex.empty()) {
            return 0;
        }

        std::uint32_t localID = 0;
        try {
            localID = static_cast<std::uint32_t>(std::stoul(hex, nullptr, 16));
        } catch (...) {
            return 0;
        }

        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data) {
            return 0;
        }

        const RE::FormID fullID = data->LookupFormID(localID, file);
        if (!fullID) {
            SKSE::log::warn("LookupFormID failed: file='{}' localID=0x{:08X}", file, localID);
        }
        return fullID;
    }

    static void LoadConfig() {
        g_cfg = Config{};

        const auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
        const auto iniPath = std::format("Data\\SKSE\\Plugins\\{}.ini", pluginName);

        CSimpleIniA ini;
        ini.SetUnicode();
        ini.SetMultiKey();

        const auto rc = ini.LoadFile(iniPath.c_str());
        if (rc < 0) {
            SKSE::log::warn("Config not found or failed to load: {}", iniPath);
            SKSE::log::warn(
                "Using defaults (ShoveMagnitude={}, ShoveDuration={}, ShoveRetries={}, ShoveRetryDelayFrames={}, "
                "DisableInFirstPerson={})",
                g_cfg.shoveMagnitude, g_cfg.shoveDuration, g_cfg.shoveRetries, g_cfg.shoveRetryDelayFrames,
                g_cfg.disableInFirstPerson);
            return;
        }

        g_cfg.shoveMagnitude = static_cast<float>(ini.GetDoubleValue("General", "ShoveMagnitude", g_cfg.shoveMagnitude));
        g_cfg.shoveDuration = static_cast<float>(ini.GetDoubleValue("General", "ShoveDuration", g_cfg.shoveDuration));

        g_cfg.shoveRetries = static_cast<std::int32_t>(ini.GetLongValue("General", "ShoveRetries", g_cfg.shoveRetries));
        g_cfg.shoveRetryDelayFrames = static_cast<std::int32_t>( ini.GetLongValue("General", "ShoveRetryDelayFrames", g_cfg.shoveRetryDelayFrames));

        g_cfg.disableInFirstPerson = ini.GetBoolValue("General", "DisableInFirstPerson", g_cfg.disableInFirstPerson);
        g_cfg.applyCurrentMinVelocity = static_cast<float>(ini.GetDoubleValue("General", "ApplyCurrentMinVelocity", g_cfg.applyCurrentMinVelocity));
        g_cfg.minDurationScale = static_cast<float>(ini.GetDoubleValue("General", "MinDurationScale", g_cfg.minDurationScale));

        if (g_cfg.applyCurrentMinVelocity < 0.0f) g_cfg.applyCurrentMinVelocity = 0.0f;
        if (g_cfg.minDurationScale < 0.0f) g_cfg.minDurationScale = 0.0f;
        if (g_cfg.minDurationScale > 1.0f) g_cfg.minDurationScale = 1.0f;

        // Clamp to sane values
        if (g_cfg.shoveRetries < 1) {
            g_cfg.shoveRetries = 1;
        }
        if (g_cfg.shoveRetries > 10) {
            g_cfg.shoveRetries = 10;
        }
        if (g_cfg.shoveRetryDelayFrames < 0) {
            g_cfg.shoveRetryDelayFrames = 0;
        }
        if (g_cfg.shoveRetryDelayFrames > 10) {
            g_cfg.shoveRetryDelayFrames = 10;
        }

        const char* allowStr = ini.GetValue("Races", "Allow", "");
        const char* denyStr = ini.GetValue("Races", "Deny", "");

        for (auto& item : SplitCSV(allowStr)) {
            if (const auto id = ParseFormSpec(item); id != 0) {
                g_cfg.allowRaces.insert(id);
            } else {
                SKSE::log::warn("Invalid Allow race spec: '{}'", item);
            }
        }

        for (auto& item : SplitCSV(denyStr)) {
            if (const auto id = ParseFormSpec(item); id != 0) {
                g_cfg.denyRaces.insert(id);
            } else {
                SKSE::log::warn("Invalid Deny race spec: '{}'", item);
            }
        }

        SKSE::log::info(
            "Config loaded: ShoveMagnitude={}, ShoveDuration={}, ShoveRetries={}, ShoveRetryDelayFrames={}, "
            "DisableInFirstPerson={}, AllowRaces={}, DenyRaces={}",
            g_cfg.shoveMagnitude, g_cfg.shoveDuration, g_cfg.shoveRetries, g_cfg.shoveRetryDelayFrames,
            g_cfg.disableInFirstPerson, g_cfg.allowRaces.size(), g_cfg.denyRaces.size());
    }

    // True if we should suppress knockback because the player is in first-person.
    static bool ShouldDisableDueToFirstPerson(RE::Actor* aggressor) {
        if (!g_cfg.disableInFirstPerson) {
            return false;
        }
        if (!aggressor) {
            return false;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || aggressor != player) {
            return false;
        }

        // PlayerCamera is the canonical source for POV.
        auto* cam = RE::PlayerCamera::GetSingleton();
        if (!cam) {
            return false;
        }

        return cam->IsInFirstPerson();
    }

    // -------------------------
    // Keyword helpers (fallback heuristic)
    // -------------------------
    constexpr RE::FormID kKW_ActorTypeNPC = 0x00013794;               // ActorTypeNPC
    constexpr RE::FormID kKW_ActorTypeUndead = 0x00013795;            // ActorTypeUndead
    constexpr RE::FormID kKW_ActorTypeDragon = 0x00013796;            // ActorTypeDragon
    constexpr RE::FormID kKW_ActorTypeGiant = 0x00013797;             // ActorTypeGiant
    constexpr RE::FormID kKW_ActorTypeDwarvenAutomaton = 0x00013798;  // ActorTypeDwarvenAutomaton

    struct KeywordCache {
        RE::BGSKeyword* npc{nullptr};
        RE::BGSKeyword* undead{nullptr};
        RE::BGSKeyword* dragon{nullptr};
        RE::BGSKeyword* giant{nullptr};
        RE::BGSKeyword* dwarvenAuto{nullptr};

        void Init() {
            npc = RE::TESForm::LookupByID<RE::BGSKeyword>(kKW_ActorTypeNPC);
            undead = RE::TESForm::LookupByID<RE::BGSKeyword>(kKW_ActorTypeUndead);
            dragon = RE::TESForm::LookupByID<RE::BGSKeyword>(kKW_ActorTypeDragon);
            giant = RE::TESForm::LookupByID<RE::BGSKeyword>(kKW_ActorTypeGiant);
            dwarvenAuto = RE::TESForm::LookupByID<RE::BGSKeyword>(kKW_ActorTypeDwarvenAutomaton);
        }
    };

    KeywordCache g_kw;

    static bool HasKW(const RE::Actor* actor, const RE::BGSKeyword* kw) {
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

    static bool IsHumanoidAllowed(const RE::Actor* target) {
        if (!target) {
            return false;
        }

        const auto* race = target->GetRace();
        if (!race) {
            return false;
        }

        const RE::FormID raceID = race->GetFormID();

        // INI deny list wins
        if (g_cfg.denyRaces.contains(raceID)) {
            return false;
        }

        // If allow list exists, require membership
        if (g_cfg.HasAllowList() && !g_cfg.allowRaces.contains(raceID)) {
            return false;
        }

        // Fallback heuristic: exclude big archetypes
        if (HasKW(target, g_kw.dragon) || HasKW(target, g_kw.giant) || HasKW(target, g_kw.dwarvenAuto)) {
            return false;
        }

        // Allow humanoids (and undead humanoids)
        if (HasKW(target, g_kw.npc) || HasKW(target, g_kw.undead)) {
            return true;
        }

        // If allow list exists, we already passed it—so permit.
        if (g_cfg.HasAllowList()) {
            return true;
        }

        return false;
    }

    // -------------------------
    // Weapon filters
    // -------------------------
    static bool IsMeleeWeapon(const RE::TESObjectWEAP* weap) {
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

    static bool IsMagicSource(RE::FormID sourceID)
    {
        if (sourceID == 0) {
            return false;
        }

        auto* form = RE::TESForm::LookupByID(sourceID);
        if (!form) {
            return false;
        }

        // Treat any MagicItem-derived thing as "spell/magic hit"
        return form->As<RE::MagicItem>() != nullptr;
    }

    static const RE::TESObjectWEAP* ResolveWeaponFromEventOrEquipped(const RE::TESHitEvent& evt, RE::Actor* aggressor) {
        if (evt.source != 0) {
            if (auto* form = RE::TESForm::LookupByID(evt.source)) {
                if (auto* weap = form->As<RE::TESObjectWEAP>()) {
                    return weap;
                }
            }
        }

        return nullptr;
    }


    static bool ApplyPhysicsShove(RE::Actor* aggressor, RE::Actor* target, float magnitude, float duration) {
        if (!aggressor || !target) {
            logger::trace("ApplyPhysicsShove: null aggressor/target");
            return false;
        }

        // Direction from aggressor -> target
        const auto aPos = aggressor->GetPosition();
        const auto tPos = target->GetPosition();

        float dx = tPos.x - aPos.x;
        float dy = tPos.y - aPos.y;
        float dz = tPos.z - aPos.z;

        // Flatten vertical by default (feels like a shove instead of a launch)
        dz = 0.0f;

        const float lenSq = dx * dx + dy * dy + dz * dz;
        if (lenSq < 1e-6f) {
            logger::trace("ApplyPhysicsShove: degenerate dir (aPos=({},{}), tPos=({},{}), lenSq={})",
                aPos.x, aPos.y, tPos.x, tPos.y, lenSq);
            return false;
        }

        const float invLen = 1.0f / std::sqrt(lenSq);
        dx *= invLen;
        dy *= invLen;
        dz *= invLen;

        // Convert to a velocity vector
        const float vx = dx * magnitude;
        const float vy = dy * magnitude;
        const float vz = dz * magnitude;

        RE::hkVector4 vel{};
        vel.quad = _mm_setr_ps(vx, vy, vz, 0.0f);

        // Apply a short velocity burst
        return target->ApplyCurrent(duration, vel);
    }

    // -------------------------
    // Next-frame queue + retries
    // -------------------------
    static void QueuePhysicsShove(RE::ActorHandle aggressorH, RE::ActorHandle targetH, std::int32_t remainingTries,
                                  std::int32_t delayFrames) {
        auto taskIf = SKSE::GetTaskInterface();
        if (!taskIf) {
			logger::trace("Shove (queued): no TaskInterface");
            return;
        }

        taskIf->AddTask([aggressorH, targetH, remainingTries, delayFrames]() {
            // countdown delay
            if (delayFrames > 0) {
                QueuePhysicsShove(aggressorH, targetH, remainingTries, delayFrames - 1);
                return;
            }

            auto aggressorPtr = aggressorH.get();
            auto targetPtr = targetH.get();

            RE::Actor* aggressor = aggressorPtr ? aggressorPtr.get() : nullptr;
            RE::Actor* target = targetPtr ? targetPtr.get() : nullptr;

            if (!aggressor || !target) {
                return;
            }
            if (aggressor == target) {
                return;
            }
            if (aggressor->IsDead() || target->IsDead()) {
                return;
            }

            // Re-check first-person suppression (POV may have changed since the hit)
            if (ShouldDisableDueToFirstPerson(aggressor)) {
                logger::trace("Shove (queued): suppressed (player in first-person)");
                return;
            }

            // Re-check humanoid filter (race can be swapped, summoned, etc.)
            if (!IsHumanoidAllowed(target)) {
                return;
            }

            float mag = g_cfg.shoveMagnitude;
            float dur = g_cfg.shoveDuration;

            // Shape it for ApplyCurrent acceptance: raise peak, shorten duration to preserve "feel"
            if (g_cfg.applyCurrentMinVelocity > 0.0f && mag > 0.0f) {
                const float peak = std::max(mag, g_cfg.applyCurrentMinVelocity);
                const float scaled = dur * (mag / peak);  // preserve displacement ~ mag*dur
                const float minDur = dur * g_cfg.minDurationScale;

                mag = peak;
                dur = std::max(scaled, minDur);
            }

            const bool ok = ApplyPhysicsShove(aggressor, target, mag, dur);

            if (ok) {
                logger::trace("Shove (queued): applied (tries left after this={})", remainingTries - 1);
                return;
            }

            logger::trace("Shove (queued): failed to apply (tries left after this={})", remainingTries - 1);

            const auto nextTries = remainingTries - 1;
            if (nextTries > 0) {
                QueuePhysicsShove(aggressorH, targetH, nextTries, g_cfg.shoveRetryDelayFrames);
            }
        });
    }

    // -------------------------
    // Event sink
    // -------------------------
    class HitEventSink : public RE::BSTEventSink<RE::TESHitEvent> {
    public:
        static HitEventSink* GetSingleton() {
            static HitEventSink s;
            return std::addressof(s);
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event,
                                              RE::BSTEventSource<RE::TESHitEvent>*) override {
            if (!a_event) {
                return RE::BSEventNotifyControl::kContinue;
            }

            // Resolve target & aggressor
            RE::Actor* target = a_event && a_event->target ? a_event->target->As<RE::Actor>() : nullptr;
            RE::Actor* aggressor = a_event && a_event->cause ? a_event->cause->As<RE::Actor>() : nullptr;

            if (!target || !aggressor) {
                logger::trace("Shove: missing target or aggressor");
                return RE::BSEventNotifyControl::kContinue;
            }
            if (target == aggressor) {
                logger::trace("Shove: target == aggressor");
                return RE::BSEventNotifyControl::kContinue;
            }
            if (target->IsDead() || aggressor->IsDead()) {
                logger::trace("Shove: target or aggressor is dead");
                return RE::BSEventNotifyControl::kContinue;
            }

            // Suppress in first-person if configured (player aggressor only)
            if (ShouldDisableDueToFirstPerson(aggressor)) {
                logger::trace("Shove: suppressed (player in first-person)");
                return RE::BSEventNotifyControl::kContinue;
            }

            // Humanoid-only filter
            if (!IsHumanoidAllowed(target)) {
                logger::trace("Shove: target not allowed (humanoid filter)");
                return RE::BSEventNotifyControl::kContinue;
            }

            // 1) Never shove on projectile hits (arrows, bolts, spell projectiles, etc.)
            if (a_event->projectile != 0) {
                logger::trace("Shove: skipped (projectile hit) projectile={:08X}", a_event->projectile);
                return RE::BSEventNotifyControl::kContinue;
            }

            // 2) Never shove on magic/spell sources
            if (IsMagicSource(a_event->source)) {
                logger::trace("Shove: skipped (magic source) source={:08X}", a_event->source);
                return RE::BSEventNotifyControl::kContinue;
            }

            // Melee weapon-only
            const auto* weap = ResolveWeaponFromEventOrEquipped(*a_event, aggressor);

            // Treat nullptr as unarmed melee (fists)
            const bool isMelee = IsMeleeWeapon(weap) || weap == nullptr;
            if (!isMelee) {
                logger::trace("Shove: weapon is not melee");
                return RE::BSEventNotifyControl::kContinue;
            }

            logger::trace(
                "Shove: queue target={:08X} aggressor={:08X} mag={} dur={} retries={} delayFrames={} "
                "DisableInFirstPerson={}",
                target->GetFormID(), aggressor->GetFormID(), g_cfg.shoveMagnitude, g_cfg.shoveDuration,
                g_cfg.shoveRetries, g_cfg.shoveRetryDelayFrames, g_cfg.disableInFirstPerson);

            QueuePhysicsShove(aggressor->GetHandle(), target->GetHandle(), g_cfg.shoveRetries, /*delayFrames*/ 0);

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    static void RegisterHitSink() {
        g_kw.Init();
        LoadConfig();

        auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
        if (!holder) {
            logger::error("ScriptEventSourceHolder not available");
            return;
        }

        holder->AddEventSink(HitEventSink::GetSingleton());
        logger::info("Registered TESHitEvent sink");
    }

    static void OnSKSEMessage(SKSE::MessagingInterface::Message* msg) {
        if (!msg) {
            return;
        }

        if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
            RegisterHitSink();
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    SetupLog();

    logger::info("KnockbackPlugin loaded (logger OK)");

    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging) {
        logger::critical("Messaging interface not available");
        return false;
    }

    messaging->RegisterListener(OnSKSEMessage);
    logger::info("Registered SKSE messaging listener");
    return true;
}
