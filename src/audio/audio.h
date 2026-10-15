// Software mixer replacing DirectSound (secondary buffers) for sound effects,
// plus hosting of the optional software MIDI synthesizer.
#pragma once
#include <cstdint>
#include <cstddef>
#include <functional>

namespace Audio {

bool Init();
void Shutdown();

// A "buffer" is the equivalent of one IDirectSoundBuffer: 8-bit unsigned mono PCM.
int CreateBuffer(const uint8_t* pcm, size_t length, int sampleRate);
void DestroyBuffer(int handle);
void Play(int handle, bool loop);          // restarts from current position (see SetPosition)
void Stop(int handle);
void SetPosition(int handle, size_t pos);   // SetCurrentPosition
void SetVolume(int handle, long hundredthsDb);  // DSBVOLUME_MIN = -10000 .. 0
bool IsPlaying(int handle);

// Music renderer callback (called on the audio thread): fill `frames` stereo float frames.
void SetMusicRenderer(std::function<void(float* out, int frames, int sampleRate)> fn);
// Second music source (MP3/FLAC streams). Renderers add into `out`; both get the music volume.
void SetStreamRenderer(std::function<void(float* out, int frames, int sampleRate)> fn);
int OutputRate();

void SetMasterVolumes(float master, float sfx, float music);

}  // namespace Audio
