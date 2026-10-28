# The Tag Injector

The Tag Injector takes weapons, AI and equipment out of Halo Custom Edition
maps and puts them in profiles, which the campaign randomizer adds to a level: a
Custom Edition map's carbine lying where a plasma rifle was, its brutes in
the level's encounters. A tag is taken out with every tag it names (its
model, animations, collision, HUD, projectiles, effects, sounds and bitmaps;
an actor variant's actor, unit, weapons and equipment), as a tag bundle
(`docs/custom_edition_caches.md`, "Tag bundles").

The game's code is `port/linux/game/tag_injector.c` (the menu's maps and
tags, taking tags out, adding them to a level), `port/linux/src/port_tag_profiles.c`
(the profiles' folder), `port/linux/game/menu_functions.c`'s "port tags"
functions (the menu) and "randomizer tag profile" ones (the screen before
the randomizer's), and `port/linux/game/randomizer.c` (the pools).

It needs Custom Edition maps to run (`game.custom_edition`, on by default)
and the maps in `custom_maps`, with Custom Edition's `bitmaps.map` and
`sounds.map` (`README.md`, "Custom Edition maps").

## The menu

**Settings > Gameplay > Tag Injector** is laid out as the Mods screen is: a
list on the left, and in the frame on the right what the row chosen is and
what A does with it. B, or **BACK**, goes back up a list, and from the
first leaves. Its first list has:

- **TAG INJECTION**: ON lets the randomizer add a profile's tags
  (`game.tag_injection`). A, left or right turns it on or off.
- **TAG PROFILES**: the profiles, each with the size of its folder (what
  its tags share counted once; EMPTY without any).
- **MULTIPLAYER PROFILES**: how a multiplayer game plays a profile's
  weapons (below, "In multiplayer").
- **MAPS**: the Custom Edition maps.

**TAG PROFILES**: A on a profile lists its tags' kinds, as a map's are
(**WEAPONS**, **AI**, **EQUIPMENT**, **VEHICLES**, **BIPEDS** and
**SCENERY**, each with how many of them it has), and A on one lists them,
each with where it came from (the frame) and its size, ON or OFF, or
MISSING when its bundle is not there. Left or right turns a tag on or off: one off stays in the profile and
is left out where the profile is played, to try the profile without it. A on
a tag opens its settings:

A weapon or an AI has these; equipment, which joins no pool, has NAME,
ENABLED, REPLACE ORIGINAL and REMOVE TAG alone.

- **NAME**: the name the menus show it by (its list, its frame, the
  profile's tags), which A types; nothing typed is the last part of its
  path. Its path stays its map's, which its bundle has and a level knows it
  by. Its custom type is its name, unless it has one of its own, and AUTO
  reads its name's words too: a protected map's `weapon 2` named
  `Sniper Rifle` is a precision weapon.
- **ENABLED**: on or off, as left and right on the tag.
- **FACTION**: the pools of the randomizer it joins. An actor variant's:
  COVENANT, FLOOD, HUMAN, or ANY POOL ONLY; a weapon's: COVENANT or HUMAN.
- **COUNTS AS** (an actor variant's): the campaign's type it is among for
  ONE TYPE (as ELITE, it is one of the elites a pool of elites plays), or
  NONE.
- **CLASS** (a weapon's): PRECISION, AUTOMATIC, POWER or NONE, the weapon
  pools of those names.
- **CUSTOM TYPE**: the name the randomizer's ONE TYPE's TAG INJECTOR chooses
  it by. A types it; nothing typed is its name. Tags of
  the same custom type (BRUTE for a brute minor, a brute major and a brute
  captain) are played together.
- **HUD** (a weapon's): the HUD it is drawn with, whoever carries it: an AI
  of the profile that carries it adds it as its own tag (MAPS, below), whose
  HUD this is. OWN, the default, is its map's. STOCK: AUTO
  and the campaign's weapons (ASSAULT RIFLE, PISTOL, SNIPER RIFLE, ROCKET
  LAUNCHER, SHOTGUN, FLAMETHROWER, PLASMA RIFLE, PLASMA PISTOL, NEEDLER)
  draw it with that weapon's HUD: its crosshairs, ammo meters and zoom
  overlay. The weapon zooms, fires and reloads as it did: a stock HUD only
  changes what is drawn. AUTO chooses by its path's words (sniper or beam:
  the sniper rifle's; needle: the needler's; rocket, launcher or fuel rod:
  the rocket launcher's; shotgun or mauler: the shotgun's; flame: the
  flamethrower's; plasma pistol, plasma rifle, pistol or magnum), else by
  the weapon: one that zooms 4x or more the sniper rifle's, one that zooms
  less the pistol's, one without a magazine (a battery or heat) the plasma
  rifle's, and the rest the assault rifle's.
- **REPLACE ORIGINAL** (a tag of a path a campaign map has: an energy
  sword, an elite major): ON, it takes the place of the campaign's tag
  wherever the level has it, with the tags it names as its map has them
  ("In a level", below); OFF, the default, the level's stays. The campaign
  maps are read for their paths the first time a tag's settings open.
- **REMOVE TAG**: A, twice, takes it out of the profile and deletes its
  bundle.

AUTO, each setting's first, finds it by the tag's name as the randomizer
finds the campaign's: an actor variant's faction and type by its unit's
(a grunt, elite, jackal, hunter or engineer is the Covenant's, a flood form
the Flood's, a marine human; a brute is none of them, and is played by ANY
and TAG INJECTOR alone until its faction is set), a weapon's by the
campaign's weapon of its path, else by its name's words (plasma, needle,
carbine: the Covenant's; sniper, pistol: precision; rocket, shotgun,
sword: power; rifle: automatic).

**MAPS** has **ADD TO**, the profile a map's tags are added to (the one
opened last in TAG PROFILES, or made; left and right choose another), and
**CUSTOM SINGLEPLAYER** and **CUSTOM MULTIPLAYER**, as the map lists have
them: A on one lists its folders of `custom_maps` (each with how many maps
it has) and the maps beside them, and A on a folder lists its maps. A on a
map loads its tags, as the game would play them, and lists its
**WEAPONS**, **AI**, **EQUIPMENT**, **VEHICLES**, **BIPEDS** and
**SCENERY**, each with how many there are. A on one lists them, by the last
part of their paths (the frame has how many tags it requires: itself and
what it names). A on a weapon, an actor variant or equipment adds it to the
profile (ADDED), or takes it out again. An actor variant added adds the
weapons and equipment it carries too, each a tag of the profile of its own
(one the profile has of that path is left as it is), so that their HUD and
replacing are set there; taking the actor variant out leaves them. The
randomizer plays weapons and AI; equipment joins no pool, and is played as
an AI's, or replacing the original. Vehicles, bipeds and scenery are listed,
and cannot be added yet.

The button bar beside BACK goes by the list shown: nothing in the first
list; **NEW PROFILE** in TAG PROFILES, which asks for a name (letters,
digits, spaces, `-` and `_` make its folder's name) and chooses it; in a
profile's lists **DUPLICATE PROFILE**, which asks for a name and makes a
copy of the profile (its tags, their settings and its pack), and **DELETE
PROFILE**, which, pressed twice, deletes it and its tags; and in MAPS'
lists NEW PROFILE and **VIEW <name> PROFILE**, which shows the profile MAPS
adds to as A on it in TAG PROFILES does, B from there going back to the list
it was pressed in (a map's weapons, still open).

**MULTIPLAYER PROFILES** lists the multiplayer profiles, each with the
profile whose weapons it plays; **NEW PROFILE** asks for a name and makes
one playing the profile shown last in TAG PROFILES. A on one opens its
settings:

- **NAME**: the name the menus and Server Setup show it by, which A types.
- **TAG PROFILE**: the profile whose weapons on it plays (its AI and
  equipment are not played). A, left and right choose another.
- **ROTATION**: **ADD TO MAP'S**, the profile's weapons join the map's, so
  that a weapon of the map may stay the map's; **PROFILE'S ONLY**, the
  map's weapons are all swapped for the profile's.
- **REPLACEMENT**: how the map's weapons are swapped. **FULLY RANDOM**: a
  weapon at random each time one spawns. **ONCE PER POSITION**: each place
  a weapon spawns rolls one the first time, and spawns it all game.
  **ONCE PER WEAPON**: each of the map's weapons rolls one the first time
  it spawns, and every one of it is that (every shotgun the same).
  **MANUAL**: below it, a row for each of the map's weapons (ASSAULT
  RIFLE, PISTOL, SNIPER RIFLE, ROCKET LAUNCHER, SHOTGUN, FLAMETHROWER,
  PLASMA RIFLE, PLASMA PISTOL, NEEDLER), with how many weapons are chosen
  for it (AS IT IS: none, and it stays the map's); A on one lists
  **ORIGINAL** (the map's own) and the profile's weapons on, each turned on
  or off by A, left or right. One of those on spawns at random each time.
  With ADD TO MAP'S, the original is among them whether or not it is on.
- **LOADOUT**: the weapons each player spawns with. **GAMETYPE'S**: as the
  gametype gives them. **RANDOM**: two different weapons of those the map's
  are swapped for, each spawn. **SET**: **PRIMARY** and **SECONDARY**
  below it, each NONE, one of the map's weapons (MAP'S PISTOL: when the map
  has it) or one of the profile's.

In a multiplayer profile's lists the button bar has **DELETE PROFILE**,
which, pressed twice, deletes the multiplayer profile (not its profile).

Under the details of a map's tag (a kind's list in MAPS) and of a
profile's (a kind's list in TAG PROFILES), the frame shows the tag's model,
turning (`port/linux/game/tag_preview.c`): a weapon's, equipment's, a
vehicle's, a biped's or scenery's own, an actor variant's its unit's, in
its default pose: the most detailed geometry of each region's first
permutation, its opaque parts alone (all of them when it has none, as an
energy sword), each part with its shader's base map (a model's or an
environment's shader's; grey without one), tinted by the change color a
model's shader names where its multipurpose map's color change mask says:
an actor variant's change colors, else its unit's (the first of each's
permutations), as the game draws an object of it (the grunt major, minor
and spec ops of one model, red, orange and black). The game draws it on the
processor, lit from above with blue edges, into a picture the frame's
`tag_preview` widget shows, once the row has been chosen for a moment (a
profile's tag is read from its bundle). Under it, `tag_needs` says what of
what the tag needs to work is MISSING, else "No data missing": a weapon
needs its model, its collision
(lying on the ground), its first-person model and animations, its
projectile (none: a melee weapon's) and its HUD; an actor variant its
behaviour (its actor), its unit, the unit's model, animations and
collision, and its weapon (none: unarmed); equipment its model and
collision; a vehicle or a biped its model, animations and collision;
scenery its model (and its collision, which it may have none of). The
details above them leave out their blank lines and stop at six; a profile's
tag's say the map it came from, not its path. A tag whose model cannot be read shows nothing
there.

A profile's tag a level could not play (its bundle missing or unreadable,
not one this build plays, past the room a level has, or adding a weapon
without a first-person model, a bitmap this build cannot draw or a model it
cannot make) is turned off, and its `failed` setting says why: its row says
FAILED, and its details (and its ENABLED's) say it failed to load last time
and why. Its row, its kind of tags', its profile's and TAG PROFILES' are
yellow, and its kind's list has the tags that failed first. Turning it on
again clears it, to try it again; it keeps its place in the list until the
list is opened again. Tags past the 64 a
level adds are left out but not turned off. Two seconds into the level
played (past its opening cutscene, its players in their bodies), each local
player's HUD says how many failed and which, by name, without stopping the
game.

A weapon without a first-person model or animations cannot be added: its
map never let a player hold it, and the game reads them as the player holds
it (a debug build stops). A profile's bundle that would add one, as an AI's
weapon, is left out where the profile is played.

A protected map's tags have no paths of their own (they are all
`<protected>`, nothing, or one path many share), which the menu lists them,
keeps them in a profile and adds them to a level by. As the map opens, each
is given a name made from what it is (`docs/custom_edition_caches.md`, "Tag
bundles"): a weapon its pickup text (`battle rifle`), a biped the stock
folder of its tags (`elite`), an actor variant its unit's and its weapon's
(`elite battle rifle`), numbered where two are the same (`battle rifle 2`).
The frame says so, and NAME renames it once it is in a profile.

Adding a tag loads its map again, with the sounds of the tags it takes
decoded: a second or two, and longer for an actor variant of many voices
(the brute major carbine of `firefight-fissurefall` takes 515 tags, 43 MB).

## Profiles

The profiles are in the `tag_injector` folder of the data root (the folder
with `maps/`; on Android the app's data folder, on the web the browser's
storage for the site). Each profile is a folder, with a `profile.toml`, a
tag file for each of its tags, and its pack:

```
tag_injector/
  Brutes/
    profile.toml
    carbine.weap.tagref
    brute major carbine.actv.tagref
    tags.pack
    tags.index
```

A tag's bundle (`docs/custom_edition_caches.md`, "Tag bundles") is kept in
two parts (`port/linux/src/port_tag_store.c`): its tags in its tag file,
and its file data, the pixels, samples and vertices they name, in the
profile's pack, a region at a time (a bitmap's pixels, a sound's samples, a
model part's vertices), each region once however many of the profile's tags
have it. The tags of one map share much (their effects, sounds and shaders:
a weapon's impacts on every material), which a bundle each held a copy of;
the tags and each region are compressed too (zlib) where that makes them an
eighth smaller. Five tags of `bleakness-firefight` (a battle rifle, a
carbine and three brutes), 136 MB of bundles, are 21 MB so. A region no tag
has any more is let go when a tag is taken out (the pack is written again
when they are a quarter of it). `tags.index` is where the pack's regions
are: one lost, or not the pack's, is made again from the pack.

A bundle as it was taken out (`carbine.weap.tagbundle`, as an earlier build
kept them, or `ce_tags extract` writes) is played as it is. The Tag
Injector's menu stores such bundles while it is open, one a frame, saying
how many are left and how much smaller the profiles are; one that cannot be
is kept as it is (the log says why).

```toml
name = "Brutes"
format = 2

[[tags]]
map = "firefight-fissurefall"
group = "weap"
tag = "cmt\\weapons\\covenant\\carbine\\carbine"
file = "carbine.weap.tagref"
enabled = true
faction = "covenant"
class = "automatic"
custom_type = "Carbine"
hud = "auto"
replace = true
name = "Brute Carbine"
```

A tag's `enabled`, `faction`, `type` (an actor variant's), `class` (a
weapon's), `custom_type`, `hud` (a weapon's), `replace` and `name` are its
settings in the menu, and `failed` why the level last loaded could not play
it (`"missing"`, `"unreadable"`, `"too large"`, `"unplayable"`,
`"no memory"`, `"not holdable"`, `"bitmaps"` or `"models"`); one left out is its default (on, AUTO, its name, OWN,
off, the last part of its path, and not failed). An actor variant's `hud`, which an earlier build gave
its weapons, is no longer read: its weapons' own tags have theirs. The values are the
randomizer's names: `faction` `"covenant"`, `"flood"`, `"human"` or
`"none"`; `type` one of `game.randomizer_type`'s (`"elite"`) or `"none"`;
`class` `"precision"`, `"automatic"`, `"power"` or `"none"`; `hud` `"auto"`,
`"assault_rifle"`, `"pistol"`, `"sniper_rifle"`, `"rocket_launcher"`,
`"shotgun"`, `"flamethrower"`, `"plasma_rifle"`, `"plasma_pistol"` or
`"needler"`.

A profile holds its bundles, not the maps they came from: it is played
without them, and can be copied to another machine whole. A folder without
`profile.toml` is not a profile, nor is one made for a later `format` (a
profile with tag files is of format 2, which an earlier build does not
list; one of bundles alone stays 1). A
tag whose bundle is missing, or is not one this build reads, is left out
where the profile is played, and the game's log says why.

Limits: 64 profiles, 256 tags a profile.

The multiplayer profiles are files of `tag_injector/.multiplayer` (64 at
most), each its name, the folder of its profile and its settings:

```toml
name = "Shotgun Madness"
profile = "Brutes"
rotation = "add"
replace = "manual"
loadout = "set"
primary = "cmt\\weapons\\covenant\\carbine\\carbine"
secondary = "stock:pistol"

[manual]
shotgun = ["original", "cmt\\weapons\\covenant\\carbine\\carbine"]
```

`rotation` is `"add"` or `"only"`; `replace` `"random"`, `"position"`,
`"weapon"` or `"manual"`; `loadout` `"map"`, `"random"` or `"set"`;
`primary` and `secondary` a weapon of the profile by its path, one of the
map's as `"stock:"` and its key, or `""` for none; `[manual]` the weapons
chosen for each of the map's by its key (`assault_rifle`, `pistol`,
`sniper_rifle`, `rocket_launcher`, `shotgun`, `flamethrower`,
`plasma_rifle`, `plasma_pistol`, `needler`), `"original"` the map's own. A
setting left out is the first of its values.

## In a level

With **TAG INJECTION** on, the difficulty of a campaign level the randomizer
plays opens, before the randomizer's screen, **TAG INJECTOR PROFILE**: NONE
and each profile, A one to play it (`game.randomizer_tag_profile`), then the
randomizer's screen, whose CANCEL comes back to it. It is shown with any
profile, one alone too, so a level can be played without its tags; with
none, none is played.
The randomizer's screen has the profile's choices only while it has tags on
of their kind:

- **ENEMY POOL**'s **TAG INJECTOR**: the profile's AI alone.
- **ENEMY TYPE**'s **TAG INJECTOR** (with ONE TYPE), and below it **CUSTOM
  TYPE**, which left and right choose among the custom types of the
  profile's AI: that one alone (`game.randomizer_custom_type`).
- **PICKUP POOL**'s and **WEAPON POOL**'s **TAG INJECTOR**: the profile's
  weapons alone; **PICKUP TYPE**'s and **WEAPON TYPE**'s **TAG INJECTOR**,
  with their CUSTOM TYPE: one custom type of them
  (`game.randomizer_pickup_custom`, `game.randomizer_weapon_custom`).

The profile's tags on join the other pools too, as their settings say: ANY
all of them, COVENANT, FLOOD and HUMAN those of their faction, PRECISION,
AUTOMATIC and POWER the weapons of their class, and ONE TYPE's campaign
types the AI that count as them (and are of the rank chosen, by their
names). A setup saved (SAVE) keeps the custom types.

A campaign level the randomizer swaps is given the profile's tags on as it
loads, after those the randomizer copies from the other campaign maps (none
for a pool of the profile's alone):

- each tag of a bundle that the level has not got (a tag of its group and
  path) is added to the level's tags, in the profile's order; one the level
  has is the level's own, and so is one an earlier bundle added;
- each reference and tag index of a tag added names the level's tag;
- a tag with REPLACE ORIGINAL on whose path the level has (its own, or a
  tag the randomizer copied) is given to the level where that tag is, in its
  place: the scenario's palettes, the actor variants that carry it and the
  randomizer's pools have it now. Its other tags are its own, added though
  the level has tags of their paths (its projectile, its model, its sounds
  as its map has them), but for one a bundle before it has replaced, which
  it shares. Of two tags replacing the same one, the first does; one whose
  path the level has not got is added as the others are;
- a weapon added whose HUD setting (its own tag's in the profile, whichever
  bundle adds it) is a stock one names that HUD instead of its own (which is added, and not drawn). A level without that HUD gets it
  from the first campaign map that has it (the sniper rifle's from a30, the
  rocket launcher's from b30, the shotgun's from c10, the flamethrower's
  from c20, the needler's from a30, the others' from a10), as the
  randomizer copies its weapons; `randomizer.pack` keeps no HUD bitmaps, so
  their pixels are read from that map as the level loads (16 MB of them at
  most). The profile's bundles are read before the randomizer copies its
  tags, so that it knows the HUDs to copy. Without the HUD (its map is not
  there), the weapon keeps its own, and the log says so;
- a gbxmodel is made this build's model, as a Custom Edition map's are
  (compressed vertices, in buffers of its own);
- a bitmap's pixels and a sound's samples are read from the bundle's file as
  the game wants them, the pixels laid out for the texture cache as a Custom
  Edition map's are, with their channels where Halo PC keeps them (HUD
  meters, multipurpose maps). A bundle's tags alone are kept in memory, 64
  MB of them at most, from 64 bundles at most;
- with `game.tag_injection_textures` `"small"`, or `"auto"` (its default)
  where the texture cache is the Xbox's 22 MB (the web build's), a bitmap
  with mipmaps is drawn from those of 256 pixels at most: Custom Edition
  weapons' bitmaps are often uncompressed 512s (a battle rifle's 16 MB of
  them), which filled the cache, and what did not fit was drawn as the
  default texture, bands of green, yellow and red (`YOU GOT STABBED` in the
  log). `"full"` draws them as they are. A bitmap without mipmaps (a HUD's)
  is drawn as it is.

A bundle whose model cannot be made, or whose bitmap cannot be drawn, is
left out; when the tags would not fit in the level's table, none are added.

Only a level played alone adds them: a network game's machines must have
the same tags, and the profile is this machine's (Server Setup's randomizer
has none of the TAG INJECTOR choices, and a setting of one is taken as ANY;
a multiplayer game shares its host's weapons instead: "In multiplayer",
below).
A daily challenge, or a random run, never adds them: they are the same for
everyone. A saved game is loaded with the tags the setup gives the level
then, as the randomizer's are: one saved with a profile's tags expects the
same profile.

A multiplayer game plays a profile's weapons another way (below).

## In multiplayer

A multiplayer game hosted on one of the game's own multiplayer maps (not a
campaign level, nor a Custom Edition map) can play the weapons of a
profile, as a multiplayer profile says (`port/linux/game/tag_multiplayer.c`).
With TAG INJECTION on and a multiplayer profile made, the gametype chosen
for such a game opens a **TAG PROFILE** screen before Server Setup: NONE
first, then the multiplayer profiles (the list starts on the one chosen
last, `game.multiplayer_tag_profile`). What is chosen there is the game's
until it ends, and decides who sees it:

- **NONE**: the map's own weapons. The game is a plain one, listed for and
  joined by every build, upstream's too.
- **A multiplayer profile**: a modded game (below), which only builds with
  the mod see and join. It needs Custom Edition maps on
  (`game.custom_edition`) on every machine; without them its row is
  refused.

Server Setup's **TAG PROFILE** row shows the choice, and A on it (or B)
goes back to the screen to choose again; nothing else changes it while the
game is hosted (config.toml's setting is only the next game's starting
point), so players who joined a plain game are never put into a modded one,
nor the other way. With TAG INJECTION off, no multiplayer profile, or
another map, the screen is skipped and the game plays none. A dedicated
server takes `game.multiplayer_tag_profile` (with `game.tag_injection` on)
as its game is hosted.

- **The share.** As the game is hosted (and a moment after the profile
  chosen, or its files, change, outside a level), the host writes the profile's weapons on as
  one file, its archive (`tag_injector/.shared/<id>.tagshare`, named for
  its hash's first 16 hexadecimal digits): their tag files and the
  profile's pack and index as they are, nothing compressed again (the pack
  has its AI's pixels and samples too), and a `profile.toml` of the weapons
  alone, with their settings (their HUD, REPLACE ORIGINAL, their names),
  and the multiplayer profile played (`multiplayer.toml`).
  It plays the archive unpacked into `.shared/host`, as each player plays
  its own copy. A share of the same weapons is not made again for the next
  game; a level loading before the share was made (a lobby a moment long)
  makes it then. The host tells each
  of its players the hash (`_distributed_message_tag_share`), in the lobby
  too, and a player without that share downloads its archive as a Custom
  Edition map is downloaded (`port/linux/NETCODE.md`, "Map sharing": from
  the host and the other players), and makes it the share's folder again.
  The player's game waits for it as for a map, whose download bar says
  "Downloading the host's tag profile". One that cannot download it
  (`network.map_download` or `game.custom_edition` off) leaves the game,
  saying why.
- **A copy of one's own.** A share downloaded is copied, once, into the
  player's TAG PROFILES as the host's profile's name and " (online)" (its
  weapons, their settings and its pack), and its multiplayer profile into
  MULTIPLAYER PROFILES, named so too and playing that copy: they are the
  player's to change, play alone or host with. The game itself plays the
  share as it came, whatever is done to the copy. A share downloaded before
  copies were made is copied as the Tag Injector's menu opens (without a
  multiplayer profile, which those shares did not carry). A marker beside
  the share (`<id>.imported`) keeps a copy deleted from coming back.
