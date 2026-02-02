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
;   3.5  = light knockback (recommended default)
;   4.0+ = strong shove / noticeable stagger
ShoveMagnitude = 3.5
ShoveDuration = 0.12
;   Retries for number of frames to try after a failed frame.
;   If it feels sluggish or laggy, reduce these or set to 0
ShoveRetries=5
ShoveRetryDelayFrames=1
;   No knockback to enemies in first person. Player should still get knocked back.
DisableInFirstPerson=true
; Set for scaling when ShoveMagnitude is too low. Here's the formula.
; peakV = max(ShoveMagnitude, ApplyCurrentMinVelocity)
; scaledDuration = ShoveDuration * (ShoveMagnitude / peakV)
; scaledDuration = max(scaledDuration, configDuration * minDurationScale)
ApplyCurrentMinVelocity=4.0
MinDurationScale=0.15
; Minimum separation enforcement (push aggressor back if too close after shove, useful if enemy is against a wall)
EnforceMinSeparation=true
MinSeparationDistance=80.0
SeparationPushDuration=0.08
SeparationMaxVelocity=12.0
SeparationRetries=6
SeparationInitialDelayFrames=2
SeparationRetryDelayFrames=1
; You don't need to touch these if you don't know what they do. They are for physics consistency checks.
ShoveInitialDelayFrames=1
MinShoveSeparationDelta=8.0

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
; ==========================================================

