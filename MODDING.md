# Modding Hopmon Native

Hopmon Native 1.3 and later can load two kinds of mods:

- **Level packs** are new campaigns: your own levels in worlds of your own, and optionally their
  own textures, models, sounds and music. You play one level pack at a time, or the original game.
- **Add-ons** replace images, 3D models, sound effects, music or original levels. They apply to
  whatever you play, the original game or any level pack, and several can be on at once.

Mods never contain the game itself. Anything a mod doesn't replace still comes from the player's
own `Hopmon.exe`, so every player still needs their own copy of the game.

## Contents

- [Installing and managing mods](#installing-and-managing-mods)
- [Level pack or add-on?](#level-pack-or-add-on)
- [Making a level pack](#making-a-level-pack)
- [Making an add-on](#making-an-add-on)
- [Examples and sharing](#examples-and-sharing)
- [mod.ini reference](#modini-reference)
- [Replacing assets](#replacing-assets)
- [Levels](#levels)
- [Level packs](#level-packs)
- [Exporting the original assets](#exporting-the-original-assets)
- [Asset names](#asset-names)
- [What you may share](#what-you-may-share)
- [Troubleshooting](#troubleshooting)

## Installing and managing mods

The `mods` folder next to `HopmonNative.exe` has a folder for each kind:

```
mods/
  levelpacks/     one folder (or .zip) per level pack
  addons/         one folder (or .zip) per add-on
```

1. Put the mod in the right folder. A mod can be a folder or a `.zip` file; you don't need to
   unzip it. The mod's description says which kind it is. If it goes in the wrong folder, the
   game uses it correctly anyway and notes in the log where it belongs. Mods put directly in
   `mods/` aren't loaded.
2. Start the game and choose **OPTIONS** on the title screen, then **Mods**.

| Item | What it does |
|---|---|
| **Level pack** | Opens the list of campaigns: *Original game* and every installed level pack, with its number of levels. *Playing* marks the one in play. Choose another and confirm: the game reloads and returns to the title screen. Each pack keeps its own progress. |
| **Add-ons** | Opens the list of add-ons in load order, each with a tick box. See below. Add-on changes apply when you close the menu. |
| **Open the mods folder** | Shows the `mods` folder (and creates `levelpacks` and `addons` in it if needed). |

Mods can only be changed from the title screen; during a level these items are greyed out.

In the Add-ons list:

| Keyboard | Gamepad | What it does |
|---|---|---|
| Up / Down | D-pad | Select an add-on |
| Space (or Left/Right) | X / Square | Tick or untick it: ticked add-ons are on |
| Shift+Up / Shift+Down (or Page Up / Page Down) | LB / RB | Move it up or down the list. It moves at once, and the numbers show the new order. |
| Enter | A / Cross | Open its details: author, version, what it replaces, its licence and its place in the load order |

When two add-ons replace the same thing, the one **higher in the list** wins. Add-ons also win over
the level pack in play, the same way they win over the original game. New add-ons are added at the
bottom of the list, switched on. The choices are saved in `hopmon_native.ini` under `[mods]` (see
[CONFIG.md](CONFIG.md#mods)).

Zipped mods are unpacked once into your user folder (`%APPDATA%\HopmonNative\modcache\` on
Windows) and unpacked again only when the zip changes.

## Level pack or add-on?

**Level packs** are for new levels. **Add-ons** are mainly for changing how the game looks and
sounds: textures, 3D models, sound effects and music.

| | Level pack | Add-on |
|---|---|---|
| Purpose | A new campaign | Change how things look or sound |
| New levels and worlds | Yes, any number | No. It can only replace original levels by name (see below) |
| Floors and music with your own file names | Yes, named in its `[world.N]` sections | No. Replacements by [asset name](#asset-names) only |
| Replace images, models, sounds, music | Yes, only while the pack is played | Yes, in whatever is played |
| How many at once | One (or the original game) | Any number, in a load order |
| Chosen in the menu | Mods → Level pack | Mods → Add-ons (on/off) |
| Saved progress | Its own save file | None of its own |
| When both replace the same file | The add-on wins | The add-on wins |

An add-on's `levels/` folder can replace original levels by name (`castle-07.txt` …). Those
replacements only change the original game, and they share the original game's saved progress,
so turning the add-on off or on changes which version of those levels the progress belongs to.
To rework levels, a level pack is usually the better choice: players can switch between it and
the untouched original, it keeps its own progress, and it can have any number of levels.

Neither kind can add new enemies, objects or game rules: levels use the game's own enemies and
objects through the [tiles](#tiles).

## Making a level pack

A level pack is one folder that holds everything it needs:

```
mods/levelpacks/my-levels/
  mod.ini       [mod], [pack] and one [world.N] per world
  levels/       the levels (.txt)
  textures/     optional: floors and replacement images
  models/       optional: replacement models
  sounds/       optional: replacement sounds
  music/        optional: music for its worlds, or replacement tracks
```

The `[pack]` section is what makes it a level pack. Its textures, models, sounds and music are
used **only while the pack is played**: choosing the original game brings back the original look,
with nothing to switch off. See [Level packs](#level-packs) for the worlds and level list, and
[Levels](#levels) for the level format.

## Making an add-on

An add-on is a folder with a `mod.ini` (without a `[pack]` section) and any of these:

```
mods/addons/my-addon/
  mod.ini
  textures/   images        (.png .bmp .tga .jpg)
  models/     3D models     (.glb .gltf .x)
  sounds/     sound effects (.wav .ogg .flac)
  music/      music         (.flac .mp3 .mid)
  levels/     replacements for original levels (.txt), used in the original game only
```

Everything in an add-on is a replacement named after the original it replaces; see
[Asset names](#asset-names). A file with any other name is ignored.

## Examples and sharing

File and folder names are not case sensitive. To share a mod, zip its folder. `mod.ini` may be at
the top of the zip or inside a single folder in it.

Two complete examples come with the game in `examples/mods/`. To try them, copy
`examples/mods/levelpacks/example-pack` into `mods/levelpacks/` and
`examples/mods/addons/example-addon` into `mods/addons/`.

- **example-pack** is a level pack of three levels in three worlds. One world has its own cave
  floor, and the pack has its own crystal, a glTF model.
- **example-addon** replaces Hopmon's fire and its sparks with purple transparent PNGs, the
  level-select lights with a 4× resolution image, and the crystal pickup sound.

Everything in the examples is CC0, so you can start from them.

## mod.ini reference

```ini
[mod]
id = my-mod                 ; lowercase letters, digits, . _ - (becomes a folder name)
name = My Mod
author = Your Name
version = 1.0.0
description = One or two sentences shown in the Mods menu.
license = CC BY 4.0         ; how others may use your work
requires = 1.3.0            ; oldest Hopmon Native it works with (optional)
api = 1                     ; the mod format version; 1 since Hopmon Native 1.3

[models]                    ; optional, per model: see "Models"
crystal = fit

[pack]                      ; level packs only: see "Level packs"
name = My Levels
version = 1.0.0

[world.1]
name = First World
look = castle
levels = intro, maze, finale
```

| Key | Meaning |
|---|---|
| `id` | Identifies the mod in the settings and a level pack's save folder. Defaults to the folder or zip name. Don't change it after release, or players lose their pack progress. `original` and `pack` are reserved. |
| `name`, `author`, `version`, `description`, `license` | Shown in the Mods menu. A level pack is listed by its `[pack]` name. |
| `requires` | If the game is older than this version, the mod is listed as *Can't use* instead of being loaded. |
| `api` | If it's newer than the game understands, the mod is listed as *Can't use*. |

Lines starting with `;` or `#` are comments.

## Replacing assets

Add-ons and level packs replace assets the same way: put a file named after the asset in the
matching folder of the mod. For example,
`textures/floor_castle.png` replaces the castle floor and `sounds/hop.wav` replaces the jump
sound. The names are listed in [Asset names](#asset-names), and
[`--export-assets`](#exporting-the-original-assets) writes every original with its name, ready to
edit.

### Images

- **Formats:** PNG, BMP, TGA or JPEG.
- **Size:** use any size. Images are drawn at the original's size, so a 4× larger image gives
  sharper detail without changing the layout. Keep the original's aspect ratio, and for sheets
  (digits, level lights, floors) keep the same grid of frames.
- **Transparency:** a PNG with an alpha channel uses its own transparency, including soft edges.
  An image without alpha uses the original's transparent colour, exactly as the original did.
- **Floor sheets** (`floor_castle`, `floor_forest`, `floor_toy`, or a pack's own floor) are a
  4 × 4 grid of tiles (64 px each in the originals):

  | Row | Column 1 | Column 2 | Column 3 | Column 4 |
  |---|---|---|---|---|
  | 1 | floor | wall top | wall side | wall side (other direction) |
  | 2 | `#` floor | `=` floor | `*` warp | `!` steam vent |
  | 3 | `[` arrow up | `>` arrow right | `]` arrow down | `<` arrow left |
  | 4 | (unused) | | | |

### Models

- **Formats:** glTF 2.0 (`.glb`, or `.gltf` with its files next to it), or text DirectX `.x`
  like the originals. Binary `.x` is not supported.
- **What's used from glTF:**
  - all triangle meshes in the default scene, with node transforms applied;
  - base colour (factor and texture), emissive colour, and alpha blending.

  Animation, skinning and morph targets are not used. Hopmon animates by switching between
  models (`hopmon`, `hopmon_2`, `hopmon_3`), so replace each frame.
- **Size and position:** the exported `.glb` files use the game's own units and axes, so a
  model built on top of an export lines up exactly. Alternatively, add `<model name> = fit` under
  `[models]` in `mod.ini` to scale and move your model to fill the original's space, whatever
  units you modelled in.
- **Limits:** 65 535 vertices per mesh. Keep models light; many copies can be on screen at once.

### Sounds

WAV (8/16/24/32-bit or float), Ogg Vorbis or FLAC. Use any sample rate, mono or stereo.

### Music

Music in `music/` named `Music01` to `Music04` replaces the original tracks:

| Track | Where it plays |
|---|---|
| `Music01` | title screen, level select and ending |
| `Music02` | castle world |
| `Music03` | forest world |
| `Music04` | toy world |

The formats are `.flac`, `.mp3` or `.mid`. A player's own `music` folder next to the game takes
priority over mods. A level pack's worlds can also choose their own music (see below).

### Levels

`levels/castle-07.txt` replaces the 7th castle level, `forest-01.txt` the first forest level,
and so on (see [Levels](#levels) for the format). A replacement level that breaks a rule is
ignored, and the original level is played instead. Replaced levels only appear in the original
game, since a level pack plays its own levels; see [Level pack or add-on?](#level-pack-or-add-on).

## Levels

A level is a text file with a map drawn in characters, one character per tile:

```ini
[level]
name = Crystal Cave

[map]
...11111111111...
...1$-------$1...
...1---------1...
11111---V---11111
1$-------------$1
1---m-------m---1
1-------*-------1
1-------@-------1
1!-------------!1
11111111111111111
```

- The `[level]` header is optional; a file holding only the map works too.
- The map ends at the end of the file, at an empty line, or at a line starting with `_`.
- Spaces count as `.` (empty). The first line is the top of the map on screen.

### Tiles

**Floor and walls**

| Char | Tile |
|---|---|
| `.` | nothing (a hole: nobody can walk there) |
| `-` | floor |
| `+` | floor that enemies can't enter (only Hopmon can walk on it) |
| `#` `=` | floor with the sheet's alternative pictures (decoration only) |
| `1` `2` `3` | wall, 1, 2 or 3 tiles high |
| `*` | warp: bring crystals here (stand on it to deliver them) |
| `[` `>` `]` `<` | arrow floor: pushes Hopmon up, right, down, left |
| `@` | Hopmon's start (on floor) |
| `$` | crystal (on floor) |

**Enemies** (each stands on floor)

| Char | Enemy |
|---|---|
| `a` | ghost |
| `b` | cube |
| `c` | jellyfish |
| `d` | UFO (flies over walls up to 2 high and most objects, but not over holes, poles or 3-high walls) |
| `e` | sesame |
| `m` / `n` | needle ball, starting up / down; it follows walls |
| `o` / `p` | fast needle ball, starting up / down |
| `q` | needle box |
| `r` | dropping needle box |
| `x` / `y` | gate (two orientations); Hopmon can't pass it |

**Objects**

| Char | Object |
|---|---|
| `A` | cannon that fires upwards |
| `N` `E` `S` `W` | cannon facing north, east, south, west |
| `V` | volcano (also takes up the 8 tiles around it, so draw those as floor) |
| `P` | pole: a pole, tree or short pole depending on the world's look |
| `!` | steam vent: a floor tile that blows steam |

Objects other than `!` block movement and fire.

### Rules

A level is only used if it follows all of these rules. Otherwise the log says what's wrong.

- Only the characters above.
- Exactly one `@`.
- 1 to 30 crystals `$`.
- At least one warp `*`.
- At most 30 enemies (`a`–`r`, `x`, `y`).
- At most 25 objects (`A N E S W V P !`).
- At most 62 × 62 tiles.

Exported original levels are the best reference for what plays well.

## Level packs

A level pack is a mod in `mods/levelpacks/` with a `[pack]` section and one `[world.N]` section
per world. Its levels are played in order, world by world, and replace the 45 original levels
while the pack is chosen. Packs can have any number of levels. With more than 45, level select
scrolls the row of level lights and shows a scrollbar under it, with a mark where each world
starts.

```ini
[pack]
name = My Levels            ; shown in the Level pack choice (default: the mod's name)
version = 1.0.0             ; stored with the pack's progress (default: the mod's version)

[world.1]
name = Courtyard
look = castle               ; castle, forest or toy
floor = floor_castle        ; optional: an original floor, or an image in textures/
music = Music01             ; optional: Music01-Music04, or a file in music/
levels = first-steps, maze  ; files in levels/ (".txt" may be left out)

[world.2]
name = Crystal Cave
look = toy
floor = cave_floor          ; textures/cave_floor.png
music = cave_theme.mp3      ; music/cave_theme.mp3
levels = cave
```

- `look` picks the world's props (`P` is a pole, tree or short pole), its default floor and
  its default music.
- `floor` is a [floor sheet](#images). It names an original floor, or a `.png` or `.bmp` in
  `textures/` whose name isn't an original image's name.
- `music` names an original track (`Music01`–`Music04`) or a file in `music/`, with its
  extension: `.flac`, `.mp3` or `.mid`.
- Levels in a pack follow the [rules](#rules). A level that breaks them is skipped and reported
  in the log.
- The pack's own textures, models, sounds and music apply only while it's played. Add-ons that
  are on still apply on top of it.
- Each pack keeps its own progress, in `saves/packs/<mod id>/progress.ini`
  ([CONFIG.md](CONFIG.md#saved-progress) explains where saves are).

To choose a pack: **OPTIONS** on the title screen, then **Mods**, then **Level pack**. A pack
whose levels all break the rules is listed as *Can't use*.

## Exporting the original assets

To get every original asset with its mod name, run:

```
HopmonNative.exe --export-assets
```

This writes an `export` folder next to the game. Add a folder name after `--export-assets` to
write somewhere else. The folder contains:

- `textures/` — every image, as PNG;
- `models/` — every model, as the original `.x` and as `.glb` (opens in Blender);
- `sounds/` — every sound effect, as WAV;
- `levels/` — all 45 levels in the level file format.

These files are from **your** copy of the game. Use them to learn from and to line your work up
with, but don't share them (see below).

## Asset names

### Images (`textures/`)

Sizes are the originals'. Replacements may be larger (see [Images](#images)).

| Name | Size | What it is |
|---|---|---|
| `floor_castle` | 256×256 | castle world floor sheet |
| `floor_forest` | 256×256 | forest world floor sheet |
| `floor_toy` | 256×256 | toy world floor sheet |
| `map_background` | 64×64 | tile under the level |
| `shadow` | 32×32 | shadow under characters |
| `player_fire` | 32×32 | Hopmon's fire (its ring) |
| `fire_hit` | 128×128 | sparks: 3 × 3 frames, drawn flickering inside Hopmon's fire and when it hits something |
| `explosion` | 32×32 | enemy explosion |
| `cannon_shot` | 32×32 | cannon ball |
| `ufo_shot` | 32×32 | UFO shot |
| `smoke_purple` | 32×32 | purple smoke |
| `smoke_green` | 32×32 | green smoke |
| `steam` | 32×32 | steam puff |
| `logo_background` | 64×64 | logo screen background tile |
| `saito_logo` | 420×200 | Saito Games logo |
| `title_logo` | 555×100 | HOPMON title |
| `text_copyright` | 312×24 | copyright line |
| `text_unregistered` | 125×14 | "unregistered" note |
| `menu_start` | 83×64 | START |
| `menu_options` | 113×64 | OPTIONS |
| `menu_exit` | 57×64 | EXIT |
| `menu_select_level` | 166×64 | SELECT LEVEL |
| `menu_back` | 73×64 | BACK |
| `menu_arrow_right`, `menu_arrow_left` | 34×56 | menu cursor arrows |
| `level_arrow_right`, `level_arrow_left` | 34×56 | level number arrows and the level row's scroll arrows |
| `level_lights` | 40×72 | level-select lights (frames stacked vertically) |
| `hud_level` | 102×33 | LEVEL |
| `hud_crystals` | 174×33 | CRYSTALS |
| `digits` | 63×300 | large digits 0–9 |
| `hud_fire_ready` | 130×56 | FIRE READY |
| `hud_fire_lights` | 39×44 | fire gauge lights |
| `hud_fps` | 38×22 | FPS |
| `digits_small` | 42×220 | small digits |
| `fade_right`, `fade_left` | 320×480 | screen wipe halves |
| `register_info` | 600×134 | registration screen text |
| `screenshot_1`, `screenshot_2` | 314×237 | registration screen pictures |
| `ending_congratulations` | 555×82 | CONGRATULATIONS |
| `ending_staff` | 126×203 | staff credits |
| `unused_banner` | 236×94 | not used by the game |

### Models (`models/`)

| Name | What it is |
|---|---|
| `hopmon`, `hopmon_2`, `hopmon_3` | Hopmon (animation frames) |
| `crystal` | crystal |
| `ghost` | ghost (`a`) |
| `cube`, `cube_2` | cube (`b`) |
| `jellyfish`, `jellyfish_2` | jellyfish (`c`) |
| `ufo` | UFO (`d`) |
| `sesame`, `sesame_2`, `sesame_3`, `sesame_4` | sesame (`e`) |
| `needle_ball` | needle ball (`m`, `n`) |
| `needle_ball_fast` | fast needle ball (`o`, `p`) |
| `needle_box` | needle box (`q`) |
| `needle_box_drop` | dropping needle box (`r`) |
| `gate` | gate (`x`, `y`) |
| `cannon_up` | upward cannon (`A`) |
| `cannon` | cannon (`N E S W`) |
| `volcano` | volcano (`V`) |
| `pole` | castle world pole (`P`) |
| `tree` | forest world tree (`P`) |
| `short_pole` | toy world pole (`P`) |
| `unused_model` | not used by the game |

### Sounds (`sounds/`)

| Name | When it plays |
|---|---|
| `hop` | Hopmon hops |
| `fire` | Hopmon fires |
| `fire_ready` | fire is ready again |
| `fire_hit` | fire hits an enemy |
| `enemy_explode` | an enemy is destroyed |
| `crystal_pickup` | a crystal is picked up |
| `crystal_delivered` | a crystal is delivered at the warp |
| `level_complete` | level complete |
| `player_hit` | Hopmon is hit |
| `player_vanish` | Hopmon vanishes |
| `cannon` | a cannon fires |
| `cannon_up` | an upward cannon fires |
| `needle_box` | a needle box lands |
| `steam` | a steam vent blows |
| `ufo_fire` | a UFO fires |
| `menu_move` | menu cursor moves |
| `menu_select` | menu choice |
| `logo` | logo screen |

### Levels (`levels/`)

`castle-01` … `castle-15`, `forest-01` … `forest-15`, `toy-01` … `toy-15`: the 45 original
levels in play order.

## What you may share

A mod you share must contain **only your own work**, or work whose licence lets you share it.

- Don't include files from `--export-assets`, from `Hopmon.exe`, or edited versions of them:
  they belong to Saito Games. That includes the original levels, so a level pack holds new
  levels, not copies or edits of original ones.
- Do say your licence in `mod.ini`, so others know what they may do with your work.
- Credit other people's work you use, as its licence asks.

## Troubleshooting

Every start writes `hopmon_native.log` next to the game. Lines starting with `Mods:` list:

- each mod found, as a level pack or an add-on, with its version and author;
- each replacement used (`Mods: model crystal from example-pack (fitted)`);
- anything wrong: a mod that can't be used and why, a level that breaks a rule, a missing floor
  texture or music file, a file that couldn't be read.

| Problem | Check |
|---|---|
| The mod isn't listed | It must be in `mods/levelpacks/` or `mods/addons/`, not directly in `mods/`. `mod.ini` must be at the top of its folder or zip, or inside a single folder in it. |
| A level pack shows up as an add-on | Its `mod.ini` needs a `[pack]` section. |
| *Can't use* | The mod's details page and the log say why: a newer `requires`/`api`, or an id another mod already uses. |
| A replacement isn't used | Is the file name an exact [asset name](#asset-names)? Is an add-on higher in the Add-ons list replacing it too? A level pack's files only apply while the pack is played. |
| A model is huge, tiny or in the wrong place | Build on the exported `.glb`, or add `name = fit` under `[models]`. |
| A level doesn't appear in the pack | The log names the [rule](#rules) it breaks. |
