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
#include <filesystem>


namespace logger = SKSE::log;

namespace Knockback
{
    static Config g_cfg{};
    namespace fs = std::filesystem;

    static std::mutex g_cfgMutex{};
    static fs::file_time_type g_lastWriteTime{};
    static std::string g_lastPath{};
    static std::chrono::steady_clock::time_point g_lastCheck{}; 
    static fs::file_time_type g_lastLegacyWriteTime{};
    static fs::file_time_type g_lastMcmWriteTime{};
    static bool g_lastLegacyExists{ false };
    static bool g_lastMcmExists{ false };

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

    static bool LoadIniFile(CSimpleIniA& ini, const std::string& path)
    {
        ini.Reset();
        ini.SetUnicode();
        ini.SetMultiKey();
        return ini.LoadFile(path.c_str()) >= 0;
    }

    static std::string GetMcmSettingsPath()
    {
        constexpr std::string_view kMcmModName = "KnockbackMCM";
        return std::format("Data\\MCM\\Settings\\{}.ini", kMcmModName);
    }

    static std::string GetLegacyPath(std::string_view pluginName)
    {
        // Your “always load” file name:
        // If you want it hard-coded to KnockbackPlugin.ini, do this:
        return "Data\\SKSE\\Plugins\\KnockbackPlugin.ini";

        // Or if you want “same name as dll” legacy behavior:
        // return std::format("Data\\SKSE\\Plugins\\{}.ini", pluginName);
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
        Config tmp{};

        const auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
        const auto legacyPath = GetLegacyPath(pluginName);
        const auto mcmPath = GetMcmSettingsPath();

        CSimpleIniA legacyIni;
        CSimpleIniA mcmIni;

        const bool haveLegacy = fs::exists(legacyPath) && LoadIniFile(legacyIni, legacyPath);
        const bool haveMcm = fs::exists(mcmPath) && LoadIniFile(mcmIni, mcmPath);

        if (!haveLegacy) {
            logger::warn("Legacy config not found or failed to load: {}", legacyPath);
            logger::warn("Using compiled defaults for advanced lists (WeaponMultipliers/Races).");
        }
        else {
            logger::info("Loaded legacy config: {}", legacyPath);
        }

        if (haveMcm) {
            logger::info("Loaded MCM settings: {}", mcmPath);
        }
        else {
            logger::info("MCM settings not present yet: {}", mcmPath);
        }

        // -----------------------------
        // 1) Apply LEGACY (base layer)
        // -----------------------------
        auto& iniBase = haveLegacy ? legacyIni : mcmIni; // fallback if you *only* have MCM (rare)

        // General from legacy (non-prefixed keys)
        if (haveLegacy) {
            tmp.shoveMagnitude = static_cast<float>(legacyIni.GetDoubleValue("General", "ShoveMagnitude", tmp.shoveMagnitude));
            tmp.shoveDuration = static_cast<float>(legacyIni.GetDoubleValue("General", "ShoveDuration", tmp.shoveDuration));

            tmp.shoveRetries = static_cast<std::int32_t>(legacyIni.GetLongValue("General", "ShoveRetries", tmp.shoveRetries));
            tmp.shoveRetryDelayFrames = static_cast<std::int32_t>(legacyIni.GetLongValue("General", "ShoveRetryDelayFrames", tmp.shoveRetryDelayFrames));

            tmp.shoveInitialDelayFrames = static_cast<std::int32_t>(legacyIni.GetLongValue("General", "ShoveInitialDelayFrames", tmp.shoveInitialDelayFrames));
            tmp.minShoveSeparationDelta = static_cast<float>(legacyIni.GetDoubleValue("General", "MinShoveSeparationDelta", tmp.minShoveSeparationDelta));

            tmp.disableInFirstPerson = legacyIni.GetBoolValue("General", "DisableInFirstPerson", tmp.disableInFirstPerson);
            tmp.applyCurrentMinVelocity = static_cast<float>(legacyIni.GetDoubleValue("General", "ApplyCurrentMinVelocity", tmp.applyCurrentMinVelocity));
            tmp.minDurationScale = static_cast<float>(legacyIni.GetDoubleValue("General", "MinDurationScale", tmp.minDurationScale));

            tmp.enforceMinSeparation = legacyIni.GetBoolValue("General", "EnforceMinSeparation", tmp.enforceMinSeparation);
            tmp.minSeparationDistance = static_cast<float>(legacyIni.GetDoubleValue("General", "MinSeparationDistance", tmp.minSeparationDistance));
            tmp.separationPushDuration = static_cast<float>(legacyIni.GetDoubleValue("General", "SeparationPushDuration", tmp.separationPushDuration));
            tmp.separationMaxVelocity = static_cast<float>(legacyIni.GetDoubleValue("General", "SeparationMaxVelocity", tmp.separationMaxVelocity));
            tmp.separationRetries = static_cast<std::int32_t>(legacyIni.GetLongValue("General", "SeparationRetries", tmp.separationRetries));
            tmp.separationInitialDelayFrames = static_cast<std::int32_t>(legacyIni.GetLongValue("General", "SeparationInitialDelayFrames", tmp.separationInitialDelayFrames));
            tmp.separationRetryDelayFrames = static_cast<std::int32_t>(legacyIni.GetLongValue("General", "SeparationRetryDelayFrames", tmp.separationRetryDelayFrames));
        }

        // Weapon multipliers + races ALWAYS from legacy
        std::size_t parsed = 0;
        std::size_t resolved = 0;

        if (haveLegacy) {
            tmp.weaponTypeMultipliers.clear();
            tmp.weaponTypeKeywordMultipliers.clear();

            // Unarmed base from legacy (can be overridden later by MCM)
            tmp.unarmedMultiplier = static_cast<float>(
                legacyIni.GetDoubleValue("WeaponMultipliers", "Unarmed", tmp.unarmedMultiplier));
            tmp.powerAttackMultiplier = static_cast<float>(
                legacyIni.GetDoubleValue("WeaponMultipliers", "PowerAttack", tmp.powerAttackMultiplier));

            CSimpleIniA::TNamesDepend keys;
            legacyIni.GetAllKeys("WeaponMultipliers", keys);

            for (const auto& k : keys) {
                if (!k.pItem) continue;

                std::string_view key{ k.pItem };
                if (_stricmp(key.data(), "Unarmed") == 0) continue;
                if (_stricmp(key.data(), "PowerAttack") == 0) continue;

                const char* valStr = legacyIni.GetValue("WeaponMultipliers", k.pItem, nullptr);
                if (!valStr) continue;

                float mult = 1.0f;
                try { mult = std::stof(valStr); }
                catch (...) { continue; }

                if (!(mult > 0.0f)) continue;

                const auto formID = ParseFormSpec(std::string(key));
                if (!formID) continue;

                ++parsed;
                tmp.weaponTypeMultipliers[formID] = mult;

                auto* kw = RE::TESForm::LookupByID<RE::BGSKeyword>(formID);
                if (!kw) continue;

                tmp.weaponTypeKeywordMultipliers[kw] = mult;
                ++resolved;
            }

            // Races
            {
                CSimpleIniA::TNamesDepend allowVals;
                CSimpleIniA::TNamesDepend denyVals;

                legacyIni.GetAllValues("Races", "Allow", allowVals);
                legacyIni.GetAllValues("Races", "Deny", denyVals);

                for (const auto& v : allowVals) {
                    if (!v.pItem) continue;
                    if (const auto id = ParseFormSpec(v.pItem); id != 0) tmp.allowRaces.insert(id);
                }
                for (const auto& v : denyVals) {
                    if (!v.pItem) continue;
                    if (const auto id = ParseFormSpec(v.pItem); id != 0) tmp.denyRaces.insert(id);
                }
            }
        }

        // -----------------------------
        // 2) Apply MCM overrides (General + Unarmed ONLY)
        // -----------------------------
        if (haveMcm) {
            auto getFloat = [&](const char* section, const char* keyNew, const char* keyOld, float cur) -> float {
                if (mcmIni.KeyExists(section, keyNew)) return static_cast<float>(mcmIni.GetDoubleValue(section, keyNew, cur));
                if (mcmIni.KeyExists(section, keyOld)) return static_cast<float>(mcmIni.GetDoubleValue(section, keyOld, cur));
                return cur;
                };
            auto getInt = [&](const char* section, const char* keyNew, const char* keyOld, std::int32_t cur) -> std::int32_t {
                if (mcmIni.KeyExists(section, keyNew)) return static_cast<std::int32_t>(mcmIni.GetLongValue(section, keyNew, cur));
                if (mcmIni.KeyExists(section, keyOld)) return static_cast<std::int32_t>(mcmIni.GetLongValue(section, keyOld, cur));
                return cur;
                };
            auto getBool = [&](const char* section, const char* keyNew, const char* keyOld, bool cur) -> bool {
                if (mcmIni.KeyExists(section, keyNew)) return mcmIni.GetBoolValue(section, keyNew, cur);
                if (mcmIni.KeyExists(section, keyOld)) return mcmIni.GetBoolValue(section, keyOld, cur);
                return cur;
                };

            tmp.shoveMagnitude = getFloat("General", "fShoveMagnitude", "ShoveMagnitude", tmp.shoveMagnitude);
            tmp.shoveDuration = getFloat("General", "fShoveDuration", "ShoveDuration", tmp.shoveDuration);

            tmp.shoveRetries = getInt("General", "iShoveRetries", "ShoveRetries", tmp.shoveRetries);
            tmp.shoveRetryDelayFrames = getInt("General", "iShoveRetryDelayFrames", "ShoveRetryDelayFrames", tmp.shoveRetryDelayFrames);

            tmp.shoveInitialDelayFrames = getInt("General", "iShoveInitialDelayFrames", "ShoveInitialDelayFrames", tmp.shoveInitialDelayFrames);
            tmp.minShoveSeparationDelta = getFloat("General", "fMinShoveSeparationDelta", "MinShoveSeparationDelta", tmp.minShoveSeparationDelta);

            tmp.disableInFirstPerson = getBool("General", "bDisableInFirstPerson", "DisableInFirstPerson", tmp.disableInFirstPerson);
            tmp.applyCurrentMinVelocity = getFloat("General", "fApplyCurrentMinVelocity", "ApplyCurrentMinVelocity", tmp.applyCurrentMinVelocity);
            tmp.minDurationScale = getFloat("General", "fMinDurationScale", "MinDurationScale", tmp.minDurationScale);

            tmp.enforceMinSeparation = getBool("General", "bEnforceMinSeparation", "EnforceMinSeparation", tmp.enforceMinSeparation);
            tmp.minSeparationDistance = getFloat("General", "fMinSeparationDistance", "MinSeparationDistance", tmp.minSeparationDistance);
            tmp.separationPushDuration = getFloat("General", "fSeparationPushDuration", "SeparationPushDuration", tmp.separationPushDuration);
            tmp.separationMaxVelocity = getFloat("General", "fSeparationMaxVelocity", "SeparationMaxVelocity", tmp.separationMaxVelocity);
            tmp.separationRetries = getInt("General", "iSeparationRetries", "SeparationRetries", tmp.separationRetries);
            tmp.separationInitialDelayFrames = getInt("General", "iSeparationInitialDelayFrames", "SeparationInitialDelayFrames", tmp.separationInitialDelayFrames);
            tmp.separationRetryDelayFrames = getInt("General", "iSeparationRetryDelayFrames", "SeparationRetryDelayFrames", tmp.separationRetryDelayFrames);

            // Unarmed and PowerAttack override ONLY
            if (mcmIni.KeyExists("WeaponMultipliers", "fUnarmed")) {
                tmp.unarmedMultiplier = static_cast<float>(
                    mcmIni.GetDoubleValue("WeaponMultipliers", "fUnarmed", tmp.unarmedMultiplier));
            }
            else if (mcmIni.KeyExists("WeaponMultipliers", "Unarmed")) {
                tmp.unarmedMultiplier = static_cast<float>(
                    mcmIni.GetDoubleValue("WeaponMultipliers", "Unarmed", tmp.unarmedMultiplier));
            }
            if (mcmIni.KeyExists("WeaponMultipliers", "fPowerAttack")) {
                tmp.powerAttackMultiplier = static_cast<float>(
                    mcmIni.GetDoubleValue("WeaponMultipliers", "fPowerAttack", tmp.unarmedMultiplier));
            }
            else if (mcmIni.KeyExists("WeaponMultipliers", "PowerAttack")) {
                tmp.powerAttackMultiplier = static_cast<float>(
                    mcmIni.GetDoubleValue("WeaponMultipliers", "PowerAttack", tmp.unarmedMultiplier));
            }
        }

        // clamps (keep yours)
        if (tmp.shoveRetries < 0) tmp.shoveRetries = 0;
        if (tmp.shoveRetries > 10) tmp.shoveRetries = 10;

        // Publish
        g_cfg = std::move(tmp);

        // Watcher state: you should watch BOTH files (see next note)
        g_lastPath.clear(); // optional: stop using single-path watcher
        logger::info("Config loaded. Legacy={} MCM={} WeaponMults(parsed={}, resolvedKeywords={}, unarmed={}, powerAttack={})",
            haveLegacy ? legacyPath : "(none)",
            haveMcm ? mcmPath : "(none)",
            parsed, resolved, g_cfg.unarmedMultiplier, g_cfg.powerAttackMultiplier);
    }

