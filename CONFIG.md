# hopmon_native.ini reference

`hopmon_native.ini` sits next to `HopmonNative.exe`. The game creates it with default values the first
time it runs. Every setting is also explained in comments inside the file itself.

## How the file works

- **Most people won't need to edit this file.** Press **F10** (or **Start** on a gamepad) in the game
  to open the options menu. Everything in this file except `[game] path` can be changed there,
  takes effect immediately, and is saved here automatically.
- If you edit the file by hand, save it and restart the game. The file is read only at startup.
  Don't edit it while the game is running: the next change in the options menu would overwrite your
  edits.
- Lines starting with `;` are comments. To turn on an example line, delete its `;`.
- **On/off** settings accept `1`/`0`, `true`/`false`, `yes`/`no` or `on`/`off`.
- **Numbers outside the allowed range** are moved to the nearest allowed value. For example,
  `msaa = 99` becomes `16`.
- **Unknown or misspelled settings** are ignored and listed in `hopmon_native.log`. Check the log when a
  change seems to have no effect.
- **To restore every default**, choose **Reset all settings** in the pause menu, or delete the file.
  Both keep your saved progress. The menu option also keeps `[game] path`.
- **Older files are updated automatically.** When a file comes from an earlier build (its
  `config_version` is lower or missing), it is rewritten on the next start with the current
  comments and any new settings, and your values are kept.
- **Command-line overrides.** Any setting can be overridden for one launch without editing the file:
  `HopmonNative.exe --set video.aspect=2 --set audio.music_volume=60`.
  An override only lasts for that launch. The exception: changing anything in the options menu during
  that launch saves the current values, overrides included.

None of these settings change gameplay. The game always runs its original rules at its original
speed of 100 logic steps per second.

---

## [game]

| Setting | Values | Default | What it does |
|---|---|---|---|
| `path` | a file path, or empty | empty | Where your original `Hopmon.exe` is. When empty, the game looks next to `HopmonNative.exe` and in the current folder. Example: `path = D:\Games\Hopmon\Hopmon.exe` |

The music files (`Music01.mid` to `Music04.mid`) are always loaded from the same folder as `Hopmon.exe`.

---

## [video]

### Window and screen

| Setting | Values | Default | What it does |
|---|---|---|---|
| `width` | 320 or more | `1280` | Window width in pixels (window mode only). |
| `height` | 240 or more | `960` | Window height in pixels (window mode only). |
| `fullscreen` | `0`, `1`, `2` | `0` | `0` = window. `1` = borderless: a window without borders covering the whole monitor at its desktop resolution. Alt+Tab switches away instantly and other windows can appear on top (recommended). `2` = exclusive fullscreen at `fullscreen_width` × `fullscreen_height`. Alt+Tab minimizes the game. |
| `fullscreen_width` | `0`, or 320 or more | `0` | Screen resolution width for exclusive fullscreen. `0` = your desktop resolution. |
| `fullscreen_height` | `0`, or 240 or more | `0` | Same, for height. Any other size picks the closest mode your monitor supports. On a widescreen monitor, a 4:3 mode such as 1280×960 is stretched or boxed by the monitor itself, and `aspect = 2` can't widen it. |
| `display` | `0`, `1`, `2`, … | `0` | Which monitor to use. `0` = your primary monitor. `1` = the first monitor, `2` = the second, and so on, in the order Windows lists them. If the number doesn't exist, the primary monitor is used and the log says so. |

You can resize the window by dragging its edges, but that size isn't saved. Alt+Enter or F11 switches
between window and borderless fullscreen while you play. That switch isn't saved either.

### Frame rate and smoothness

| Setting | Values | Default | What it does |
|---|---|---|---|
| `vsync` | on/off | `1` | Matches frames to your monitor's refresh rate, which prevents tearing. Turn it off to draw as fast as possible. |
| `max_fps` | `0`, or any positive number | `0` | Frame-rate cap. `0` = no cap. Common values are `60`, `120` and `144`. Applies only when `interpolation = 1` (see below). |
| `interpolation` | on/off | `1` | The game updates 100 times per second. When on, extra in-between frames are drawn so motion looks fluid on any monitor (60, 144 or 240 Hz). When off, frames are drawn only on the game's own steps, like the original. Purely visual. |

