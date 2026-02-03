#include <Knockback/Config.h>

#include "SKSE/SKSE.h"
#include "SimpleIni.h"

#include <RE/T/TESDataHandler.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace logger = SKSE::log;

namespace Knockback
{
    static Config g_cfg{};

    const Config& GetConfig()
    {
        return g_cfg;
    }

    static std::string Trim(std::string s)
    {
        auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };

        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&](char c) { return !is_space((unsigned char)c); }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [&](char c) { return !is_space((unsigned char)c); }).base(), s.end());
        return s;
    }

    static std::string StripIniComment(std::string s)
    {
        const auto pos = s.find_first_of(";#");
        if (pos != std::string::npos) {
            s.erase(pos);
        }
        return Trim(std::move(s));
    }

    static std::vector<std::string> SplitCSV(std::string_view csv)
    {
        std::vector<std::string> out;
        std::string cur;
        for (char c : csv) {
            if (c == ',') {
                cur = StripIniComment(std::move(cur));
                if (!cur.empty()) {
                    out.push_back(std::move(cur));
                }
                cur.clear();
            }
            else {
                cur.push_back(c);
            }
        }

        cur = StripIniComment(std::move(cur));
        if (!cur.empty()) {
            out.push_back(std::move(cur));
        }
        return out;
    }

    static std::string NormalizeHexToken(std::string hex)
    {
        hex = Trim(std::move(hex));

        if (hex.rfind("FormID:", 0) == 0) {
            hex = Trim(hex.substr(6));
        }
        if (hex.rfind("0x", 0) == 0 || hex.rfind("0X", 0) == 0) {
            hex = Trim(hex.substr(2));
        }
        return hex;
    }

    static RE::FormID ParseFormSpec(const std::string& spec)
    {
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
        }
        catch (...) {
            return 0;
        }

        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data) {
            return 0;
        }

        const RE::FormID fullID = data->LookupFormID(localID, file);
        if (!fullID) {
            logger::warn("LookupFormID failed: file='{}' localID=0x{:08X}", file, localID);
        }
        return fullID;
    }

    void LoadConfig()
    {
        g_cfg = Config{};

        const auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
        const auto iniPath = std::format("Data\\SKSE\\Plugins\\{}.ini", pluginName);

        CSimpleIniA ini;
        ini.SetUnicode();
        ini.SetMultiKey();

        const auto rc = ini.LoadFile(iniPath.c_str());
        if (rc < 0) {
            logger::warn("Config not found or failed to load: {}", iniPath);
            logger::warn(
                "Using defaults (ShoveMagnitude={}, ShoveDuration={}, ShoveRetries={}, ShoveRetryDelayFrames={}, DisableInFirstPerson={})",
                g_cfg.shoveMagnitude, g_cfg.shoveDuration, g_cfg.shoveRetries, g_cfg.shoveRetryDelayFrames, g_cfg.disableInFirstPerson);
            return;
        }

        g_cfg.shoveMagnitude = static_cast<float>(ini.GetDoubleValue("General", "ShoveMagnitude", g_cfg.shoveMagnitude));
        g_cfg.shoveDuration = static_cast<float>(ini.GetDoubleValue("General", "ShoveDuration", g_cfg.shoveDuration));

        g_cfg.shoveRetries = static_cast<std::int32_t>(ini.GetLongValue("General", "ShoveRetries", g_cfg.shoveRetries));
        g_cfg.shoveRetryDelayFrames = static_cast<std::int32_t>(ini.GetLongValue("General", "ShoveRetryDelayFrames", g_cfg.shoveRetryDelayFrames));

        g_cfg.shoveInitialDelayFrames = static_cast<std::int32_t>(ini.GetLongValue("General", "ShoveInitialDelayFrames", g_cfg.shoveInitialDelayFrames));
        g_cfg.minShoveSeparationDelta = static_cast<float>(ini.GetDoubleValue("General", "MinShoveSeparationDelta", g_cfg.minShoveSeparationDelta));

        g_cfg.disableInFirstPerson = ini.GetBoolValue("General", "DisableInFirstPerson", g_cfg.disableInFirstPerson);
        g_cfg.applyCurrentMinVelocity = static_cast<float>(ini.GetDoubleValue("General", "ApplyCurrentMinVelocity", g_cfg.applyCurrentMinVelocity));
        g_cfg.minDurationScale = static_cast<float>(ini.GetDoubleValue("General", "MinDurationScale", g_cfg.minDurationScale));

        g_cfg.enforceMinSeparation = ini.GetBoolValue("General", "EnforceMinSeparation", g_cfg.enforceMinSeparation);
        g_cfg.minSeparationDistance = static_cast<float>(ini.GetDoubleValue("General", "MinSeparationDistance", g_cfg.minSeparationDistance));
        g_cfg.separationPushDuration = static_cast<float>(ini.GetDoubleValue("General", "SeparationPushDuration", g_cfg.separationPushDuration));
        g_cfg.separationMaxVelocity = static_cast<float>(ini.GetDoubleValue("General", "SeparationMaxVelocity", g_cfg.separationMaxVelocity));
        g_cfg.separationRetries = static_cast<std::int32_t>(ini.GetLongValue("General", "SeparationRetries", g_cfg.separationRetries));
        g_cfg.separationInitialDelayFrames = static_cast<std::int32_t>(ini.GetLongValue("General", "SeparationInitialDelayFrames", g_cfg.separationInitialDelayFrames));
        g_cfg.separationRetryDelayFrames = static_cast<std::int32_t>(ini.GetLongValue("General", "SeparationRetryDelayFrames", g_cfg.separationRetryDelayFrames));

        // clamps
        if (g_cfg.minSeparationDistance < 0.0f) g_cfg.minSeparationDistance = 0.0f;
        if (g_cfg.separationPushDuration < 0.01f) g_cfg.separationPushDuration = 0.01f;
        if (g_cfg.separationMaxVelocity < 0.0f) g_cfg.separationMaxVelocity = 0.0f;

        g_cfg.separationRetries = std::clamp(g_cfg.separationRetries, 0, 20);
        g_cfg.separationInitialDelayFrames = std::clamp(g_cfg.separationInitialDelayFrames, 0, 10);
        g_cfg.separationRetryDelayFrames = std::clamp(g_cfg.separationRetryDelayFrames, 1, 10);

        g_cfg.shoveInitialDelayFrames = std::clamp(g_cfg.shoveInitialDelayFrames, 0, 10);
        if (g_cfg.minShoveSeparationDelta < 0.0f) g_cfg.minShoveSeparationDelta = 0.0f;

        if (g_cfg.applyCurrentMinVelocity < 0.0f) g_cfg.applyCurrentMinVelocity = 0.0f;
        if (g_cfg.minDurationScale < 0.0f) g_cfg.minDurationScale = 0.0f;
        if (g_cfg.minDurationScale > 1.0f) g_cfg.minDurationScale = 1.0f;

        if (g_cfg.shoveRetries < 1) g_cfg.shoveRetries = 1;
        if (g_cfg.shoveRetries > 10) g_cfg.shoveRetries = 10;
        if (g_cfg.shoveRetryDelayFrames < 0) g_cfg.shoveRetryDelayFrames = 0;
        if (g_cfg.shoveRetryDelayFrames > 10) g_cfg.shoveRetryDelayFrames = 10;


        // ============================================================
        // Weapon multipliers
        // ============================================================

        // Clear any defaults populated by Config{} construction if you want ini-only behavior.
        // If you want defaults to remain when section is missing, keep the defaults and only override what is present.
        g_cfg.weaponTypeMultipliers.clear();
        g_cfg.weaponTypeKeywordMultipliers.clear();

        // "Unarmed" is a plain key in your INI.
        g_cfg.unarmedMultiplier = static_cast<float>(
            ini.GetDoubleValue("WeaponMultipliers", "Unarmed", g_cfg.unarmedMultiplier));

        // Iterate all keys under [WeaponMultipliers]
        CSimpleIniA::TNamesDepend keys;
        ini.GetAllKeys("WeaponMultipliers", keys);

        std::size_t parsed = 0;
        std::size_t resolved = 0;

        for (const auto& k : keys) {
            if (!k.pItem) {
                continue;
            }

            std::string_view key{ k.pItem };

            // Skip Unarmed since it's not a FormSpec
            if (_stricmp(key.data(), "Unarmed") == 0) {
                continue;
            }

            const char* valStr = ini.GetValue("WeaponMultipliers", k.pItem, nullptr);
            if (!valStr) {
                continue;
            }

            float mult = 1.0f;
            try {
                mult = std::stof(valStr);
            }
            catch (...) {
                logger::warn("[WeaponMultipliers] Invalid multiplier value '{}' for key '{}'", valStr, k.pItem);
                continue;
            }

            if (!(mult > 0.0f)) {
                logger::warn("[WeaponMultipliers] Multiplier must be > 0.0. Key='{}' Value={}", k.pItem, mult);
                continue;
            }

            const auto formID = ParseFormSpec(std::string(key));
            if (formID == 0) {
                logger::warn("[WeaponMultipliers] Invalid keyword spec '{}'", k.pItem);
                continue;
            }

            ++parsed;

            // Keep FormID map too (useful for debugging / reload)
            g_cfg.weaponTypeMultipliers[formID] = mult;

            // Resolve to keyword*
            auto* kw = RE::TESForm::LookupByID<RE::BGSKeyword>(formID);
            if (!kw) {
                logger::warn("[WeaponMultipliers] FormID {:08X} ('{}') did not resolve to a BGSKeyword", formID, k.pItem);
                continue;
            }

            g_cfg.weaponTypeKeywordMultipliers[kw] = mult;
            ++resolved;
        }

		// ============================================================
        // Races
		// ============================================================
        const char* allowStr = ini.GetValue("Races", "Allow", "");
        const char* denyStr = ini.GetValue("Races", "Deny", "");

        for (auto& item : SplitCSV(allowStr)) {
            if (const auto id = ParseFormSpec(item); id != 0) {
                g_cfg.allowRaces.insert(id);
            }
            else {
                logger::warn("Invalid Allow race spec: '{}'", item);
            }
        }

        for (auto& item : SplitCSV(denyStr)) {
            if (const auto id = ParseFormSpec(item); id != 0) {
                g_cfg.denyRaces.insert(id);
            }
            else {
                logger::warn("Invalid Deny race spec: '{}'", item);
            }
        }


        logger::info(
            "Config loaded: ShoveMagnitude={}, ShoveDuration={}, ShoveRetries={}, ShoveRetryDelayFrames={}, "
            "ShoveInitialDelayFrames={}, MinShoveSeparationDelta={}, DisableInFirstPerson={}, AllowRaces={}, DenyRaces={}, "
            "WeaponMults(parsed={}, resolvedKeywords={}, unarmed={})",
            g_cfg.shoveMagnitude, g_cfg.shoveDuration, g_cfg.shoveRetries, g_cfg.shoveRetryDelayFrames,
            g_cfg.shoveInitialDelayFrames, g_cfg.minShoveSeparationDelta,
            g_cfg.disableInFirstPerson, g_cfg.allowRaces.size(), g_cfg.denyRaces.size(),
            parsed, resolved, g_cfg.unarmedMultiplier);
    }
}
