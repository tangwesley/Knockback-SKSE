// plugin.cpp
// Entry point only: SKSE init, logging, and messaging registration.

#include "SKSE/SKSE.h"
#include <Knockback/Log.h>
#include <Knockback/HitSink.h>

namespace logger = SKSE::log;

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);
    Knockback::SetupLog();

    logger::info("KnockbackPlugin loaded (logger OK)");

    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging) {
        logger::critical("Messaging interface not available");
        return false;
    }

    messaging->RegisterListener(Knockback::OnSKSEMessage);
    logger::info("Registered SKSE messaging listener");
    return true;
}