The game never draws more than 100 frames per second when `interpolation = 0`, because that's how
often the game itself updates.

### Image quality

| Setting | Values | Default | What it does |
|---|---|---|---|
| `msaa` | `0`, `2`, `4`, `8`, `16` | `4` | Antialiasing, which smooths jagged polygon edges. `0` = off. If your graphics card supports less, it's lowered automatically, and the log shows the value actually used. |
| `anisotropy` | `1`, `2`, `4`, `8`, `16` | `8` | Keeps floor and wall textures sharp when seen at an angle. `1` = off. |
| `render_scale` | `0.25` to `4.0` | `1.00` | Draws the 3D scene at this multiple of your screen size, then fits it to the screen. `1.0` = native. `1.5`–`2.0` = supersampling: sharper, but heavier on the graphics card. Values below `1.0` are faster but blurrier. |

### Picture shape

| Setting | Values | Default | What it does |
|---|---|---|---|
| `aspect` | `0`, `1`, `2` | `0` | `0` = the original 4:3 picture, with black bars on wide screens. `1` = stretch the 4:3 picture to fill the window, which distorts it. `2` = widescreen: the 3D view is widened to fill the screen, while menus and the on-screen display stay 4:3 in the middle. On a screen narrower than 4:3, `2` behaves like `0`. |
| `hud_layout` | `0`, `1` | `1` | Where the in-game HUD goes in widescreen (`aspect = 2`). `1` = at the screen edges: "LEVEL / CRYSTALS" in the top-left corner, the fire-energy gauge bottom-right and the FPS counter top-right. `0` = the original positions inside the centred 4:3 area. Menus, the title screen and the ending stay centred either way. No effect with `aspect` `0` or `1`. |
| `hud_max_aspect` | `0`, or `1.33` to `10` | `2.40` | With `hud_layout = 1`, how far the HUD may spread on very wide screens, given as an aspect ratio. `2.4` ≈ 21:9, so on a 32:9 monitor the HUD sits at the edges of a 21:9 area in the middle instead of the far corners. `1.78` = 16:9. `0` = always use the full screen width. |
| `hud_scale` | `0.5` to `2.0` | `1.00` | Size of the in-game HUD. `1.0` = original size. Each part grows or shrinks from its own screen corner: level and crystals from the top-left, the fire gauge from the bottom-right, and the FPS counter from the top-right. If the level/crystals line gets wide enough to reach the FPS counter, the counter moves down below it. Works with every `aspect` setting. Also under **Options → Video → HUD size**. |
| `fov_scale` | `0.5` to `2.0` | `1.00` | Camera field of view in levels. `1.0` = original. Higher shows more of the level (zoomed out), lower zooms in. The title screen, level select and ending always use the original view. |

### Visual upgrades

| Setting | Values | Default | What it does |
|---|---|---|---|
| `smooth_textures` | on/off | `0` | Smooth (bilinear) filtering on 3D textures that the original drew pixelated. |
| `smooth_sprites` | on/off | `0` | Smooth scaling of 2D images: the logo, menus and on-screen display. Off keeps them crisp. |
| `per_pixel_lighting` | on/off | `0` | Uses the original lights but calculates them for every pixel instead of every corner, for smoother shading on round objects. |
| `enhanced_shadows` | on/off | `0` | Soft-edged shadows under characters and objects, instead of the original hard-edged ones. |
| `show_fps` | on/off | `0` | Shows the game's own frames-per-second counter in the top-right corner. |

### Example: a typical widescreen setup

```ini
[video]
fullscreen = 1
aspect = 2
hud_layout = 1
msaa = 8
render_scale = 1.5
smooth_textures = 1
per_pixel_lighting = 1
enhanced_shadows = 1
```