- **The level.** As the map loads, every machine adds the share's tags to
  it as a profile's are added to a campaign level ("In a level", above:
  the stock HUDs its weapons are drawn with copied from the campaign maps
  when the map has not got them), before its menus' and mods' own tags, so
  that they are at the same indices on every machine. A share's tag a
  machine could not play is not turned off in it (it is the host's), but
  that machine's level then differs from the host's: the host checks each
  player's tags as it does a modded co-op game's, and the player's HUD says
  so.
- **The weapons.** The host alone spawns the map's weapons and gives its
  players theirs (the players have the host's objects). Each of the map's
  weapons it spawns (its netgame equipment's; not the flag, the ball,
  grenades or powerups) is swapped as REPLACEMENT says, among the share's
  weapons, and the map's with ADD TO MAP'S (those its netgame equipment
  spawns). Each player spawning is given LOADOUT's weapons, its others
  taken away, unless it is GAMETYPE'S; grenades are the gametype's. The
  gametype's NO WEAPONS ON MAP spawns none of them. A gametype that gives
  its weapons itself (Gun Game, Infection) keeps its own.

A game playing a multiplayer profile is a modded game
(`port/linux/NETCODE.md`, "Modded games"): only a build with the mod joins
it. A profile's tag of a later Tag Injector, or a bundle a player's build
reads otherwise (a texture its renderer cannot draw), makes that player's
level differ from the host's.

Not built yet: Custom Edition maps and the campaign; a profile's AI or
equipment. Downloaded shares stay in `tag_injector/.shared`
until deleted by hand.

## The Tag Editor

The overlay menu's **TAG EDITOR** (L, R and the d-pad's down) changes the
weapon the first player holds, when it is one of the played profile's. Its
first list has three:

- **Weapon stats**: the weapon's own values (zoom, heat, autoaim), its
  triggers, magazines and projectiles (their damage below them), and its
  melee damage.
- **HUD**: its HUD and the parent HUDs it draws too (below).
- **Tag settings**: **Name**, the name the profile shows it by (A types it
  over the game: the d-pad's up and down change the letter chosen, left and
  right choose another, Back deletes the one before, A keeps it, B does
  not), and **Remove from profile** (A twice: the level loaded keeps it
  until it loads again).

Each change is kept in the weapon's tag of the profile, under
`[tags.edits]` in `profile.toml`, a number by its key
(`port/linux/game/tag_editor.c` lists them), and every level that adds the
weapon gives it its edits; a network game's clients play the host's.

**HUD** lists the weapon's HUD, and below it each **PARENT HUD** the HUD
draws too (a Custom Edition HUD often keeps its visor, its ammunition
counter's frame or its grenades in parent HUDs, shared by every weapon of
the mod): their anchor, and each element with its bitmap's name. A on an
element moves it over the game (the d-pad; A switches to its scale, Back
puts it back, B is done); left or right on it hides it, and shows it again.
A parent HUD's elements are its keys under `hud.parent1.` (its parent),
`hud.parent2.` (that one's) and so on; a hidden one's `hide` is 1.

A weapon given edits plays a copy of each tag they change (its HUD, and a
parent HUD changed: every HUD up to it), named for its edits, so that the
weapons that share them keep theirs; weapons edited alike share one copy.
While the level plays, a change is made to the tags it plays at once: a
parent HUD shared with other weapons (**Edits apply to** names them)
changes for them too until the level loads again.

## The firing range

A profile's list ends with **FIRING RANGE**: A, from the main menu's
settings, plays a level alone to try the profile's tags in
(`port/linux/game/firing_range.c`). Its parts, from the start in the
middle facing north:

Each side faces its other half: the weapons west and the AI east cover the
same 12 units north to south, and the arena north and the mirror south are
as wide as each other (16 units), 11 units from the start either way.

- **West**, the weapons: each of the profile's weapons the level could add,
  once, laid on benches (twelve to a bench, 12 units long, the first 4.5
  units west of the start, the next 2.5 further each, 96 weapons at most). A
  weapon taken is laid there again 3 seconds after its place is empty.
  Past each bench's north end (its first weapon's) stands a dummy: the
  multiplayer player's biped, unarmed, with a multiplayer player's health
  and shield (no difficulty or skull scaling, the shield recharging as in
  multiplayer, and multiplayer headshots), so a weapon's kill time is
  multiplayer's. As it dies the HUD says how long it took from the first
  hit; it is stood again 3 seconds later, healed 5 seconds after it was
  last hurt, and put back when a blast pushes it. The AI leave it be.
- **East**, the AI: each of the profile's actor variants the level could
  add, once (32 at most), stands in columns of four from 4.5 units east of
  the start (the next 2.5 further each), facing you, unarmed, playing every
  base animation of its graph in turn (each name once), and nothing hurts
  it. Walking up to one, the HUD says "Press action to fight" it; action
  puts a live one of it in the arena, holding one of the profile's weapons
  by chance (its own when the profile has none). Eight fight at once at
  most: the ninth takes the oldest's place.
- **North**, the arena: 16 by 14 units, walled 1 high, a door in its south
  wall. Its AI walk the floor (its only walkable surface, so they do not
  path round its walls) and have no encounter or firing positions. The pad
  east of its door outside clears it: standing on it, the HUD says so, and
  action takes away the bodies in it (and those of the AI fought, wherever
  they fell) and the weapons and items lying in it.
- **South**, the mirror: a frame 11 units south, 16 wide and 1.4 high
  (drawn alone: it does not stop you). Standing up to 5 units before the
  pane (within the frame across), a copy of you stands
  where your reflection is, holding your weapons slot for slot (one picked
  up or swapped is its too, in hand), and moves as you do: it is given your
  controls each tick, turned about the pane, so it walks, jumps, crouches,
  aims, fires, reloads, throws and strikes as you do, seen from the front.
  Stepping back lets it go. Its shots and grenades hurt no one, and nothing
  hurts it.

The level is Blood Gulch (`maps/bloodgulch.map`), for its sky, sounds and
shaders, with its ground never read: the game writes a structure bsp of its
own where the map's would be, a floor walled round 8 units high, reaching
6 units past the mirror's reflection south, the arena north, the farthest
bench west and the farthest AI east (no narrower than the arena) (and solid past the walls, up to a ceiling 100 up), with
the benches its own (their collision and their faces). It
has no lightmap: the floor is drawn with the renderer's own directional
light, and objects are lit by the bsp's default lighting. While it plays,
Blood Gulch's scenery, vehicles, items, netgame flags and decals are taken
out, and its players start at the middle facing north; the next time the
map is loaded it is as it was. The pause menu's QUIT leaves the range.

## The game's log

The game's log says what is taken out and added (`tag injector:`), and why
a map cannot be read, a tag cannot be taken out (a tag it names is of a
group this build cannot bundle), or a bundle is left out.
