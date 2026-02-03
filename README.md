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

[WeaponMultipliers]
; Keyword FormID = multiplier
Skyrim.esm|0001E711 = 1.00   ; WeapTypeSword
Skyrim.esm|0006D931 = 1.00   ; WeapTypeGreatsword
Skyrim.esm|0001E713 = 0.85   ; WeapTypeDagger
Skyrim.esm|0001E712 = 1.00	 ; WeapTypeWarAxe
Skyrim.esm|0006D932 = 1.00   ; WeapTypeBattleAxe
Skyrim.esm|0001E714 = 1.00   ; WeapTypeMace
Skyrim.esm|0006D930 = 1.00   ; WeapTypeWarhammer
Unarmed = 0.85

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
[Races]
Allow=Skyrim.esm|00109C7C ; Fox
Allow=Skyrim.esm|00108272 ; Breton
Allow=Skyrim.esm|0010760A ; Nord
Allow=Skyrim.esm|00106C10 ; Wolf
Allow=Skyrim.esm|00104F45 ; Deer
Allow=Skyrim.esm|000F905F ; Dog
Allow=Skyrim.esm|000F71DC ; Draugr
Allow=Skyrim.esm|000F3903 ; Spriggan
Allow=Skyrim.esm|000EBE18 ; Dragon Priest
Allow=Skyrim.esm|000EB872 ; Skeleton
Allow=Skyrim.esm|000F1AC4 ; Dog
Allow=Skyrim.esm|000F1182 ; Wisp
Allow=Skyrim.esm|000DE505 ; Horse
Allow=Skyrim.esm|000CD657 ; Dog
Allow=Skyrim.esm|000A919D ; Rabbit
Allow=Skyrim.esm|00097A3D ; Breton
Allow=Skyrim.esm|0007EAF3 ; Nord
Allow=Skyrim.esm|0006DC99 ; Rabbit
Allow=Skyrim.esm|000CF89B ; Deer
Allow=Skyrim.esm|000CDD84 ; Werewolf
Allow=Skyrim.esm|000C3EDF ; Skeever
Allow=Skyrim.esm|000B9FD7 ; Skeleton
Allow=Skyrim.esm|000A82BA ; Old People Race
Allow=Skyrim.esm|000A82B9 ; Orc
Allow=Skyrim.esm|00088884 ; Wood Elf
Allow=Skyrim.esm|00088846 ; Redguard
Allow=Skyrim.esm|00088845 ; Khajiit
Allow=Skyrim.esm|00088844 ; Imperial
Allow=Skyrim.esm|00088840 ; High Elf
Allow=Skyrim.esm|0008883D ; Dark Elf
Allow=Skyrim.esm|0008883C ; Breton
Allow=Skyrim.esm|0008883A ; Argonian
Allow=Skyrim.esm|00088794 ; Nord
Allow=Skyrim.esm|000BA545 ; MudCrab
Allow=Skyrim.esm|000B7998 ; Skeleton
Allow=Skyrim.esm|000A5601 ; Chaurus
Allow=Skyrim.esm|0009AA44 ; Witchlight
Allow=Skyrim.esm|0009AA3C ; Witchlight
Allow=Skyrim.esm|0006FC4A ; Goat
Allow=Skyrim.esm|00067CD8 ; Old People Race
Allow=Skyrim.esm|00053477 ; Frostbite Spider
Allow=Skyrim.esm|0004E785 ; HighlandCow
Allow=Skyrim.esm|0004E507 ; Frostbite Spider
Allow=Skyrim.esm|0002C65C ; Breton
Allow=Skyrim.esm|0002C65B ; Nord
Allow=Skyrim.esm|0002C65A ; Redguard
Allow=Skyrim.esm|0002C659 ; Imperial
Allow=Skyrim.esm|00013749 ; Wood Elf
Allow=Skyrim.esm|00013748 ; Redguard
Allow=Skyrim.esm|00013747 ; Orc
Allow=Skyrim.esm|00013746 ; Nord
Allow=Skyrim.esm|00013745 ; Khajiit
Allow=Skyrim.esm|00013744 ; Imperial
Allow=Skyrim.esm|00013743 ; High Elf
Allow=Skyrim.esm|00013742 ; Dark Elf
Allow=Skyrim.esm|00013741 ; Breton
Allow=Skyrim.esm|00013740 ; Argonian
Allow=Skyrim.esm|0001320A ; Wolf
Allow=Skyrim.esm|00013209 ; Witchlight
Allow=Skyrim.esm|00013208 ; Wisp
Allow=Skyrim.esm|00013206 ; Snow Troll
Allow=Skyrim.esm|00013205 ; Troll
Allow=Skyrim.esm|00013204 ; Spriggan
Allow=Skyrim.esm|00013203 ; Slaughterfish
Allow=Skyrim.esm|00013202 ; Snowy Sabre Cat
Allow=Skyrim.esm|00013201 ; Skeever
Allow=Skyrim.esm|00013200 ; Sabre Cat
Allow=Skyrim.esm|000131FE ; Ice Wraith
Allow=Skyrim.esm|000131FD ; Horse
Allow=Skyrim.esm|000131FC ; Horker
Allow=Skyrim.esm|000131FB ; Hagraven
Allow=Skyrim.esm|000131FA ; Goat
Allow=Skyrim.esm|000131F9 ; Giant
Allow=Skyrim.esm|000131F8 ; Frostbite Spider
Allow=Skyrim.esm|000131F7 ; Storm Atronach
Allow=Skyrim.esm|000131F6 ; Frost Atronach
Allow=Skyrim.esm|000131F5 ; Flame Atronach
Allow=Skyrim.esm|000131F4 ; Falmer
Allow=Skyrim.esm|000131F3 ; Dwarven Spider
Allow=Skyrim.esm|000131F2 ; Dwarven Sphere
Allow=Skyrim.esm|000131F0 ; Dremora
Allow=Skyrim.esm|000131EF ; Dragon Priest
Allow=Skyrim.esm|000131EE ; Dog
Allow=Skyrim.esm|000131ED ; Deer
Allow=Skyrim.esm|000131EB ; Chaurus
Allow=Skyrim.esm|000131E9 ; Snow Bear
Allow=Skyrim.esm|000131E8 ; Cave Bear
Allow=Skyrim.esm|000131E7 ; Bear
Allow=Skyrim.esm|00000D53 ; Draugr
Allow=Update.esm|0010760A ; Nord
Allow=Update.esm|000DE505 ; Horse
Allow=Update.esm|000131FD ; Horse
Allow=Update.esm|00002F93 ; Dog
Allow=Update.esm|00002F74 ; Dog
Allow=Update.esm|00013740 ; Argonian
Allow=Update.esm|0008883A ; Argonian
Allow=Update.esm|00013741 ; Breton
Allow=Update.esm|0008883C ; Breton
Allow=Update.esm|00013742 ; Dark Elf
Allow=Update.esm|0008883D ; Dark Elf
Allow=Update.esm|00013743 ; High Elf
Allow=Update.esm|00088840 ; High Elf
Allow=Update.esm|00013744 ; Imperial
Allow=Update.esm|00088844 ; Imperial
Allow=Update.esm|00013745 ; Khajiit
Allow=Update.esm|00088845 ; Khajiit
Allow=Update.esm|00013746 ; Nord
Allow=Update.esm|00088794 ; Nord
Allow=Update.esm|00013747 ; Orc
Allow=Update.esm|000A82B9 ; Orc
Allow=Update.esm|00013748 ; Redguard
Allow=Update.esm|00088846 ; Redguard
Allow=Update.esm|00013749 ; Wood Elf
Allow=Update.esm|00088884 ; Wood Elf
Allow=Dawnguard.esm|000122B7 ; Dog
Allow=Dawnguard.esm|000EB872 ; Skeleton
Allow=Dawnguard.esm|00088845 ; Khajiit
Allow=Dawnguard.esm|00010D00 ; Gargoyle
Allow=Dawnguard.esm|0000E88A ; Nord
Allow=Dawnguard.esm|00003D02 ; Deathhound
Allow=Dawnguard.esm|00003D01 ; Dog
Allow=Dawnguard.esm|00002AE0 ; Witchlight
Allow=Dawnguard.esm|0001AACC ; Falmer
Allow=Dawnguard.esm|00019FD3 ; Skeleton
Allow=Dawnguard.esm|00019D86 ; Gargoyle
Allow=Dawnguard.esm|000CDD84 ; Werewolf
Allow=Dawnguard.esm|00018B36 ; Dog
Allow=Dawnguard.esm|00018B33 ; Dog
Allow=Dawnguard.esm|000F3903 ; Spriggan
Allow=Dawnguard.esm|0009AA44 ; Witchlight
Allow=Dawnguard.esm|00013204 ; Spriggan
Allow=Dawnguard.esm|00015C34 ; The Forgemaster
Allow=Dawnguard.esm|00015136 ; Chaurus
Allow=Dawnguard.esm|00013B77 ; Spriggan
Allow=Dawnguard.esm|000117F5 ; Troll
Allow=Dawnguard.esm|000117F4 ; Snow Troll
Allow=Dawnguard.esm|0000D0B6 ; Sabre Cat
Allow=Dawnguard.esm|0000D0B2 ; Deer
Allow=Dawnguard.esm|0000C5F0 ; Dog
Allow=Dawnguard.esm|0000A94B ; Skeleton
Allow=Dawnguard.esm|0000A2C6 ; Gargoyle
Allow=Dawnguard.esm|0000894D ; Draugr
Allow=Dawnguard.esm|00007AF3 ; Draugr
Allow=Dawnguard.esm|000A82B9 ; Orc
Allow=Dawnguard.esm|00088884 ; Wood Elf
Allow=Dawnguard.esm|00088846 ; Redguard
Allow=Dawnguard.esm|00088844 ; Imperial
Allow=Dawnguard.esm|00088840 ; High Elf
Allow=Dawnguard.esm|0008883D ; Dark Elf
Allow=Dawnguard.esm|0008883C ; Breton
Allow=Dawnguard.esm|00088794 ; Nord
Allow=Dawnguard.esm|00006AFA ; Skeleton
Allow=Dawnguard.esm|000051FB ; Chaurusflyer
Allow=Dawnguard.esm|00004D31 ; Nord
Allow=Dawnguard.esm|0000483B ; Draugr
Allow=Dawnguard.esm|0000377D ; High Elf
Allow=Dawnguard.esm|000131F4 ; Falmer
Allow=Dawnguard.esm|0000283A ; Vampire Lord
Allow=Dawnguard.esm|000023E2 ; Draugr
Allow=Dragonborn.esm|0003CECB ; Skeleton
Allow=Dragonborn.esm|0003CA97 ; Nord
Allow=Dragonborn.esm|0003911A ; Dragon Priest
Allow=Dragonborn.esm|00035538 ; Dremora
Allow=Dragonborn.esm|0002B014 ; Dwarven Sphere
Allow=Dragonborn.esm|0002A6FD ; Draugr
Allow=Dragonborn.esm|00029EFC ; Witchlight
Allow=Dragonborn.esm|00029EE7 ; Ice Wraith
Allow=Dragonborn.esm|00028580 ; Netch
Allow=Dragonborn.esm|00027BFC ; Storm Atronach
Allow=Dragonborn.esm|00027483 ; Frostbite Spider
Allow=Dragonborn.esm|00024038 ; DLC2BoarRace
Allow=Dragonborn.esm|0001FEB8 ; Netch
Allow=Dragonborn.esm|0001E17B ; Werewolf
Allow=Dragonborn.esm|0001DCB9 ; HMDaedra
Allow=Dragonborn.esm|0001B658 ; Scrib
Allow=Dragonborn.esm|0001B647 ; MudCrab
Allow=Dragonborn.esm|0001B644 ; Spriggan
Allow=Dragonborn.esm|0001B637 ; Draugr
Allow=Dragonborn.esm|0001A50A ; DLC2ThirskRieklingRace
Allow=Dragonborn.esm|00017F44 ; DLC2RieklingRace
Allow=Dragonborn.esm|000179CF ; DLC2MountedRieklingRace
Allow=Dragonborn.esm|00014449 ; Frostbite Spider