; Leave Allow empty to use keyword-based humanoid detection (only playable races and draugr)
Allow =
	Skyrim.esm|00109C7C,    ; Fox
    Skyrim.esm|00108272,    ; Breton
    Skyrim.esm|0010760A,    ; Nord
    Skyrim.esm|00106C10,    ; Wolf
    Skyrim.esm|00104F45,    ; Deer
    Skyrim.esm|000F905F,    ; Dog
    Skyrim.esm|000F71DC,    ; Draugr
    Skyrim.esm|000F3903,    ; Spriggan
    Skyrim.esm|000EBE18,    ; Dragon Priest
    Skyrim.esm|000EB872,    ; Skeleton
    Skyrim.esm|000F1AC4,    ; Dog
    Skyrim.esm|000F1182,    ; Wisp
    Skyrim.esm|000DE505,    ; Horse
    Skyrim.esm|000CD657,    ; Dog
    Skyrim.esm|000A919D,    ; Rabbit
    Skyrim.esm|00097A3D,    ; Breton
    Skyrim.esm|0007EAF3,    ; Nord
    Skyrim.esm|0006DC99,    ; Rabbit
    Skyrim.esm|000CF89B,    ; Deer
    Skyrim.esm|000CDD84,    ; Werewolf
    Skyrim.esm|000C3EDF,    ; Skeever
    Skyrim.esm|000B9FD7,    ; Skeleton
    Skyrim.esm|000A82BA,    ; Old People Race
    Skyrim.esm|000A82B9,    ; Orc
    Skyrim.esm|00088884,    ; Wood Elf
    Skyrim.esm|00088846,    ; Redguard
    Skyrim.esm|00088845,    ; Khajiit
    Skyrim.esm|00088844,    ; Imperial
    Skyrim.esm|00088840,    ; High Elf
    Skyrim.esm|0008883D,    ; Dark Elf
    Skyrim.esm|0008883C,    ; Breton
    Skyrim.esm|0008883A,    ; Argonian
    Skyrim.esm|00088794,    ; Nord
    Skyrim.esm|000BA545,    ; MudCrab
    Skyrim.esm|000B7998,    ; Skeleton
    Skyrim.esm|000A5601,    ; Chaurus
    Skyrim.esm|0009AA44,    ; Witchlight
    Skyrim.esm|0009AA3C,    ; Witchlight
    Skyrim.esm|0006FC4A,    ; Goat
    Skyrim.esm|00067CD8,    ; Old People Race
    Skyrim.esm|00053477,    ; Frostbite Spider
    Skyrim.esm|0004E785,    ; HighlandCow
    Skyrim.esm|0004E507,    ; Frostbite Spider
    Skyrim.esm|0002C65C,    ; Breton
    Skyrim.esm|0002C65B,    ; Nord
    Skyrim.esm|0002C65A,    ; Redguard
    Skyrim.esm|0002C659,    ; Imperial
    Skyrim.esm|00013749,    ; Wood Elf
    Skyrim.esm|00013748,    ; Redguard
    Skyrim.esm|00013747,    ; Orc
    Skyrim.esm|00013746,    ; Nord
    Skyrim.esm|00013745,    ; Khajiit
    Skyrim.esm|00013744,    ; Imperial
    Skyrim.esm|00013743,    ; High Elf
    Skyrim.esm|00013742,    ; Dark Elf
    Skyrim.esm|00013741,    ; Breton
    Skyrim.esm|00013740,    ; Argonian
    Skyrim.esm|0001320A,    ; Wolf
    Skyrim.esm|00013209,    ; Witchlight
    Skyrim.esm|00013208,    ; Wisp
    Skyrim.esm|00013206,    ; Snow Troll
    Skyrim.esm|00013205,    ; Troll
    Skyrim.esm|00013204,    ; Spriggan
    Skyrim.esm|00013203,    ; Slaughterfish
    Skyrim.esm|00013202,    ; Snowy Sabre Cat
    Skyrim.esm|00013201,    ; Skeever
    Skyrim.esm|00013200,    ; Sabre Cat
    Skyrim.esm|000131FE,    ; Ice Wraith
    Skyrim.esm|000131FD,    ; Horse
    Skyrim.esm|000131FC,    ; Horker
    Skyrim.esm|000131FB,    ; Hagraven
    Skyrim.esm|000131FA,    ; Goat
    Skyrim.esm|000131F9,    ; Giant
    Skyrim.esm|000131F8,    ; Frostbite Spider
    Skyrim.esm|000131F7,    ; Storm Atronach
    Skyrim.esm|000131F6,    ; Frost Atronach
    Skyrim.esm|000131F5,    ; Flame Atronach
    Skyrim.esm|000131F4,    ; Falmer
    Skyrim.esm|000131F3,    ; Dwarven Spider
    Skyrim.esm|000131F2,    ; Dwarven Sphere
    Skyrim.esm|000131F0,    ; Dremora
    Skyrim.esm|000131EF,    ; Dragon Priest
    Skyrim.esm|000131EE,    ; Dog
    Skyrim.esm|000131ED,    ; Deer
    Skyrim.esm|000131EB,    ; Chaurus
    Skyrim.esm|000131E9,    ; Snow Bear
    Skyrim.esm|000131E8,    ; Cave Bear
    Skyrim.esm|000131E7,    ; Bear
    Skyrim.esm|00000D53,    ; Draugr
    Update.esm|0010760A,    ; Nord
    Update.esm|000DE505,    ; Horse
    Update.esm|000131FD,    ; Horse
    Update.esm|00002F93,    ; Dog
    Update.esm|00002F74,    ; Dog
    Update.esm|00013740,    ; Argonian
    Update.esm|0008883A,    ; Argonian
    Update.esm|00013741,    ; Breton
    Update.esm|0008883C,    ; Breton
    Update.esm|00013742,    ; Dark Elf
    Update.esm|0008883D,    ; Dark Elf
    Update.esm|00013743,    ; High Elf
    Update.esm|00088840,    ; High Elf
    Update.esm|00013744,    ; Imperial
    Update.esm|00088844,    ; Imperial
    Update.esm|00013745,    ; Khajiit
    Update.esm|00088845,    ; Khajiit
    Update.esm|00013746,    ; Nord
    Update.esm|00088794,    ; Nord
    Update.esm|00013747,    ; Orc
    Update.esm|000A82B9,    ; Orc
    Update.esm|00013748,    ; Redguard
    Update.esm|00088846,    ; Redguard
    Update.esm|00013749,    ; Wood Elf
    Update.esm|00088884,    ; Wood Elf
    Dawnguard.esm|000122B7,    ; Dog
    Dawnguard.esm|000EB872,    ; Skeleton
    Dawnguard.esm|00088845,    ; Khajiit
    Dawnguard.esm|00010D00,    ; Gargoyle
    Dawnguard.esm|0000E88A,    ; Nord
    Dawnguard.esm|00003D02,    ; Deathhound
    Dawnguard.esm|00003D01,    ; Dog
    Dawnguard.esm|00002AE0,    ; Witchlight
    Dawnguard.esm|0001AACC,    ; Falmer
    Dawnguard.esm|00019FD3,    ; Skeleton
    Dawnguard.esm|00019D86,    ; Gargoyle
    Dawnguard.esm|000CDD84,    ; Werewolf
    Dawnguard.esm|00018B36,    ; Dog
    Dawnguard.esm|00018B33,    ; Dog
    Dawnguard.esm|000F3903,    ; Spriggan
    Dawnguard.esm|0009AA44,    ; Witchlight
    Dawnguard.esm|00013204,    ; Spriggan
    Dawnguard.esm|00015C34,    ; The Forgemaster
    Dawnguard.esm|00015136,    ; Chaurus
    Dawnguard.esm|00013B77,    ; Spriggan
    Dawnguard.esm|000117F5,    ; Troll
    Dawnguard.esm|000117F4,    ; Snow Troll
    Dawnguard.esm|0000D0B6,    ; Sabre Cat
    Dawnguard.esm|0000D0B2,    ; Deer
    Dawnguard.esm|0000C5F0,    ; Dog
    Dawnguard.esm|0000A94B,    ; Skeleton
    Dawnguard.esm|0000A2C6,    ; Gargoyle
    Dawnguard.esm|0000894D,    ; Draugr
    Dawnguard.esm|00007AF3,    ; Draugr
    Dawnguard.esm|000A82B9,    ; Orc
    Dawnguard.esm|00088884,    ; Wood Elf
    Dawnguard.esm|00088846,    ; Redguard
    Dawnguard.esm|00088844,    ; Imperial
    Dawnguard.esm|00088840,    ; High Elf
    Dawnguard.esm|0008883D,    ; Dark Elf
    Dawnguard.esm|0008883C,    ; Breton
    Dawnguard.esm|00088794,    ; Nord
    Dawnguard.esm|00006AFA,    ; Skeleton
    Dawnguard.esm|000051FB,    ; Chaurusflyer
    Dawnguard.esm|00004D31,    ; Nord
    Dawnguard.esm|0000483B,    ; Draugr
    Dawnguard.esm|0000377D,    ; High Elf
    Dawnguard.esm|000131F4,    ; Falmer
    Dawnguard.esm|0000283A,    ; Vampire Lord
    Dawnguard.esm|000023E2,    ; Draugr
    Dragonborn.esm|0003CECB,    ; Skeleton
    Dragonborn.esm|0003CA97,    ; Nord
    Dragonborn.esm|0003911A,    ; Dragon Priest
    Dragonborn.esm|00035538,    ; Dremora
    Dragonborn.esm|0002B014,    ; Dwarven Sphere
    Dragonborn.esm|0002A6FD,    ; Draugr
    Dragonborn.esm|00029EFC,    ; Witchlight
    Dragonborn.esm|00029EE7,    ; Ice Wraith
    Dragonborn.esm|00028580,    ; Netch
    Dragonborn.esm|00027BFC,    ; Storm Atronach
    Dragonborn.esm|00027483,    ; Frostbite Spider
    Dragonborn.esm|00024038,    ; DLC2BoarRace
    Dragonborn.esm|0001FEB8,    ; Netch
    Dragonborn.esm|0001E17B,    ; Werewolf
    Dragonborn.esm|0001DCB9,    ; HMDaedra
    Dragonborn.esm|0001B658,    ; Scrib
    Dragonborn.esm|0001B647,    ; MudCrab
    Dragonborn.esm|0001B644,    ; Spriggan
    Dragonborn.esm|0001B637,    ; Draugr
    Dragonborn.esm|0001A50A,    ; DLC2ThirskRieklingRace
    Dragonborn.esm|00017F44,    ; DLC2RieklingRace
    Dragonborn.esm|000179CF,    ; DLC2MountedRieklingRace
    Dragonborn.esm|00014449     ; Frostbite Spider

