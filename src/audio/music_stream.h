// Native addition: MP3 / FLAC music played in place of the original MIDI tracks.
// A file is loaded into memory on Open and decoded a little at a time on the audio thread,
// resampled to the output rate and mixed through the music channel (so the master and music
// volume settings apply). It loops seamlessly: from the end back to the start, or between
// LOOPSTART / LOOPLENGTH / LOOPEND tags when the file has them.
#pragma once
#include <string>

namespace MusicStream {

bool Init();      // registers the renderer with the mixer
void Shutdown();

// Loads and checks a file. On failure returns false, logs why, and leaves nothing open.
bool Open(const std::string& path);
void PlayFromStart();
void Pause();
void Resume();
void Close();
bool IsOpen();

// Short description of the open file for the log ("FLAC, 44100 Hz, stereo, 3:12, loop 0:05.20-3:10.00")
std::string Describe();

// MP3 / FLAC replacement for one of the game's MIDI tracks, or "" if there is none.
// index: 0-based track; midiName: the original file name (e.g. "Music02.mid").
std::string FindReplacement(int index, const std::string& midiName);

}  // namespace MusicStream
