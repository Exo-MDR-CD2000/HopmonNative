# Changelog

All notable changes to Hopmon Native. Versions follow [semantic versioning](https://semver.org/):
MAJOR for incompatible changes, MINOR for new features, PATCH for fixes.

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
