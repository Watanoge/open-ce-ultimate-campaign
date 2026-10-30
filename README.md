<div align="center">

# OpenCE Ports

**Halo: Combat Evolved on Android phones and handhelds, the Nintendo Switch and the PlayStation 5, built from the decompilation of the Xbox game.**

[![Latest release](https://img.shields.io/github/v/release/thelinkin3000/halo-ce-universal?label=latest&color=2ea44f)](https://github.com/thelinkin3000/halo-ce-universal/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/thelinkin3000/halo-ce-universal/total?color=blue)](https://github.com/thelinkin3000/halo-ce-universal/releases)
[![Android](https://img.shields.io/badge/Android-Vulkan%20%C2%B7%20Turnip%20%C2%B7%20OpenGL%20ES-3ddc84?logo=android&logoColor=white)](#android)
[![Nintendo Switch](https://img.shields.io/badge/Switch-deko3d-e60012)](#nintendo-switch)
[![PlayStation 5](https://img.shields.io/badge/PS5-Vulkan%20%C2%B7%20RADV-003791)](#playstation-5)
[![License: AGPL-3.0-or-later](https://img.shields.io/badge/license-AGPL--3.0--or--later-blue)](LICENSE)
[![Discord](https://img.shields.io/badge/Discord-join%20the%20server-5865F2?logo=discord&logoColor=white)](https://discord.gg/9gqcHyr5km)

[**Download**](#download) ·
[Campaign upgrades](#campaign-upgrades) ·
[Install](#install) ·
[Graphics](#graphics-vulkan-turnip-and-deko3d) ·
[Multiplayer](#multiplayer) ·
[Controls](#controls) ·
[Report a problem](#reporting-a-problem) ·
[Building](#building) ·
[Discord](https://discord.gg/9gqcHyr5km)

<!-- Videos and screenshots from a contributor go here: one large capture,
then a row of three, each taken on a phone, a Switch or a PS5. -->

</div>

On top of the port, this fork upgrades the campaign: an **enemy
randomizer**, **40 skulls**, and a **scoring system** with medals,
**online leaderboards** and a **daily challenge**. Not everyone will want
all of it, so every part can be turned off or tuned in
**Settings > Gameplay**. See [Campaign upgrades](#campaign-upgrades).

It is not an emulator. The game's own code, decompiled from the Xbox build
2342, is compiled for the device's processor (ARM, or the PS5's x86-64).
Its Direct3D 8 rendering is translated to **Vulkan** on Android and the PS5
and to **deko3d**, the Switch's own GPU interface, on the Switch.

> [!TIP]
> **New: the OpenCE overlay.** Hold **L + R** and press **d-pad down** (on
> Android without a controller, tap the screen with three fingers, or four as set in the touch controls' General options) for a
> panel of cheats (god mode, infinite ammo, super jump and more), bots, the
> game's speed, the HUD, the camera and the renderer's switches, without
> leaving the game, and entries of your own. No cheats in multiplayer unless
> you are the host: in a game someone else hosts, the overlay's cheats are
> refused. See [The OpenCE overlay](#the-opence-overlay).

> [!IMPORTANT]
> **No game data is included.** You need your own **Xbox** disc image of
> Halo: Combat Evolved (`.iso` or `.xiso`, any region). The game unpacks
> its maps from it on the first start.

> [!WARNING]
> **Official sources.** The only official downloads are the
> [releases on this repository](https://github.com/thelinkin3000/halo-ce-universal/releases). Builds,
> "updates" or donation requests offered anywhere else under this project's
> name are not from us.

<div align="center">

[![Halo Combat Evolved - Nintendo Switch: watch on YouTube](https://img.youtube.com/vi/imNdnKszCVI/maxresdefault.jpg)](https://www.youtube.com/watch?v=imNdnKszCVI)

*Halo: Combat Evolved on the Nintendo Switch (video by puginatux).*

</div>

## Contents

- [What works](#what-works) · [Known issues](#known-issues)
- [Campaign upgrades](#campaign-upgrades): [daily challenge](#daily-challenge), [randomizer](#enemy-randomizer), [Tag Injector](#tag-injector), [skulls](#skulls), [multiplayer and co-op](#multiplayer-and-co-op), [settings](#gameplay-settings)
- [Download](#download) · [Install](#install): [Android](#android), [Nintendo Switch](#nintendo-switch), [PlayStation 5](#playstation-5), [updating](#updating)
- [Graphics: Vulkan, Turnip and deko3d](#graphics-vulkan-turnip-and-deko3d)
- [Multiplayer](#multiplayer) · [Controls](#controls) · [The OpenCE overlay](#the-opence-overlay)
- [Reporting a problem](#reporting-a-problem)
- [Building](#building) · [Credits](#credits)

## What works

| | |
| --- | --- |
| 🎮 **Campaign** | The whole campaign, with checkpoints, saves and cinematics. Xbox maps of every region; PAL maps are made to play as NTSC ones. |
| 🎲 **Campaign upgrades** | An enemy and weapon randomizer (with weapons and enemies from Custom Edition maps through the Tag Injector), 40 skulls, Halo 3-style scoring and medals (in multiplayer too), online leaderboards and a daily challenge. All optional. See [Campaign upgrades](#campaign-upgrades). |
| 🖼️ **Vulkan on Android** | The default renderer. On Qualcomm Adreno GPUs the app can download and use **Turnip**, Mesa's open-source driver, in place of the phone's own. OpenGL ES 3 is still there as a fallback. |
| 🟥 **deko3d on the Switch** | A renderer written for the console's GPU, docked at 1080p or handheld at 720p, switching on its own when you dock. |
| 🟦 **The PS5** | A homebrew title for a jailbroken console: up to native 4K at 120 frames a second, aiming with the DualSense's gyroscope, voice chat through the pad's microphone. |
| 👥 **Split screen** | Up to four players on one device, with one controller each. |
| 🌐 **Multiplayer** | System link on the local network and internet play through invite links and the server browser, with voice chat. |
| 🗺️ **Custom maps** | Xbox maps and Halo PC / Custom Edition maps, downloaded from the host when you join without them. |
| ✋ **Touch** | On Android: touch controls in game with swipe aiming, optional gyro aiming, a layout editor; taps work in every menu. |
| 🔠 **High-res HUD and text** | The HUD, the menus' text and their titles redrawn at high resolution. |
| 🔄 **Self-updating** | Both builds look for a newer release when they start and offer to install it. |

### Known issues

- Bink movies don't play: the game skips them.
- **Android:** kernels with 16 KB memory pages (a developer option on
  Android 15, and some newer phones) can't run the game: the Xbox memory
  uses 4 KB pages. The app also needs Android 9 or later and a 64-bit
  ARM CPU.
- **Android, Vulkan:** the first time a scene shows, a few parts can be
  missing for a moment while the phone compiles their shaders.

The current list is in the [issues](https://github.com/thelinkin3000/halo-ce-universal/issues).

## Campaign upgrades

The aim of this fork is a better campaign: new ways to replay the levels
you know, and a score to chase on them. Every feature below is optional.
Each one has its own page in **Settings > Gameplay**, where it can be
turned off or tuned, and the defaults leave the original game alone where
it matters (scoring, for example, starts off).

### Daily challenge

<img width="640" alt="The Campaign menu, with DAILY CHALLENGE" src="https://github.com/user-attachments/assets/38bf5664-44e7-487e-bcd9-894360953401" />

**DAILY CHALLENGE** is a new item in the Campaign menu. Each day there is
one challenge, the same for everyone: a campaign level at a set difficulty,
a randomizer setup and a set of skulls, and a seed for the randomizer, so
every player meets the same enemies in the same places.

<img width="640" alt="The daily challenge screen: today's level, difficulty, skulls and leaderboard" src="https://github.com/user-attachments/assets/450b7b8f-8962-451d-acc1-92e67f65feaf" />

The challenge screen shows the day's level, difficulty and skulls, your
best run, and the day's leaderboard. **PLAY** applies the challenge's
settings for that run only: your own randomizer and skull settings are
kept and come back when you return to the main menu. Every run you finish
is sent to the leaderboard, and the screen comes back when the level is
won, so you can see where you placed.

<img width="640" alt="PAST DAYS: earlier challenges with their best score and your place" src="https://github.com/user-attachments/assets/4e3916f3-d0ee-45b1-b6d8-b039acdaac73" />

**PAST DAYS** lists the earlier challenges, with each one's best score,
your own score and place, and how many people played it. Past challenges
can still be played, and still count on their day's leaderboard; the top
three from while the day was live stay at the top. A **random run**, made
up on the spot from your unlocked skulls and settings, is there for when
you want something new that doesn't count.

The challenges and leaderboards come from a community server running
[api-opence](https://github.com/Watanoge/api-opence). By default that is
`https://oceapi.watanoge.com`; you can point the game at your own server
instead, or play offline. Offline, the day's challenge is generated on the
device from the date, and the leaderboard is your own profiles'.

### Enemy randomizer

<img width="640" alt="The randomizer screen shown after choosing a level's difficulty" src="https://github.com/user-attachments/assets/bbcfb82c-911f-43b4-8ea9-7bd9af21e7bf" />

With the randomizer on, choosing a campaign level's difficulty opens a
screen that sets how the level is shuffled:

- **Enemies:** each squad's actors are drawn from a pool: any, Covenant,
  Flood, human, or a single type (and rank, like all Zealots or all
  Spec Ops grunts). Allies can be swapped too.
- **Enemy weapons:** each actor's weapon is swapped for another it can
  hold, from a pool of your choosing.
- **Pickups:** the weapons lying around the level are swapped as well.

Characters and weapons a level doesn't have are copied in from the other
campaign maps. Scripted characters (in a conversation or following a
command list) keep their own actor, so the story still plays out.

<img width="640" alt="Loading a saved randomizer preset" src="https://github.com/user-attachments/assets/eb88a82c-dc57-4d13-b233-8daa2ae842c7" />

The setup is kept for the following levels, and up to eight setups can be
saved as named presets and loaded again from this screen. To delete one,
press **DELETE** on the load screen, then the preset twice (or X twice on
it with a controller).

On the first start (or the first time the randomizer is turned on, with
the daily challenge off), the game builds `maps/watanoge_randomizer.pack`
(about 500 MB) from your maps, once, which randomized levels copy their
enemies and weapons from. The other tag packs (the skulls', the game
modes', the Tag Injector's weapon HUDs) are smaller and built the same way
([port/linux/TAG_INJECTION.md](port/linux/TAG_INJECTION.md)).

### Tag Injector

<img width="640" alt="Gameplay with Custom Edition enemies, a Custom Edition weapon held, others on the floor and a marine carrying one" src="https://github.com/user-attachments/assets/efbcc8c9-6e7b-4781-9d80-89187f4f4a39" />

The Tag Injector brings weapons, enemies and equipment from Halo PC /
Custom Edition maps into the Xbox campaign. You take them out of the
[Custom Edition maps](#custom-edition-maps) in your `custom_maps` folder,
keep them in **tag profiles**, and the [randomizer](#enemy-randomizer) puts
them in the level.

<img width="640" alt="Settings > Gameplay, with TAG INJECTOR" src="https://github.com/user-attachments/assets/639b884d-1139-4a1c-a107-e4a24b8cde4b" />

<img width="640" alt="The Tag Injector menu: TAG INJECTION, TAG PROFILES and MAPS" src="https://github.com/user-attachments/assets/72a717ad-f69d-4d72-8398-1bfd004ce650" />

It's in **Settings > Gameplay > Tag Injector**. **TAG INJECTION** turns
it on or off, **TAG PROFILES** lists your profiles, and **MAPS** lists your
Custom Edition maps.

#### Taking tags out of a map

<img width="640" alt="MAPS: ADD TO, CUSTOM SINGLEPLAYER and CUSTOM MULTIPLAYER" src="https://github.com/user-attachments/assets/0533551b-c1df-4533-a164-d4535abaaf4f" />

In **MAPS**, **ADD TO** picks the profile tags go into (**NEW PROFILE**
makes one). Open **CUSTOM SINGLEPLAYER** or **CUSTOM MULTIPLAYER**, a
folder and a map, and the map's tags are listed by kind: weapons, AI,
equipment, vehicles, bipeds and scenery.

<img width="640" alt="A map's tags by kind" src="https://github.com/user-attachments/assets/c7861247-f169-4141-b19f-d46f88351ae5" />

<img width="640" alt="A map's AI, with one added to the profile" src="https://github.com/user-attachments/assets/c9f85c5a-9927-4b87-8ec9-cdf490ed68d1" />

Press A on a weapon, an enemy or a piece of equipment to add it to the
profile. Everything it needs comes with it: its model, animations,
collision, HUD, projectiles, effects, sounds and bitmaps, and for an enemy
the weapons and equipment it carries. Vehicles, bipeds and scenery are
listed but can't be added yet.

<img width="640" alt="An enemy's turning model preview" src="https://github.com/user-attachments/assets/0e1c588e-774e-4b9d-b856-d54cd814d149" />

<img width="640" alt="A weapon's turning model preview" src="https://github.com/user-attachments/assets/cb23cbbb-591c-47bd-912d-04c710c7ed3b" />

The frame on the right shows the tag's model turning, with its own colors,
and under it what the tag needs to work and whether any of it is missing.
Tags from protected maps, which have no names of their own, are named from
what they are (`battle rifle`, `elite battle rifle`).

#### Tag profiles

<img width="640" alt="TAG PROFILES, with each profile's size" src="https://github.com/user-attachments/assets/9013e9d1-d1a8-47c5-a09a-0f7a12cd2f53" />

<img width="640" alt="A profile's tags by kind" src="https://github.com/user-attachments/assets/6e4396c0-06f6-40f2-88d7-b19f09ca8dd4" />

A profile keeps its tags on its own, so it plays without the maps they came
from and can be copied to another device whole. Each one is a folder in
`tag_injector`, beside `maps`. Tags from the same map share their data, so
a profile is much smaller than its tags were in their maps.

Left or right turns a tag off, to keep it in the profile but leave it out.
A on a tag opens its settings:

<img width="640" alt="An enemy's settings" src="https://github.com/user-attachments/assets/522f8084-0be4-4093-8bd7-32f2726dd4bc" />

<img width="640" alt="A weapon's settings" src="https://github.com/user-attachments/assets/ab3ac3c3-8f93-4506-b7ac-01941892c4c6" />

- **NAME:** the name the menus show.
- **FACTION:** the randomizer pools it joins (Covenant, Flood, human).
- **COUNTS AS** (an enemy's): the campaign type it plays as, so a brute
  set to ELITE shows up when the randomizer picks elites.
- **CLASS** (a weapon's): precision, automatic or power.
- **CUSTOM TYPE:** groups tags so they can be played together: a brute
  minor, major and captain all of type BRUTE.
- **HUD** (a weapon's): its own HUD, or a stock one (the sniper rifle's,
  the needler's...) with its crosshair, ammo meter and zoom.
- **REPLACE ORIGINAL:** for a tag that has the same name as one in the
  campaign (an energy sword, an elite major), puts it in that tag's place
  everywhere in the level.
- **REMOVE TAG:** takes it out of the profile.

**DUPLICATE PROFILE** copies a profile, and **DELETE PROFILE** deletes it.

<img width="640" alt="TAG PROFILES with a profile whose tag failed to load, in yellow" src="https://github.com/user-attachments/assets/f7ce8ef2-bfb1-42e5-89a4-130f666c950f" />

<img width="640" alt="A kind of tags with one that failed, listed first" src="https://github.com/user-attachments/assets/37cfbcce-5532-450d-bd9c-f9b4fd13f3ff" />

<img width="640" alt="A failed tag's details, saying why it failed" src="https://github.com/user-attachments/assets/82ad1a63-f7a9-4896-be53-9803389412af" />

A tag the game couldn't play in a level (its data missing, a model or a
bitmap this build can't draw, no room left in the level) is turned off and
marked **FAILED**, with the reason in its details. The profile, the kind of
tag and the tag itself turn yellow so it's easy to find, and the HUD says
which tags failed a couple of seconds into the level. Turning it on again
tries it again.

#### Playing a profile

With **TAG INJECTION** on, choosing a campaign level's difficulty first asks
which profile to play in it, or none, then opens the randomizer's screen.
The profile's tags join the pools their settings name, and the randomizer
gets new **TAG INJECTOR** choices:

<img width="640" alt="The randomizer with only the Tag Injector as the source" src="https://github.com/user-attachments/assets/06af9d6e-54a0-4bd6-9de5-6bf11cffc372" />

- **ENEMY POOL**, **WEAPON POOL** and **PICKUP POOL** set to **TAG
  INJECTOR** play the profile's enemies or weapons alone.

<img width="640" alt="The randomizer with ONE TYPE set to a custom type from the Tag Injector" src="https://github.com/user-attachments/assets/829d693f-0ffe-4c67-8170-ae2c9568ad96" />

- **ONE TYPE**'s **TAG INJECTOR** with a **CUSTOM TYPE** plays a single
  custom type: a brute-only campaign, say.

Only a level played alone gets the profile's tags: network games, the daily
challenge and random runs don't, since everyone has to play the same thing.

#### The code

- [port/linux/game/tag_injector.c](port/linux/game/tag_injector.c): the
  menu's maps and tags, taking tags out, and adding a profile's tags to a
  level as it loads.
- [port/linux/game/tag_bundle.c](port/linux/game/tag_bundle.c): a tag and
  everything it names, packed as a bundle.
- [port/linux/game/tag_preview.c](port/linux/game/tag_preview.c): the
  turning model preview, drawn on the processor.
- [port/linux/src/port_tag_profiles.c](port/linux/src/port_tag_profiles.c)
  and [port_tag_store.c](port/linux/src/port_tag_store.c): the profiles'
  folders, `profile.toml`, and the shared pack their data is kept in.
- [port/linux/game/menu_functions.c](port/linux/game/menu_functions.c): the
  menu ("port tags" functions) and the profile screen before the
  randomizer's ("randomizer tag profile").
- [port/linux/game/randomizer.c](port/linux/game/randomizer.c): the pools
  the tags join.

How it all works, down to the profile's files and what happens in the
level, is in [docs/tag_injector.md](docs/tag_injector.md).

### Skulls

<img width="640" alt="The skulls screen" src="https://github.com/user-attachments/assets/af8fc80e-503c-45a2-a35b-40ddf5ec8268" />

After the difficulty (and the randomizer, if it's on), a screen turns
skulls on and off. There are 40: the 22 Halo: Combat Evolved skulls of the
Master Chief Collection, some from Halo 2 Anniversary and Halo 3 (Envy,
Assassins, Streaking, Swarm), and new ones made for this port, such as:

- **Outbreak:** killed enemies may release Flood spores that raise nearby
  bodies as combat forms.
- **Scarab:** your human weapons fire rockets, and Covenant ones fuel rods.
- **Floor Is Lava:** standing on the level's ground burns you.
- **Give & Take:** health only comes back by hitting enemies.
- **Woe Is Me:** everyone is your enemy, the marines included.
- **Rip & Tear:** melee only, but punches hit twice as hard and you get an
  overshield.

<img width="640" alt="The skulls screen, with RANDOM" src="https://github.com/user-attachments/assets/0096dfaa-0668-457c-b1d4-87f82a456025" />

<img width="640" alt="RANDOM: the fewest and the most skulls, and ROLL" src="https://github.com/user-attachments/assets/e0f0543f-325a-4a26-860b-2258c7efe446" />

**RANDOM** turns on a random set of skulls: choose the fewest and the most
skulls you want, and **ROLL** turns on a random number between the two,
picked from the skulls you have, and turns the rest off. The range is
remembered for next time (`game.skulls_random_min` and
`game.skulls_random_max` in `config.toml`).

The help line on the screen describes each skull. On the first start,
once the game has built the skulls' data, it asks how you want to get them: all unlocked from the start, or
unlocked by playing. Played for, skulls drop from enemies you kill (more
often for headshots, assassinations and multi-kills), and are kept once
you finish the level, or lost on death if you choose. How many skulls drop
per level, and how often, can be changed.

### Scoring and medals

With **SCORING** on, the campaign is scored as in Halo 3 and the Master
Chief Collection. Each kill is worth its enemy's points, multiplied by
how it was done (headshot, melee, assassination...), the difficulty and
the skulls on. Dying and reverting to a checkpoint cost points, and
finishing under the level's par time multiplies the total by up to four.

The score, time and current multiplier sit at the bottom right during
play, Halo 3 and Reach medals pop up as you earn them, and a summary of
the level is shown when you win it. Each profile keeps its best score and
time for every level, and finished runs can be shared to each level's
online leaderboard, which is shown from the level list in **New Game**.
In co-op each player can score separately, or as a team.

All the point values are in
[campaign_score_values.json](port/linux/game/campaign_score_values.json).

#### In multiplayer

The medals and the score panel work in multiplayer games too, with
**MULTIPLAYER** on in the scoring settings. They're yours alone: each
machine works out its own players' medals, nothing is sent to the other
players, and the host doesn't need this port's scoring to show them.

<img width="640" alt="A free for all game with a green Spartan, the score panel forced to white and a few medals on screen" src="https://github.com/user-attachments/assets/44de32dd-9e46-4fcd-9c3c-e27fa9f09d59" />

The panel at the bottom right has two parts:

- **Medal history** (with **MEDALS** on): your last 12 medals.
- **Score** (with **SCORE HUD** on): whether you're winning, tied or losing
  and the match time, then your name, your place and your kills / deaths /
  assists, then the game mode and its score.

<img width="640" alt="A team CTF game with the score panel in the Spartan's color: the red team's" src="https://github.com/user-attachments/assets/00f05b2a-af6f-42ca-a32c-4f110704fc81" />

**HUD COLOR** sets the panel's color, in the campaign and in multiplayer:
**DEFAULT** is the blue of the rest of the interface, **SPARTAN** matches
your Spartan (your armor color, or your team's in a team game), and
**CUSTOM** always uses the color you pick. Above, a green Spartan with
the panel forced to white in a free for all, and the same Spartan with
**SPARTAN** in a team CTF game, where both turn red.

### Multiplayer and co-op

<img width="640" alt="The co-op map list with the daily challenges" src="https://github.com/user-attachments/assets/a7633055-687e-47de-a3cc-6154918e9301" />

Daily challenges can be played in co-op too: the host picks one from the
map list in the lobby, and the run goes to that day's leaderboard.

<img width="640" alt="The lobby's game settings, with Skulls and Randomizer" src="https://github.com/user-attachments/assets/4a1c3225-bc7f-4d36-8c31-a5d0e8e53068" />

The lobby's settings have their own **SKULLS** and **RANDOMIZER** pages,
so a host sets them for the whole game. Clients play with the host's
settings, and a randomized game is listed with `[RNDM MOD]` before the
level's name so players know what they're joining.

### Gameplay settings

<img width="640" alt="The Settings menu, with GAMEPLAY" src="https://github.com/user-attachments/assets/21528b89-dc57-490c-98ac-430a9ad01dfd" />

Everything above is configured from **Settings > Gameplay**.

<img width="640" alt="The Gameplay menu" src="https://github.com/user-attachments/assets/66bfd967-3d17-44d1-9736-3cdb976ca9e7" />

The Gameplay menu also holds settings that aren't part of the campaign
upgrades, such as ragdolls and campaign progression (unlock every level
from the start), and has a page for each feature.

<img width="640" alt="Randomizer settings with GO CRAZY on" src="https://github.com/user-attachments/assets/0d08f176-44db-4ea0-9648-f78c1e7b4c8b" />

<img width="640" alt="Randomizer settings with GO CRAZY off" src="https://github.com/user-attachments/assets/929c8381-3964-4290-92de-001d688fd143" />

**Randomizer** turns the randomizer screen on or off. **GO CRAZY** unlocks
the less stable options: any character can hold any weapon (even one it
has no animations for), engineers and sentinels join the enemy pools, the
multiplayer flamethrower can show up, and more of the Flood's infection
forms are swapped. Expect some odd behavior with it on.

<img width="640" alt="Skulls settings, all options shown" src="https://github.com/user-attachments/assets/e8ffaee4-b54b-40b1-b518-9473e250ab3b" />

**Skulls** turns the skulls screen on or off and sets how skulls are
earned: all unlocked, unlocked as soon as they're picked up, or only by
finishing the level, whether they are lost on death, how many drop per
level and how often. **RESET SKULL PROGRESS** starts over.

<img width="640" alt="Scoring settings, with HUD color and multiplayer" src="https://github.com/user-attachments/assets/a3bc2b5b-7e23-45bb-ab69-3787f44e4037" />

**Scoring** turns the campaign score on or off, shows or hides the medal
popups, sets the score panel's color (**HUD COLOR**, and **CUSTOM COLOR**
with **CUSTOM**), and turns on the medals and score panel in multiplayer
(**MULTIPLAYER**, then **MEDALS** and **SCORE HUD**). See
[Scoring and medals](#scoring-and-medals).

<img width="640" alt="Community settings" src="https://github.com/user-attachments/assets/a194434c-e622-45bf-89b4-d338e2db9642" />

**Community** sets the online side: whether the game talks to a community
server at all, which one (the default or your own), whether your level
scores are shared to the leaderboards, and whether the daily challenge is
in the Campaign menu. With **ONLINE** off nothing is sent anywhere.
Your scores are tied to a key made for this install, kept in
`config.toml` as `community.key`; copy it to another install to keep
them.

**Tag Injector** turns Custom Edition weapons and enemies in the
randomizer on or off, and manages their profiles. See
[Tag Injector](#tag-injector).

Every one of these settings is also in `config.toml`; they are listed in
"Settings" in [port/linux/README.md](port/linux/README.md#settings).

## Download

Each release is a build of `main` we choose to publish, numbered by its tag.
These links always point to the latest one:

| Platform | Play | Debug (for reports) |
| --- | --- | --- |
| Android | [halo-android-release.zip](https://github.com/thelinkin3000/halo-ce-universal/releases/latest/download/halo-android-release.zip) | [halo-android-debug.zip](https://github.com/thelinkin3000/halo-ce-universal/releases/latest/download/halo-android-debug.zip) |
| Nintendo Switch | [halo-switch-release.zip](https://github.com/thelinkin3000/halo-ce-universal/releases/latest/download/halo-switch-release.zip) | [halo-switch-debug.zip](https://github.com/thelinkin3000/halo-ce-universal/releases/latest/download/halo-switch-debug.zip) |
| PlayStation 5 | [halo-ps5-release.zip](https://github.com/thelinkin3000/halo-ce-universal/releases/latest/download/halo-ps5-release.zip) | [halo-ps5-debug.zip](https://github.com/thelinkin3000/halo-ce-universal/releases/latest/download/halo-ps5-debug.zip) |

Use the release build to play. The debug build stops at the first failed
assertion and writes it to the log, which makes it the one to use when
you're chasing a problem. The [releases page](https://github.com/thelinkin3000/halo-ce-universal/releases)
keeps the last five builds, if the latest one has a problem.

For Windows and Linux, see the upstream project,
[OpenCE](https://github.com/OpenCommunityEdition/OpenCE).

## Install

### Android

1. Unzip the download and install the APK.
2. Copy your Xbox disc image to the phone.
3. Start the app. On the first start it asks for the disc image: pick it,
   and it unpacks the maps (about 1.7 GB) into its own data folder.

The app needs Android 9 or later on 64-bit ARM. More, including where the
data folder is and how to keep your saves across a reinstall, is in
[port/android/README.md](port/android/README.md).

### Nintendo Switch

The Switch build is a homebrew program, for a console that runs custom
firmware (Atmosphère). Run it with **title takeover** (hold R while you
start a game, or use a forwarder), not from the album: the game needs the
memory that only a full title gets.

1. Unzip the download into `sdmc:/switch/halo/` on the SD card, so that
   you have `sdmc:/switch/halo/halo.nro` and `sdmc:/switch/halo/halo_guest.elf`.
2. Put your disc image next to them as `sdmc:/switch/halo/halo.iso`.
3. Start `halo.nro`. On the first start it unpacks the maps (about 1.7 GB;
   it takes a few minutes, and the screen shows how far it has got).

An already unpacked `maps/` folder at `sdmc:/switch/halo/maps/` works as
well. Saves go in `sdmc:/switch/halo/save/` and settings in
`sdmc:/switch/halo/config.toml`. See
[port/switch/README.md](port/switch/README.md).

### PlayStation 5

The PS5 build is a homebrew title, for a console on system software 13.60
or lower, jailbroken (the Relapse exploit, or another kernel exploit, then
kstuff), with ShadowMountPlus 1.7beta5 or later and an FTP server such as
ftpsrv.

1. Unzip the download on your computer, and copy its `PPSA99500` folder
   into `/data/homebrew` on the console over FTP.
2. Make the folder `/data/halo` and copy your disc image into it.
3. Start Halo: Combat Evolved from the home screen. On the first start it
   unpacks the maps (under a minute); notifications show how far it has
   got.

Saves and settings go in `/data/halo`. See
[port/ps5/README.md](port/ps5/README.md).

### Updating

The game updates itself: when it starts, it looks for a newer release and
asks whether you want it. `update.auto = false` in `config.toml` turns the
check off. On the PS5 it downloads the new release while you play and
tells you to close the game and start it again. To update by hand, install
the new APK over the old one, or copy the new files over the old ones on
the SD card or the console. Your saves and settings stay.

## Graphics: Vulkan, Turnip and deko3d

**Android** draws with Vulkan by default. To change it, open **SETTINGS**
from the main menu, choose a profile, and open **VIDEO SETUP**:

- **GRAPHICS BACKEND:** `VULKAN` or `OPENGL` (OpenGL ES 3).
- **VULKAN DRIVER** (with `VULKAN`): `STOCK`, the phone's own driver, or
  `TURNIP`, the open-source Mesa driver for Qualcomm Adreno GPUs, which the
  app downloads. On Adreno, Turnip is often the more correct of the two.

Select **OK**, then close the game and start it again: the backend and the
driver change only at start-up. The line below the version number in the
main menu shows which ones are running. For driver archives of your own and
what to send when something looks wrong, see "Graphics: OpenGL ES and
Vulkan" in [port/android/README.md](port/android/README.md).

**The Switch** draws with deko3d, the console's native graphics interface,
through a renderer written for it ([port/switch/DEKO3D.md](port/switch/DEKO3D.md)).
There is nothing to set.

**The PS5** draws with the same Vulkan renderer as Android, on RADV, Mesa's
driver for AMD's GPUs, built for the console by
[PS5_Vulkan](https://github.com/mpereiraesaa/PS5_Vulkan). Its VIDEO SETUP has
MAXIMUM RESOLUTION (2160P, 1440P or 1080P). The first visit to each place
can stutter while its shaders are made; the second is smooth.

## Multiplayer

- **System link** on a local network, with up to 128 players. Android phones,
  Switches and PS5s on the same network see each other's games.
- **Internet play:** the host's invite link (Android puts it on the
  clipboard, and opens `halo://join/...` links), or the server browser.
  No server of this project is needed.
- **Voice chat**, with the phone's microphone, a headset in the Switch's
  jack, or the DualSense's microphone.
- **Split screen** for up to four players on one device.
- **Custom maps** are sent by the host to players who don't have them.
- **Medals and a score panel**, shown only to you, with nothing needed from
  the host. See [In multiplayer](#in-multiplayer).

### Custom Edition maps

Put Halo PC / Custom Edition maps in `custom_maps`, beside `maps` (on the
Switch, `sdmc:/switch/halo/custom_maps/`; on the web, **Add a custom_maps
folder…** under **Game data**), with Custom Edition's
`bitmaps.map`, `sounds.map` and `loc.map`. The level lists then have CUSTOM
SINGLEPLAYER and CUSTOM MULTIPLAYER. A subfolder of `custom_maps` is listed
under its own name and opens to its maps, so a set of maps (Cursed Halo,
say) can keep its own folder; the campaign maps in it show under CUSTOM
SINGLEPLAYER and the multiplayer maps under CUSTOM MULTIPLAYER. One level
of folders is read, and `bitmaps.map`, `sounds.map` and `loc.map` stay in
`custom_maps` itself. Joining a game on a map you don't have downloads it,
with its progress at the top of the screen.

<img width="640" alt="CUSTOM SINGLEPLAYER, listing folders of maps" src="docs/images/custom-singleplayer.jpg" />

<img width="640" alt="A folder of CUSTOM MULTIPLAYER opened to its maps" src="docs/images/custom-multiplayer.jpg" />

<img width="280" alt="custom_maps with bitmaps.map, sounds.map, loc.map and a folder for each set of maps" src="docs/images/custom-map-folders.png" />

<img width="640" alt="The lobby downloading a Custom Edition map" src="docs/images/map-download.jpg" />

The netcode is new: each machine moves its own player at once, and the host
makes the decisions for the game. See [port/linux/NETCODE.md](port/linux/NETCODE.md).
Keep the game in the front during a network game on Android: in the
background, Android stops it and the others drop it after 15 seconds.

## Controls

Any controller that Android knows works, and on the Switch the Joy-Con,
the Pro Controller and the Lite's own buttons, with rumble. The first
controller is player 1 and the others are players 2 to 4. The buttons
follow the Xbox controller's:

| Xbox | DualSense | In game |
| --- | --- | --- |
| left / right stick | left / right stick | move / look |
| right trigger | R2 | fire |
| left trigger | L2 | throw a grenade |
| A | Cross | jump, accept |
| B | Circle | melee, back |
| X | Square | action, reload |
| Y | Triangle | change weapon |
| white / black | L1 / R1 | flashlight / change grenade |
| stick clicks | L3 / R3 | crouch / zoom |
| start / back | Options / Create | pause / scoreboard |

On Android without a controller, touch controls show in game: a move stick,
swipe to look (with the controller's aim assist), and buttons that you can
move, resize, hide or copy from **Options** at the top of the screen.
In menus, tap an item to pick it and drag to
scroll. All of it is in "Controls" in
[port/android/README.md](port/android/README.md).

The gyroscope can aim as well, the phone's on Android and the controller's
on the Switch (each player's own): open a profile, **GAMEPADS**, then
**GYRO AIM**, to turn it on (ON, or ONLY ON ZOOM for fine aiming through a
scope) and set its sensitivity and inversion each way. See
[port/switch/README.md](port/switch/README.md#gyro-aiming).

<img width="640" alt="Controller setup, with Gyro Aim as its own row" src="docs/images/gyro-aim-row.jpg" />

<img width="640" alt="The Gyro Aim screen: aim, sensitivity and invert" src="docs/images/gyro-aim.jpg" />

The settings, which are the same on every platform, are described in
[port/linux/README.md](port/linux/README.md#settings).

## The OpenCE overlay

A panel over the left of the screen with cheats, bots and switches, in the
menus and in a game, on the Switch, the PS5 and Android.

### Opening it

| | Open and close |
| --- | --- |
| Switch | hold **L** and **R**, press **d-pad down** |
| PS5 | hold **L1** and **R1**, press **d-pad down** |
| Android, with a controller | hold **L1** and **R1** (LB and RB), press **d-pad down** |
| Android, touch only | tap the screen with **three fingers** together (or four: the touch controls' Options, General) |

The same buttons close it. While it is open the game gets none of your
input, but it is not paused.

| Control | Does |
| --- | --- |
| d-pad or left stick | move up and down the list (hold to repeat) |
| A (Cross) | choose: run a command, flip a toggle, open a folder |
| left / right | flip a toggle; otherwise right opens a folder, left goes back |
| B (Circle) | back out of a folder; at the top, close |
| Back / Start (Create / Options) | close |
| touch or mouse | tap a row to choose it, tap the folder's name to go back, tap outside the panel to close, drag or scroll the wheel to scroll |

A label too long for the panel scrolls while it is chosen, and some entries
show a note above the panel's foot (for example, "Applies at the next
restart"). What a command did shows at the foot for a few seconds.

Its folders: **Cheats**, **Bots** (add and drop bots, their difficulty,
vehicles and routes), **Game Speed**, **HUD**, **Camera** and **System**
(the frame rate, the renderer's switches and, on the Switch and the PS5,
Xbox button pictures). `game.overlay_menu = false` in `config.toml` stops the
buttons from opening it.

### Cheats and multiplayer

The overlay gives no way to cheat in another player's game. It runs the
same commands as the game's console, and in a game someone else hosts
(system link or internet) the game refuses every command that changes the
game: cheats, game speed, spawning, bots, the camera, wireframe, fog and
the other drawing switches. Cheats, a game speed and drawing switches
turned on before you joined are put back as soon as you join, and kept so
while you play. What still works changes only what you see and hear: the
HUD's parts, the frame rate, sound and your own settings. The host decides the game in any case (damage,
deaths, pickups and spawns), so nothing a player's machine does changes
anyone else's game.

The cheats work where the game is yours: the campaign, split screen, and
games you host.

### Adding your own entries

Your entries are `.toml` files in the `overlay` folder of the game's data
folder, the one with `maps/` and `config.toml` in it. The game makes the
folder the first time the overlay opens.

| Device | Folder |
| --- | --- |
| Switch | `sdmc:/switch/halo/overlay/` |
| PS5 | `/data/halo/overlay/` |
| Android | `/sdcard/Android/data/com.halo.decomp/files/overlay/` (copy files with `adb push`) |
| Linux and Windows | `overlay/` beside `maps/` |

Each folder in it is a folder of the overlay, and each folder in one of
those a folder in that, by its name. A folder with the name of one of the
overlay's own (`Cheats`, `Bots`, `System`, in any case) adds its entries to
it. Each `.toml` file is one entry:

```
overlay/
  Cheats/
    infinite_grenades.toml   added to the overlay's Cheats
  Fun/                       a new folder, after the overlay's own
    big_heads.toml
    Weather/                 a folder in Fun
      rain.toml
```

There are three kinds of entry:

```toml
# a command, run when it is chosen
label   = "Spawn All Weapons"
type    = "command"
command = "cheat_all_weapons"
```

```toml
# a toggle, ON or OFF
label = "VSync"
type  = "toggle"
on    = "config display.vsync true"
off   = "config display.vsync false"
state = "config:display.vsync"
note  = "Takes effect at once"
```

```toml
# a choice: one of several in a folder, marked ON while it is the one
label   = "Half Speed"
type    = "choice"
command = "game_speed 0.5"
state   = "speed=0.5"
```

| Key | Means |
| --- | --- |
| `label` | the entry's name in the list |
| `type` | `command`, `toggle` or `choice` |
| `command` | a command's or a choice's: a console command, or `config <setting> <value>` to set and save a setting of `config.toml` |
| `on`, `off` | a toggle's commands, as `command` |
| `state` | where a toggle's ON or a choice's being chosen is read (below); a toggle without one shows what it was last set to |
| `note` | a line shown while the entry is chosen (optional) |

| `state` | ON while |
| --- | --- |
| `global:<name>` | the console global is true (`global:cheat_deathless_player`) |
| `global:<name>=<number>` | the console global has that value (`global:rasterizer_stats=5`) |
| `config:<setting>` | the setting is true, or a number not 0 (`config:display.vsync`) |
| `config:<setting>=<word>` | the setting has that value (`config:bots.routes=lanes`) |
| `speed=<number>` | the game runs at that speed (`speed=2`) |
| `remembered` | the overlay last turned it on |

A folder lists the overlay's own entries first, then its folders, then its
files' entries, each in order of name. The files are read again each time
the overlay opens after one has changed, so an entry copied over FTP shows
the next time you open it. A file that is not a whole entry shows in red as
**LABEL: MALFORMED ENTRY** (**MALFORMED ENTRY: NO LABEL** without a label),
and choosing it says what is wrong; `debug.txt` says so too. Entries from
files follow the same multiplayer rules as the overlay's own.

The overlay's first entries are those of hardran3's
[OpenCE-Ultrahand](https://github.com/hardran3/OpenCE-Ultrahand) and
HaloDrones packages for the Switch's Ultrahand Overlay.

## Reporting a problem

Open an [issue](https://github.com/thelinkin3000/halo-ce-universal/issues) or ask on
[Discord](https://discord.gg/9gqcHyr5km), and include:

- the device (phone model and GPU, or Switch / Switch Lite / OLED) and the
  build number from the main menu;
- on Android, the renderer and driver line from the main menu;
- `debug.txt` and `halo.log` from the game's folder
  (`sdmc:/switch/halo/` on the Switch, the app's data folder on Android);
- on the Switch, if it crashed, the newest report in
  `sdmc:/atmosphere/crash_reports/`;
- screenshots or a video of what looks wrong.

A debug build gives the most useful logs.

## Building

You do not need the Xbox SDK. The port supplies the SDK declarations that
the game uses. Refer to [port/include/xdk](port/include/xdk/README.md).

To build the game:

1. Install Python and [ninja](https://ninja-build.org/).
2. Install the tools for your platform. Refer to the README for the
   platform.
3. In the root folder of the repository, enter `python configure.py`.
4. Enter `ninja` with the target for the platform:

| Target | Result |
| --- | --- |
| `ninja linux` | `build/linux/halo` |
| `ninja linux_arm64` (on 64-bit ARM Linux) | `build/linux_arm64/halo` and `libSDL3.so.0` |
| `ninja linux_x86_64` (on 64-bit x86 Linux, after `python configure.py --linux-x86-64`) | `build/linux_x86_64/halo` and `libSDL3.so.0` |
| `ninja windows` (on Windows, or on Linux after `configure.py --windows-cross`) | `build/windows/halo.exe` and `SDL3.dll` |
| `ninja android_apk` | `port/android/app/build/outputs/apk/debug/app-debug.apk` |
| `ninja switch` | `build/switch/halo.nro` and `build/switch/halo_guest.elf` |

The Switch needs two toolchains and neither can be replaced by the other.
devkitPro builds the host (`devkitA64`, `libnx`, `switch-sdl2`,
`switch-mesa`, `switch-libdrm_nouveau`), and the Android NDK builds the game
image, which is ILP32 AArch64 - 32-bit pointers, because the Xbox data
formats embed them and have to keep their layout. devkitA64 cannot build it,
being LP64 only. So the Switch build wants the same NDK as the Android build,
found as it is there, plus devkitPro:

```
sudo dkp-pacman -S --needed devkitA64 libnx switch-sdl2 switch-mesa switch-libdrm_nouveau
```

`configure.py` finds devkitPro through `DEVKITPRO`, or `/opt/devkitpro`, or
`~/devkitpro`. Both files are needed to run: the NRO is the program, and the
ELF is the game image it loads from the card.

If you enter `ninja` without a target, ninja builds the game for the
computer that you use: on 64-bit x86 Linux, the x86 build (`ninja linux`).

`tools/ci_build.py` makes the same builds as GitHub Actions. For example,
enter `python tools/ci_build.py linux release`.

### Build options

Give these options to `configure.py`:

| Option | Result |
| --- | --- |
| (none) | A debug build. A failed assertion stops the game. |
| `--release` | A release build. The game does not examine assertions, as in the retail game. |
| `--portable` | The Linux and Windows builds operate on all x86-64 processors. The Linux build also operates on older distributions and on SteamOS: refer to "Portable build" in [port/linux/README.md](port/linux/README.md#portable-build). Use this option for builds that you give to other persons. |
| `--lto=thin`, `--lto=off` | Less link-time optimization. The link is faster. |
| `--pgo=off` | No profile-guided optimization. |
| `--pgo=train` | Records a new optimization profile. Refer to "Optimization profiles". |
| `--profile` | A profiling build. It records CPU times. Refer to "Profiling builds". |
| `--web-emcc PATH` | The Emscripten `emcc` for `ninja web` (else the one on the PATH, or `~/emsdk`'s). |

Without `--portable`, the Linux and Windows builds use all the instructions
of the processor that builds them (`-march=native`). Such a build does not
always start on a different computer.

### Optimization profiles

The builds use profiles of the game to optimize the code:

- `pgo/halo_linux.profdata` for Linux and Android.
- `pgo/halo_windows.profdata` for Windows.

The profiles need clang 22 or later. With an older clang, the builds do not
use the profiles.

To record a new profile:

1. Delete the profile.
2. Enter `python configure.py --pgo=train`.
3. Enter `ninja linux` or `ninja windows`.

The build then plays the main menu and the first minute of each campaign
level. This procedure continues for approximately 15 minutes. The game
data must be in `assets/`.

### Profiling builds

A profiling build records where the game spends its time.

1. Enter `python configure.py --profile`, then build as usual.
2. Start a recording. Enter `profile_record` in the console (or the telnet
   console), or set `debug.profile_record = true` in `config.toml`. To
   record each game of a session, also set
   `debug.profile_record_when = "game"`.
3. Stop the recording. Enter `profile_stop`, load another map, or quit.

The game writes numbered `.part<n>.json` files under `profiles/` in its data root
and logs their paths. Parts are retained after the recording stops. Logs go to
standard error; a Windows release build without standard error writes them to
`halo.log` next to `halo.exe`.

`tools/net_report.py` is a helper for developers. Enter
`python tools/net_report.py <recording.part1.json>` to read a recording
as tables. The command also writes `<recording>.summary.txt`, a short text
file.

| Command | Result |
| --- | --- |
| `profile_record [seconds]` | Starts a recording. With a number of seconds, the recording stops after that time. |
| `profile_stop` | Stops the recording. |

`debug.profile_memory` sets the memory of a recording in MB (default 256,
from 4 to 1024). A recording has no length limit:
the game writes a part each time the memory is full. Refer to "Settings" in
[port/linux/README.md](port/linux/README.md).

On Android, the files are in `/sdcard/Android/data/com.halo.decomp/files/profiles/`.
Enter `adb pull` to copy the recording parts to a computer.

A profiling build plays with normal builds. It does not use
`--pgo=train`.

## Credits

- The decompilation: [punpckhdq/halo](https://github.com/punpckhdq/halo),
  continued in [bnunu/halo-1](https://github.com/bnunu/halo-1). It is of the
  Xbox build 2342 (`cachebeta.exe`, SHA-256
  `4cc87b45f721270392a96f1674ed2b5cd4a7bb4355faeab4531d1cf1884d9520`).
- [OpenCE](https://github.com/OpenCommunityEdition/OpenCE), the upstream
  port to Windows and Linux, which these ports follow and share the game
  code, netcode and menus with.
- [Mesa](https://mesa3d.org/)'s Turnip driver, in the builds of
  [K11MCH1/AdrenoToolsDrivers](https://github.com/K11MCH1/AdrenoToolsDrivers).
- [deko3d](https://github.com/devkitPro/deko3d), [libnx](https://github.com/switchbrew/libnx)
  and devkitPro, for the Switch.
- [SDL](https://www.libsdl.org/), on both platforms.
- [Kenney](https://kenney.nl/)'s [Input Prompts](https://kenney.nl/assets/input-prompts)
  (CC0), the controller buttons' pictures on the Switch and the PS5, and
  a replay's button prompts everywhere.
- AL2009man's [Gamepad Asset Pack](https://github.com/AL2009man/Gamepad-Asset-Pack)
  (MIT), the controllers of the button-layout overlay (Gamepad Setup's
  BUTTON LAYOUT).

Halo is a trademark of Microsoft. This project is not affiliated with or
endorsed by Microsoft, Bungie or 343 Industries.

## License

This fork includes OpenCE's code, which OpenCE moved to the GNU Affero
General Public License, version 3 or (at your option) any later version
(`AGPL-3.0-or-later`), in its commit `4b09168e7`. The full text is in
[LICENSE](LICENSE). The commits before that change came into this fork,
and the releases built from them (up to Build 106), were released under
[CC0 1.0](LICENSES/CC0-1.0.txt) and stay that way. Third-party code under
`port/third_party`, and the fonts and art under `port/assets`, keep their
own licenses.