; Races that should NEVER be knocked back
; (Dragons, Giants, Dwarven Automatons, etc.)

Deny=Skyrim.esm|00012E82 ; DragonRace
Deny=Skyrim.esm|000131F9 ; GiantRace
Deny=Skyrim.esm|000CAE13 ; Giant
Deny=Skyrim.esm|000131F1 ; DwarvenCenurionRace
Deny=Skyrim.esm|000131FF ; MammothRace
Deny=Skyrim.esm|001052A3 ; Dragon Race
Deny=Skyrim.esm|000E7713 ; Dragon Race
Deny=Update.esm|001052A3 ; Dragon Race
Deny=Dawnguard.esm|000117DE ; Dragon Race
Deny=Dragonborn.esm|0002C88C ; Dragon Race
Deny=Dragonborn.esm|0002C88B ; Dragon Race
Deny=Dragonborn.esm|0001F98F ; Spectral Dragon
Deny=Dragonborn.esm|0001CAD8 ; Ghost Giant
Deny=Dragonborn.esm|001052A3 ; Dragon Race
Deny=Dragonborn.esm|000E7713 ; Dragon Race
Deny=Dragonborn.esm|00012E82 ; Dragon Race
Deny=Dragonborn.esm|00014495 ; Giant


```

========================================================================================================

## License and Commercial Use

This repository is provided for **non-commercial use only**.

Commercial use is **explicitly prohibited**, including but not limited to:
- Selling the software or source code
- Offering it as part of a paid product, service, or subscription
- Hosting it as a paid SaaS or API
- Charging fees for access, support, or distribution