---

## [audio]

| Setting | Values | Default | What it does |
|---|---|---|---|
| `master_volume` | `0` to `100` | `50` | Overall volume. Scales both sound effects and music. |
| `sfx_volume` | `0` to `100` | `50` | Sound-effect volume. |
| `music_volume` | `0` to `100` | `50` | Music volume. |
| `soundfont` | a file path, `system`, or empty | empty | A General MIDI SoundFont (`.sf2`) file to play the music with. A relative path is read from the program's folder. When empty, the game first looks for `soundfont.sf2` or `hopmon.sf2` next to the program or the game. If there's none, it uses the Windows built-in MIDI synthesizer, like the original. `system` always uses the Windows synthesizer, even when a SoundFont file is present. |
| `music_source` | `auto`, `midi` | `auto` | `auto`: an MP3 or FLAC file in the `music` folder replaces the matching MIDI track, and tracks without one play their MIDI. `midi`: always the original MIDI music. Also under **Options → Audio → Music source**. |
| `music_1` … `music_4` | a file name, or empty | empty | Use this file (inside the `music` folder; an absolute path also works) for track 1–4 instead of the default name. Example: `music_2 = castle theme.flac` |

### Replacement music (MP3 / FLAC)

- **Folder:** create a folder named `music` next to `Hopmon.exe`. A `music` folder next to
  `HopmonNative.exe` also works if the two programs are in different places. The original
  `Music01.mid` to `Music04.mid` stay where they are.
- **Names:** a file named after a MIDI track replaces it: `Music01.mp3` or `Music01.flac` replaces
  `Music01.mid`, and so on. Names aren't case-sensitive. If a track has both, the FLAC is used.

| Track | Plays on |
|---|---|
| `Music01` | title, level select, ending |
| `Music02` | castle world (levels 1–15) |
| `Music03` | forest world (levels 16–30) |
| `Music04` | toy world (levels 31–45) and the registration screen |

- **Behaviour:** replacement music starts, stops (when Hopmon dies), pauses (when the game window
  loses focus) and loops exactly like the MIDI music, and follows `master_volume` and
  `music_volume`.
- **Loop points:** if a file has `LOOPSTART` with `LOOPLENGTH` or `LOOPEND` tags, values in samples
  (also spelled `LOOP_START`, `LOOP_LENGTH`, `LOOP_END`), it plays the intro once and then loops
  seamlessly between those points. Otherwise it loops from the end back to the start, without a gap.
- **What's supported:** any MP3 (including VBR and ID3v2 tags) and any FLAC (16/24-bit, any sample
  rate, mono or stereo).
- **Problems:** if a file can't be read, the track falls back to its MIDI, and `hopmon_native.log`
  says why. The log also lists which file each track plays.

The Windows built-in synth sounds the same as the original game. A good SoundFont usually sounds much
better, and it's needed for music on Linux and macOS. Free General MIDI SoundFonts such as
*GeneralUser GS* or *FluidR3_GM* work well. `hopmon_native.log` shows which synthesizer is in use.

---

## [input]

| Setting | Values | Default | What it does |
|---|---|---|---|
| `gamepad_deadzone` | `0` to `90` (percent) | `25` | How far an analog stick must move before it counts. Raise it if your character moves on its own. Lower it if the stick feels unresponsive. |

---

## [keys] — keyboard remapping

Format:

```ini
dik_<code> = <key>, <key>, ...
```

`<code>` is the game's original key code from the table below. Listing keys **replaces** the default
key for that action, so include the original key if you want to keep it. You can list as many keys as
you like.

