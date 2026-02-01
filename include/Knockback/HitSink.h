#pragma once

#include "SKSE/SKSE.h"

namespace Knockback
{
    void RegisterHitSink();
    void OnSKSEMessage(SKSE::MessagingInterface::Message* msg);
}
