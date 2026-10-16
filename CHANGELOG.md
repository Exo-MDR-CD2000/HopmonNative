# Changelog

All notable changes to Hopmon Native. Versions follow [semantic versioning](https://semver.org/):
MAJOR for incompatible changes, MINOR for new features, PATCH for fixes.

## [1.3.0] - 2026-10-16

The first version with mods. See [MODDING.md](MODDING.md).

### Added
- **Mods**, in two kinds, each with its own folder next to the game:
  - **Level packs** (`mods/levelpacks/`): new campaigns. You play one at a time, or the original
    game.
  - **Add-ons** (`mods/addons/`): replacements that apply to whatever you play. Several can be on
    at once.

  A mod is a folder or `.zip` file described by a `mod.ini`. Mods only ever add their creators'
  own files; everything they don't replace still comes from your Hopmon.exe.
- **Replacing assets.** Add-ons and level packs can replace the game's images, 3D models, sound
  effects, music and levels by name (`textures/floor_castle.png`, `models/crystal.glb`, `sounds/hop.wav`,
  `levels/castle-07.txt`...).
  - Images can be PNG, BMP, TGA or JPEG, at any resolution: larger images give sharper detail at
    the same size on screen. PNGs with transparency use their own alpha, soft edges included.
  - Models can be glTF 2.0 (`.glb`/`.gltf`, with colours, textures and transparency) or text `.x`.
    `fit` in `mod.ini` scales a model to the original's size.
  - Sounds can be WAV, Ogg Vorbis or FLAC, at any sample rate.
  - Music can be FLAC, MP3 or MIDI.
- **Level packs.** A mod can hold a whole new campaign: levels in text files, grouped into worlds
  that each choose a look (castle, forest or toy), a floor texture and music. Packs can have any
  number of levels. Levels are checked when the pack loads, and any problem is reported in the log.
  A pack's own textures, models, sounds and music apply only while it's played.
- **Options → Mods** (on the title screen):
  - **Level pack** lists the original game and every installed pack, with its number of levels.
    Choosing one asks for confirmation, then reloads at the title screen.
  - **Add-ons** lists the add-ons in load order with tick boxes. Space (gamepad X) ticks one on
    or off, Shift+Up/Down (gamepad LB/RB) moves it up or down the list, and Enter shows its
    details. The add-on higher in the list wins when two replace the same thing. These changes
    apply when the menu closes.

  The choices are saved in a new `[mods]` section of `hopmon_native.ini`.
- Each level pack keeps its own progress, in `saves/packs/<mod id>/progress.ini`.
- `--export-assets [folder]` writes every original image (PNG), model (`.x` and `.glb`), sound
  (WAV) and level (text) from your copy of the game under the names mods use, as a starting point
  for your own work.
- Two example mods (CC0) in `examples/mods`: a three-level pack with its own cave floor and
  crystal model, and an add-on with purple fire, HD level-select lights and a new pickup sound.
- [MODDING.md](MODDING.md): the guide to making mods, with every asset name and the level format.
- `hopmon_native.log` lists the mods found and each replacement they make.
- Faster level choosing for long level packs. On level select, holding Left/Right keeps scrolling,
  at about 8 levels a second and then about 30 a second after 2 seconds, and Up/Down jump 10
  levels (also repeating while held). A single press still moves one level and wraps round at the
  ends as before; held scrolling stops at the first and last level.
- Packs longer than 45 levels show a scrollbar under the row of level lights on level select. The
  bright part shows which part of the pack the row is showing, and small marks show where each
  world starts. The original game, with 45 levels, looks as before.
- Options menu pages with more entries than fit on screen (such as a long list of add-ons or
  level packs) show a scrollbar along the right edge, in place of the small arrows.

### Fixed
- Files in folders with non-English characters in their names (such as a Windows user name with
  accents) are now read correctly.
- CONFIG.md: the `save_path` row of the `[game]` table now displays as part of the table.

## [1.2.0] - 2026-10-15

### Added
- Level select is ready for level packs longer than the original 45 levels: the row of level
  lights shows 45 at a time and scrolls with the selected level, with arrows at the ends when more
  levels are off to either side. Level numbers past 99 are shown in full on level select and in
  the in-level display. With the original game nothing looks different.

### Changed
- Saved progress is no longer stored in the Windows registry. It's now a text file in your user
  folder, `%APPDATA%\HopmonNative\saves\original\progress.ini`, like most PC games. The new
  `save_path` setting in `[game]` moves it anywhere else, including next to the game
  (`save_path = save`). On the first start, the progress you have from the original game is
  copied over once; the original Hopmon.exe keeps its own progress in the registry, untouched.
- Saves are written safely: a new save only replaces the old one once it has been written in
  full, the previous save is kept as `progress.bak`, and a damaged save is restored from it.
- Changing `save_path` copies your saves to the new folder on the next start.
- Reset all settings keeps `save_path`.

## [1.1.0] - 2026-10-15

### Added
- OPTIONS entry on the title screen, between START and EXIT, that opens the options menu. It uses
  the game's own OPTIONS artwork, which is in the original Hopmon.exe but was never shown. The
  title menu moves up slightly to fit the third entry. Opened from the title screen, the menu is
  titled OPTIONS, its first entry is Back, and Main menu is hidden.
- Brightness setting (Options → Graphics → Brightness, or `brightness` in the config file),
  50–150%. It works like a gamma control: dark and middle tones get lighter or darker while black
  and white stay the same.

### Changed
- F10 and gamepad Start no longer open the menu on the title screen and level select; use OPTIONS
  on the title screen there. They still open the pause menu during a level.

### Fixed
- Pause menu text no longer shows thin light lines along the edges of letters when antialiasing
  is on.

## [1.0.0] - 2026-10-14

First public release.

### Game
- The complete game: all 45 levels, the title, level select, registration and ending screens,
  reconstructed from the original program with its original rules, timing (100 logic steps per
  second) and random-number generator.
- Reads all assets from your own copy of the original `Hopmon.exe`, and checks its SHA-256 before
  starting. Saved progress is shared with the original game.

### Display and graphics
- Any resolution, in a window, borderless fullscreen or exclusive fullscreen, on any monitor.
  Borderless switches away instantly with Alt+Tab, and exclusive fullscreen defaults to the desktop
  resolution.
- Widescreen mode that widens the 3D view, with the in-game HUD at the screen edges (capped for
  ultrawide screens).
- Smooth motion at any refresh rate (interpolated frames), VSync and frame limiter.
- Antialiasing, render scale (supersampling), anisotropic filtering, smooth textures and sprites,
  per-pixel lighting and soft shadows.
- HUD size setting (50–200%) and an in-level field-of-view setting.

### Pause / options menu (F10 or gamepad Start)
- Video, Graphics, Audio, Controls and Cheats pages. Every change applies immediately and is saved
  to `hopmon_native.ini`.
- Window and borderless changes apply instantly. Exclusive-fullscreen changes are applied with
  Enter and confirmed, with an automatic revert after 10 seconds.
- Keyboard and gamepad rebinding, stick dead zone.
- Resume, Main menu, Quit game and Reset all settings, each with confirmation where it matters.
- Not available during the intro, the ending, the death animation and the level-complete sequence.

### Audio
- Master, sound-effect and music volume (default 50% each).
- MIDI music through the Windows synth or any General MIDI SoundFont (`.sf2`). The synth can be
  switched while playing.
- Optional MP3/FLAC replacement music from a `music` folder, with seamless looping and
  `LOOPSTART`/`LOOPLENGTH` tag support.

### Cheats (off by default)
- Unlock all levels (doesn't change real saved progress), infinite fire energy, no crystal slowdown.

### Fixes compared with the original
- Sprites crossing the screen edge (the last fire-energy lights, the FPS digits) are clipped
  instead of disappearing.
- Sparkle effects no longer consume the gameplay random-number generator, so enemy behaviour no
  longer depends on the frame rate.