; Races that should NEVER be knocked back
; (Dragons, Giants, Dwarven Automatons, etc.)
Deny =
    Skyrim.esm|00012E82,    ; DragonRace
    Skyrim.esm|000131F9,    ; GiantRace
    Skyrim.esm|000CAE13,    ; Giant
    Skyrim.esm|000131F1,    ; DwarvenCenurionRace
    Skyrim.esm|000131FF,    ; MammothRace
    Skyrim.esm|001052A3,    ; Dragon Race
    Skyrim.esm|000E7713,    ; Dragon Race
    Update.esm|001052A3,    ; Dragon Race
    Dawnguard.esm|000117DE,    ; Dragon Race
    Dragonborn.esm|0002C88C,    ; Dragon Race
    Dragonborn.esm|0002C88B,    ; Dragon Race
    Dragonborn.esm|0001F98F,    ; Spectral Dragon
    Dragonborn.esm|0001CAD8,    ; Ghost Giant
    Dragonborn.esm|001052A3,    ; Dragon Race
    Dragonborn.esm|000E7713,    ; Dragon Race
    Dragonborn.esm|00012E82,    ; Dragon Race
    Dragonborn.esm|00014495,    ; Giant

```

========================================================================================================

## License and Commercial Use

This repository is provided for **non-commercial use only**.

Commercial use is **explicitly prohibited**, including but not limited to:
- Selling the software or source code
- Offering it as part of a paid product, service, or subscription
- Hosting it as a paid SaaS or API
- Charging fees for access, support, or distribution