| Code | Original key | Used for |
|---|---|---|
| `0xC8` | Up arrow | move up, menu up; with the camera modifier: tilt camera |
| `0xD0` | Down arrow | move down, menu down; with the camera modifier: tilt camera |
| `0xCB` | Left arrow | move left, menu left; with the camera modifier: turn camera |
| `0xCD` | Right arrow | move right, menu right; with the camera modifier: turn camera |
| `0x2C` | Z | fire, menu confirm |
| `0x2A` | Left Shift | fire, menu confirm |
| `0x36` | Right Shift | fire, menu confirm |
| `0x2D` | X | camera modifier (hold), menu confirm |
| `0x1D` | Left Ctrl | camera modifier (hold), menu confirm |
| `0x9D` | Right Ctrl | camera modifier (hold), menu confirm |
| `0x1C` | Return | menu confirm |
| `0x9C` | Keypad Enter | menu confirm |
| `0x01` | Escape | menu back; in a level: give up the level |
| `0x19` | P | pause |

**Key names** you can use:

- letters and digits: `A`–`Z`, `0`–`9`;
- `Space`, `Return`, `Escape`, `Tab`, `Backspace`;
- `Up`, `Down`, `Left`, `Right`;
- `Left Shift`, `Right Shift`, `Left Ctrl`, `Right Ctrl`, `Left Alt`, `Right Alt`;
- `Keypad 0`–`Keypad 9`, `Keypad Enter`;
- `F1`–`F9`.

Names aren't case-sensitive. An unrecognized name is reported in the log. F10 (options menu), F11 (fullscreen) and F12
(screenshot) are reserved.

Example: WASD movement, with Space and J as extra fire keys.

```ini
[keys]
dik_0xC8 = Up, W
dik_0xD0 = Down, S
dik_0xCB = Left, A
dik_0xCD = Right, D
dik_0x2C = Z, Space, J
```

---

## [gamepad] — controller remapping

Format:

```ini
button_<n> = <button>, <button>, ...
```

The game reads only two controller buttons. Movement is always the left stick and the d-pad.

| Button | Used for | Default |
|---|---|---|
| `button_0` | fire, menu confirm | `a` (bottom face button) |
| `button_1` | camera modifier (hold), menu confirm | `b` (right face button) |

**Button names:**

- face buttons: `a`, `b`, `x`, `y`;
- `back`, `start`, `guide`;
- shoulders: `leftshoulder`, `rightshoulder`;
- stick clicks: `leftstick`, `rightstick`;
- d-pad: `dpup`, `dpdown`, `dpleft`, `dpright`.

Face buttons are named by their **position**, as on an Xbox controller: `a` = bottom, `b` = right,
`x` = left, `y` = top. That's the same physical spot on PlayStation and Switch controllers, whatever
the printed label says. Listing buttons replaces the default.

Example: also fire with the right bumper, and use the left bumper as the camera modifier.

```ini
[gamepad]
button_0 = a, rightshoulder
button_1 = b, leftshoulder
```

---

## [cheats]

Optional game modifiers. These are the only settings that change gameplay, and they're all off by
default. With every cheat off, the game plays exactly like the original. They can also be switched
on and off while you play under **Options → Cheats** in the pause menu. Unlock all levels can only be
changed there on the title and level-select screens; in a level it's greyed out.

| Setting | Values | Default | What it does |
|---|---|---|---|
| `unlock_all_levels` | on/off | `0` | Every level (1–45) can be chosen on the level-select screen. |
| `infinite_fire_energy` | on/off | `0` | The fire gauge is always full, so Hopmon fires on every press instead of waiting about 3 seconds for it to recharge. The game's own limit of 3 shots on screen at once still applies. |
| `no_crystal_slowdown` | on/off | `0` | Carrying crystals no longer slows Hopmon down. Normally each crystal he carries makes every hop 0.03 seconds slower; with this on, every hop takes the normal 0.4 seconds. |

`unlock_all_levels` never changes your real progress, which is the highest level you've reached and
is shared with the original `Hopmon.exe`.

- **Turning it off** brings back exactly the levels you had unlocked before.
- **Clearing a level with the cheat on** saves progress only if that level was already unlocked for
  real. Clearing level 40 when you've only reached level 3 lets you play on, but doesn't unlock
  levels 4–41.
