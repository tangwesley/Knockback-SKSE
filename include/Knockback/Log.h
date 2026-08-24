#pragma once

namespace Knockback
{
    void SetupLog();

    // Verbose logging = per-hit trace spam. Off by default; enabled from config.
    void SetVerboseLogging(bool a_enabled);
}
