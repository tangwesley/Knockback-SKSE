This SKSE plugin allows actors to apply a knockback force on melee hits.
Configurable force, duration, latency, and exceptions for certain races.

Example `KnockbackPlugin.ini`:

```ini
; =========================================
; Knockback Plugin Configuration
; =========================================

[General]
; Strength of the physics shove applied on a melee hit.
; Typical range:
;   1.0  = very subtle
;   2.5  = light knockback (recommended default)
;   4.0+ = strong shove / noticeable stagger
ShoveMagnitude = 3.5
ShoveDuration = 0.12
;   Retries for number of frames to try after a failed frame.
;   If it feels sluggish or laggy, reduce these or set to 0
ShoveRetries=3
ShoveRetryDelayFrames=1
;   No knockback to enemies in first person. Player should still get knocked back.
DisableInFirstPerson=true


[Races]
; ==========================================================
; Race filtering
;
; Format for each entry:
;   PluginName.esm|FormID
;
; Examples:
;   Skyrim.esm|00013796
;   Update.esm|00000800
;
; Notes:
; - If Allow is empty, ALL races are eligible unless denied.
; - If Allow has entries, ONLY those races are eligible
;   (deny list always wins).
; ==========================================================

; Leave Allow empty to use keyword-based humanoid detection
Allow =

; Races that should NEVER be knocked back
; (Dragons, Giants, Dwarven Automatons, etc.)
Deny =
    Skyrim.esm|00013796,    ; DragonRace
    Skyrim.esm|00013797,    ; GiantRace
    Skyrim.esm|00013798,    ; DwarvenAutomatonRace
    Skyrim.esm|00012E82,    ; DragonPriestRace
    Skyrim.esm|000131F5     ; MammothRace
```

========================================================================================================

## License and Commercial Use

This repository is provided for **non-commercial use only**.

Commercial use is **explicitly prohibited**, including but not limited to:
- Selling the software or source code
- Offering it as part of a paid product, service, or subscription
- Hosting it as a paid SaaS or API
- Charging fees for access, support, or distribution