    void MaybeReloadConfig()
    {
        using namespace std::chrono;

        // Throttle: check at most once per second
        const auto now = steady_clock::now();
        if (now - g_lastCheck < 1s) {
            return;
        }
        g_lastCheck = now;

        const auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();

        const auto legacyPath = GetLegacyPath(pluginName);
        const auto mcmPath = GetMcmSettingsPath();

        auto getState = [](const std::string& path, fs::file_time_type& outWt, std::error_code& outEc) -> bool {
            outEc.clear();
            if (!fs::exists(path, outEc) || outEc) {
                outWt = fs::file_time_type{};
                return false;
            }
            outWt = fs::last_write_time(path, outEc);
            if (outEc) {
                outWt = fs::file_time_type{};
                return false;
            }
            return true;
            };

        fs::file_time_type legacyWt{};
        fs::file_time_type mcmWt{};
        std::error_code ecLegacy{};
        std::error_code ecMcm{};

        const bool legacyExists = getState(legacyPath, legacyWt, ecLegacy);
        const bool mcmExists = getState(mcmPath, mcmWt, ecMcm);

        // If the filesystem is erroring, fail silent; don't spam logs.
        // (But still allow reload if the other file is OK.)
        if (ecLegacy && ecMcm) {
            return;
        }

        // First run initialization: capture current state, don’t reload.
        // (Assumes LoadConfig() already ran during startup.)
        static bool initialized = false;
        if (!initialized) {
            g_lastLegacyExists = legacyExists;
            g_lastMcmExists = mcmExists;
            g_lastLegacyWriteTime = legacyWt;
            g_lastMcmWriteTime = mcmWt;
            initialized = true;
            return;
        }

        // Detect appearance/disappearance or timestamp changes.
        const bool legacyChanged =
            (legacyExists != g_lastLegacyExists) ||
            (legacyExists && legacyWt != g_lastLegacyWriteTime);

        const bool mcmChanged =
            (mcmExists != g_lastMcmExists) ||
            (mcmExists && mcmWt != g_lastMcmWriteTime);

        if (!legacyChanged && !mcmChanged) {
            return;
        }

        // Update stored state before reload to avoid loops if LoadConfig touches files.
        g_lastLegacyExists = legacyExists;
        g_lastMcmExists = mcmExists;
        g_lastLegacyWriteTime = legacyWt;
        g_lastMcmWriteTime = mcmWt;

        LoadConfig();

        if (legacyChanged && mcmChanged) {
            logger::info("Config reloaded (legacy + MCM changed)");
        }
        else if (legacyChanged) {
            logger::info("Config reloaded (legacy changed): {}", legacyPath);
        }
        else {
            logger::info("Config reloaded (MCM changed): {}", mcmPath);
        }
    }
}
