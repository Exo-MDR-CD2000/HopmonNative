// Standard MIDI File sequencer replacing the MCI "sequencer" device used by the original.
// Output goes either to the operating system MIDI synth (Windows: same synth the
// original used) or to a built-in SoundFont synthesizer.
#pragma once
#include <string>

namespace Midi {

// Uses the SoundFont if one is given or found next to the program/game; otherwise the
// Windows system synthesizer (Windows only).
bool Init(const std::string& soundFontPath);
void Shutdown();
// Switch synthesizer while running (options menu); the current song keeps playing.
// soundFontPath: "" = auto (soundfont.sf2/hopmon.sf2, else system), "system" = OS synth, else a .sf2 path.
bool SwitchSynth(const std::string& soundFontPath);

// MCI-equivalent commands
bool Open(const std::string& path);   // "close all" + "open <path> type sequencer"
void PlayFromStart();                 // "play MUSIC from 0 notify"
void Pause();                         // "stop MUSIC"
void Resume();                        // "play MUSIC notify"
void Close();                         // "close all"

// Returns true once each time playback reached the end (MM_MCINOTIFY / MCI_NOTIFY_SUCCESSFUL)
bool PollFinished();

void SetVolume(float v);  // 0..1
const char* BackendName();

}  // namespace Midi
