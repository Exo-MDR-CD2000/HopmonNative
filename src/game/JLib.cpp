#include "JLib.h"
#include "../core/assets.h"
#include "../core/log.h"
#include "../audio/audio.h"
#include "../audio/midi.h"
#include "../audio/music_stream.h"
#include "../framework/d3dapp.h"
#include "../framework/d3dtextr.h"
#include "../framework/d3dutil.h"
#include "../gfx/device.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// ============================================================== JString
int JString::FindChar(char c, unsigned start) const {
    size_t len = strlen(s_.c_str());
    if (start >= len) return -1;
    const char* p = strchr(s_.c_str() + start, c);
    return p ? (int)(p - s_.c_str()) : -1;
}

JString JString::ExtractString(unsigned start, unsigned endInclusive) const {
    std::string r;
    size_t len = strlen(s_.c_str());
    for (size_t pos = start; pos < len && pos <= endInclusive; pos++) r += s_[pos];
    return JString(r);
}

void JString::ReplaceChar(char from, char to) {
    // strchr-based like the original: stops at the first NUL
    for (char* p = strchr(&s_[0], from); p; p = strchr(&s_[0], from)) {
        *p = to;
        if (from == '\0') break;
    }
}

void JString::ToUpper() {
    for (auto& c : s_) c = (char)toupper((unsigned char)c);
}

void JString::ToLower() {
    for (auto& c : s_) c = (char)tolower((unsigned char)c);
}

void JString::Strip(StripType type, char c) {
    size_t len = strlen(s_.c_str());
    if (len && (type == LEADING || type == BOTH)) {
        size_t p = 0;
        while (p < len && s_[p] == c) p++;
        if (p >= len) s_.clear();
        else if (p) s_ = ExtractString((unsigned)p, (unsigned)len).str();
    }
    len = strlen(s_.c_str());
    if (len && (type == TRAILING || type == BOTH)) {
        int p = (int)len;
        do {
            p--;
        } while (p >= 0 && s_[p] == c);
        if (p < 0) s_.clear();
        else if (p < (int)len - 1) s_ = ExtractString(0, (unsigned)p).str();
    }
}

int GetStringListFromResource(JStringList& list, int resId) {
    if (!Assets::RCData((uint32_t)(resId & 0xFFFF))) return 0;
    list.Empty();
    JString all(Assets::RCDataCString((uint32_t)(resId & 0xFFFF)));
    unsigned pos = 0;
    while (pos < all.Length()) {
        int nl = all.FindChar('\n', pos);
        if (nl < 0) {
            list.Append(all.ExtractString(pos, all.Length()));
            break;
        }
        JString line = all.ExtractString(pos, (unsigned)nl);
        line.ReplaceChar('\r', '\n');
        line.Strip(JString::TRAILING, '\n');
        list.Append(line);
        pos = (unsigned)nl + 1;
    }
    return 1;
}

// ============================================================== JSurface
JSurface::JSurface(CD3DApplication* a, WORD id, int key)
    : app(a), resId(id), lpSurface(nullptr), width(-1), height(-1), useColorKey(key), colorKey(0xFFFFFFFF) {
    _Create();
}

JSurface::~JSurface() {
    delete lpSurface;
    lpSurface = nullptr;
}

void JSurface::SetColorKey(DWORD key) {
    colorKey = key;
    if (useColorKey) DDSetColorKey(lpSurface, colorKey);
}

int JSurface::Restore() {
    delete lpSurface;
    lpSurface = nullptr;
    _Create();
    return lpSurface != nullptr;
}

void JSurface::Invalidate() {
    delete lpSurface;
    lpSurface = nullptr;
}

void JSurface::_Create() {
    lpSurface = DDLoadBitmap(resId, &width, &height);
    if (useColorKey) DDSetColorKey(lpSurface, colorKey);
}

JSurfaceList::JSurfaceList(CD3DApplication* a) : app(a) {}
JSurfaceList::~JSurfaceList() { DestroyAllSurfaces(); }

int JSurfaceList::CreateSurface(WORD id, int key) {
    FindIndexOfResId(id);  // (result unused in the original as well)
    JSurface* s = new JSurface(app, id, key);
    if (s->GetDDSurface()) {
        list.push_back(s);
        return 1;
    }
    Log("Bitmap resource %u could not be loaded", id);
    delete s;  // the original leaked it
    return 0;
}

void JSurfaceList::DestroySurface(WORD id) {
    int i = FindIndexOfResId(id);
    if (i >= 0) {
        delete list[i];
        list.erase(list.begin() + i);
    }
}

void JSurfaceList::DestroyAllSurfaces() {
    for (auto* s : list) delete s;
    list.clear();
}

JSurface* JSurfaceList::GetByResId(WORD id) {
    for (auto* s : list)
        if (s->GetResID() == id) return s;
    return nullptr;
}

JSurface* JSurfaceList::GetByIndex(DWORD i) { return i < list.size() ? list[i] : nullptr; }

int JSurfaceList::RestoreAllSurfaces() {
    for (auto* s : list)
        if (!s->Restore()) return 0;
    return 1;
}

void JSurfaceList::InvalidateAllSurfaces() {
    for (auto* s : list) s->Invalidate();
}

int JSurfaceList::FindIndexOfResId(WORD id) {
    for (size_t i = 0; i < list.size(); i++)
        if (list[i]->GetResID() == id) return (int)i;
    return -1;
}

int FillBufferRect(IDirectDrawSurface7* surface, const RECT* rect, DWORD rgb) {
    if (!surface) return 0;
    // DDColorMatch maps the COLORREF (0x00BBGGRR) to the surface format
    DWORD r = rgb & 0xFF, g = (rgb >> 8) & 0xFF, b = (rgb >> 16) & 0xFF;
    return surface->ColorFill(rect, r << 16 | g << 8 | b) == S_OK;
}

// ============================================================== JSprite
JSprite::JSprite(JSurfaceList* list, WORD id, int frames)
    : surfaceList(list), resId(id), posX(0.f), posY(0.f), width(-1), height(-1), drawWidth(-1), drawHeight(-1),
      currentFrame(0), numFrames(frames) {
    JSurface* s = surfaceList->GetByResId(resId);
    if (!s) {
        Log("JSprite: surface %u not loaded", id);
        return;
    }
    width = s->GetWidth();
    height = s->GetHeight() / numFrames;
}

int JSprite::GetPosIntX() const { return (int)Round(GetPosX()); }
int JSprite::GetPosIntY() const { return (int)Round(GetPosY()); }
int JSprite::GetCenterIntX() const { return width / 2 + GetPosIntX(); }
int JSprite::GetCenterIntY() const { return height / 2 + GetPosIntY(); }

void JSprite::SetCenterPos(int x, int y) {
    posX = (float)(x - width / 2);
    posY = (float)(y - height / 2);
}

void JSprite::Draw(IDirectDrawSurface7* dest) {
    if (!dest) return;
    JSurface* s = surfaceList->GetByResId(resId);
    if (!s) return;
    int x = GetPosIntX(), y = GetPosIntY();
    int w = drawWidth < 0 ? width : drawWidth;
    int h = drawHeight < 0 ? height : drawHeight;
    RECT src{0, currentFrame * height, width, currentFrame * height + height};
    RECT dst{x, y, x + w, y + h};
    dest->Blt(&dst, s->GetDDSurface(), &src, s->UseColorKey() != 0);
}

void JSprite::DrawScaleXY(IDirectDrawSurface7* dest, float sx, float sy) {
    if (!dest) return;
    JSurface* s = surfaceList->GetByResId(resId);
    if (!s) return;
    long w = Round((float)width * sx);
    int x = GetCenterIntX() - w / 2;
    long h = Round((float)height * sy);
    int y = GetCenterIntY() - h / 2;
    RECT src{0, currentFrame * height, width, currentFrame * height + height};
    RECT dst{x, y, (int)(x + w), (int)(y + h)};
    dest->Blt(&dst, s->GetDDSurface(), &src, s->UseColorKey() != 0);
}

// ============================================================== JNumber
JNumber::JNumber(JSurfaceList* list, WORD id, int frames)
    : numberSprite(new JSprite(list, id, frames)), posX(0), posY(0), displayZero(1), numDigits(0), number(0) {}

JNumber::~JNumber() {
    delete numberSprite;
    numberSprite = nullptr;
}

void JNumber::Draw(IDirectDrawSurface7* dest) {
    char buffer[34];
    snprintf(buffer, sizeof buffer, "%ld", (long)number);
    int len = (int)strlen(buffer);
    int nx = posX;
    if (numDigits > 0 && len < numDigits) {
        int pad = numDigits - len;
        for (int i = 0; i < pad; i++) {
            numberSprite->SetCurrentFrame(0);
            numberSprite->SetPosition(nx, posY);
            if (displayZero) numberSprite->Draw(dest);
            nx += GetDigitWidth();
        }
    }
    for (int i = 0; i < len; i++) {
        if (showAllDigits || len - i <= numDigits) {
            numberSprite->SetCurrentFrame(buffer[i] - '0');
            numberSprite->SetPosition(nx, posY);
            numberSprite->Draw(dest);
            nx += GetDigitWidth();
        }
    }
}

int JNumber::GetDigitWidth() const { return numberSprite->GetWidth() / 3; }

int JNumber::DigitCount() const {
    char buffer[34];
    int len = snprintf(buffer, sizeof buffer, "%ld", (long)number);
    return (showAllDigits && len > numDigits) ? len : numDigits;
}
int JNumber::GetHeight() const { return numberSprite->GetHeight(); }

// ============================================================== JSound
JSound::JSound(WORD id, DWORD c) : resId(id), count(c), playIndex(0), volume(0) { CreateWaveData(resId); }

JSound::~JSound() {
    for (int h : bufferList) Audio::DestroyBuffer(h);
    bufferList.clear();
}

void JSound::SetVolume(BYTE v) { volume = v < 101 ? v : 100; }

void JSound::Play(int loop) {
    if (volume == 0) return;
    if (playIndex < bufferList.size()) {
        int h = bufferList[playIndex];
        Audio::Stop(h);
        Audio::SetPosition(h, 0);
        Audio::SetVolume(h, (long)(((float)volume / 100.0f) * 10000.0f) - 10000);
        Audio::Play(h, loop == 1);
    }
    playIndex = (playIndex + 1) % count;
}

void JSound::Stop() {
    for (int h : bufferList) {
        Audio::Stop(h);
        Audio::SetPosition(h, 0);
    }
}

void JSound::CreateWaveData(WORD id) {
    const auto* res = Assets::Wave(id);
    if (!res || res->size() < 12) {
        Log("Sound resource %u missing", id);
        return;
    }
    const uint8_t* base = res->data();
    uint32_t riffSize = base[4] | base[5] << 8 | base[6] << 16 | (uint32_t)base[7] << 24;
    const uint8_t* end = base + std::min<size_t>(res->size(), (size_t)riffSize + 8);
    const uint8_t* p = base + 12;
    const uint8_t* data = nullptr;
    uint32_t dataSize = 0;
    bool haveFmt = false;
    while (p + 8 <= end && !(haveFmt && data)) {
        uint32_t sz = p[4] | p[5] << 8 | p[6] << 16 | (uint32_t)p[7] << 24;
        if (!memcmp(p, "fmt ", 4)) haveFmt = true;
        else if (!memcmp(p, "data", 4) && !data) {
            data = p + 8;
            dataSize = sz;
        }
        p += 8 + ((sz + 1) & ~1u);
    }
    if (!data) return;
    dataSize = (uint32_t)std::min<size_t>(dataSize, (size_t)(res->data() + res->size() - data));
    waveData.assign(data, data + dataSize);
    // The original always creates 22050 Hz 8-bit mono buffers regardless of the file header.
    for (DWORD i = 0; i < count; i++) bufferList.push_back(Audio::CreateBuffer(waveData.data(), waveData.size(), 22050));
}

JSoundList::JSoundList() {
    for (auto& s : soundArray) s = nullptr;
}

JSoundList::~JSoundList() {
    StopAll();
    for (auto& s : soundArray) {
        delete s;
        s = nullptr;
    }
}

int JSoundList::Create() {
    created_ = true;
    return 1;
}

void JSoundList::CreateSound(WORD id, DWORD c) {
    if (!created_) return;
    int i = GetNullIndex();
    if (i >= 0) soundArray[i] = new JSound(id, c);
}

void JSoundList::Play(WORD id, BYTE vol, int loop) {
    if (!created_) return;
    int i = FindIndexOfResId(id);
    if (i >= 0) {
        soundArray[i]->SetVolume(vol);
        soundArray[i]->Play(loop);
    }
}

void JSoundList::Stop(WORD id) {
    if (!created_) return;
    int i = FindIndexOfResId(id);
    if (i >= 0) soundArray[i]->Stop();
}

void JSoundList::StopAll() {
    for (auto* s : soundArray)
        if (s) s->Stop();
}

int JSoundList::GetNullIndex() {
    for (int i = 0; i < 30; i++)
        if (!soundArray[i]) return i;
    return -1;
}

int JSoundList::FindIndexOfResId(WORD id) {
    for (int i = 0; i < 30; i++)
        if (soundArray[i] && soundArray[i]->GetResId() == id) return i;
    return -1;
}

// ============================================================== JMidiList
JMidiList::JMidiList() : paused(0), currentMidiIndex(-1), midiFileList(new JStringList()) {}
JMidiList::~JMidiList() {
    delete midiFileList;
    midiFileList = nullptr;
}

void JMidiList::Append(const char* file) { midiFileList->Append(JString(file)); }

// Native addition: each track can be replaced by an MP3/FLAC file in the "music" folder
// (MusicStream::FindReplacement). The game's calls - play, replay at the end, pause, resume,
// stop - are routed to whichever player has the current track, so both behave identically.
// MP3/FLAC tracks loop inside the stream (seamlessly), so they never ask for a Replay.
void JMidiList::Play(int index) {
    if (index < 0 || index >= (int)midiFileList->Count() || currentMidiIndex == index) return;
    Stop();
    std::string name = midiFileList->GetString(index).str();
    std::string file = MusicStream::FindReplacement(index, name);
    if (!file.empty() && MusicStream::Open(file)) {
        streaming = true;
        Log("Music: track %d plays %s (%s)", index + 1, file.c_str(), MusicStream::Describe().c_str());
        MusicStream::PlayFromStart();
        currentMidiIndex = index;
        if (paused) MusicStream::Pause();
        return;
    }
    if (!file.empty()) Log("Music: track %d falls back to %s", index + 1, name.c_str());
    std::string path = Assets::ResolveGameFile(name);
    if (Midi::Open(path)) Midi::PlayFromStart();
    currentMidiIndex = index;
    if (paused) Midi::Pause();
}

void JMidiList::Replay() {
    if (currentMidiIndex < 0) return;
    if (streaming) {
        MusicStream::PlayFromStart();
        if (paused) MusicStream::Pause();
        return;
    }
    Midi::PlayFromStart();
    if (paused) Midi::Pause();
}

void JMidiList::Pause() {
    paused = 1;
    if (currentMidiIndex < 0) return;
    if (streaming) MusicStream::Pause();
    else Midi::Pause();
}

void JMidiList::Resume() {
    paused = 0;
    if (currentMidiIndex < 0) return;
    if (streaming) MusicStream::Resume();
    else Midi::Resume();
}

void JMidiList::Stop() {
    currentMidiIndex = -1;
    streaming = false;
    MusicStream::Close();
    Midi::Close();
}

void JMidiList::Reload() {
    int index = currentMidiIndex;
    if (index < 0) return;
    Stop();
    Play(index);
}

const char* JMidiList::CurrentSource() const {
    if (currentMidiIndex < 0) return "";
    return streaming ? "MP3/FLAC file" : "MIDI";
}

// ============================================================== JDInput
JDInput::JDInput() : acquireKeyboard(1), acquireJoystick(1) {
    memset(&inputState1, 0, sizeof inputState1);
    memset(&inputState2, 0, sizeof inputState2);
    for (auto& j : joystickPresent) j = false;
}

JDInput::~JDInput() { Destroy(); }

int JDInput::Create() { return 1; }
void JDInput::Destroy() {}

void JDInput::UpdateInputState() {
    inputState2 = inputState1;
    memset(&inputState1, 0, sizeof inputState1);
    if (acquireKeyboard) Input::GetKeyboardState(inputState1.keyState);
    for (int i = 0; i < 4; i++) {
        joystickPresent[i] = Input::NumJoysticks() > i;
        if (joystickPresent[i] && acquireJoystick) Input::GetJoystickState(i, &inputState1.joyState[i]);
    }
}

int JDInput::GetKeyboardPressed(int key) const {
    return !(inputState2.keyState[key] & 0x80) && (inputState1.keyState[key] & 0x80);
}

int JDInput::GetKeyboardReleased(int key) const {
    return (inputState2.keyState[key] & 0x80) && !(inputState1.keyState[key] & 0x80);
}

void JDInput::GetKeyboardBuffer(JString& out) {
    std::vector<int> keys;
    Input::GetBufferedKeys(&keys);
    out = JString("");
    for (int k : keys) {
        char c = ConvertKeyCodeToAscii((DWORD)k);
        if (c) {
            out += c;
            if (out.Length() > 15) break;
        }
    }
}

void JDInput::ClearKeyboardBuffer() {
    JString dummy;
    GetKeyboardBuffer(dummy);
}

int JDInput::GetJoyButtonState(int joy, int b) const {
    if (!joystickPresent[joy]) return 0;
    return inputState1.joyState[joy].rgbButtons[b] & 0x80;
}

int JDInput::GetJoyButtonPressed(int joy, int b) const {
    if (!joystickPresent[joy]) return 0;
    return !(inputState2.joyState[joy].rgbButtons[b] & 0x80) && (inputState1.joyState[joy].rgbButtons[b] & 0x80);
}

int JDInput::GetJoyButtonReleased(int joy, int b) const {
    if (!joystickPresent[joy]) return 0;
    return (inputState2.joyState[joy].rgbButtons[b] & 0x80) && !(inputState1.joyState[joy].rgbButtons[b] & 0x80);
}

int JDInput::DirFromState(const DIJOYSTATE& s, int dir) {
    switch (dir) {
        case 0: return s.lY < -800;   // up
        case 1: return 800 < s.lX;    // right
        case 2: return 800 < s.lY;    // down
        case 3: return s.lX < -800;   // left
    }
    return 0;
}

int JDInput::GetJoyDirState(int joy, int dir) const {
    if (!joystickPresent[joy]) return 0;
    return DirFromState(inputState1.joyState[joy], dir);
}

int JDInput::_GetJoyDirStatePrev(int joy, int dir) const {
    if (!joystickPresent[joy]) return 0;
    return DirFromState(inputState2.joyState[joy], dir);
}

int JDInput::GetJoyDirPressed(int joy, int dir) const {
    if (!joystickPresent[joy]) return 0;
    return !_GetJoyDirStatePrev(joy, dir) && GetJoyDirState(joy, dir);
}

int JDInput::GetJoyDirReleased(int joy, int dir) const {
    if (!joystickPresent[joy]) return 0;
    return _GetJoyDirStatePrev(joy, dir) && !GetJoyDirState(joy, dir);
}

char ConvertKeyCodeToAscii(DWORD k) {
    switch (k) {
        case 0x02: case 0x4F: return '1';
        case 0x03: case 0x50: return '2';
        case 0x04: case 0x51: return '3';
        case 0x05: case 0x4B: return '4';
        case 0x06: case 0x4C: return '5';
        case 0x07: case 0x4D: return '6';
        case 0x08: case 0x47: return '7';
        case 0x09: case 0x48: return '8';
        case 0x0A: case 0x49: return '9';
        case 0x0B: case 0x52: return '0';
        case 0x0C: case 0x4A: return '-';
        case 0x0E: case 0xCB: return '\b';
        case 0x10: return 'Q'; case 0x11: return 'W'; case 0x12: return 'E'; case 0x13: return 'R';
        case 0x14: return 'T'; case 0x15: return 'Y'; case 0x16: return 'U'; case 0x17: return 'I';
        case 0x18: return 'O'; case 0x19: return 'P';
        case 0x1C: case 0x9C: return '\n';
        case 0x1E: return 'A'; case 0x1F: return 'S'; case 0x20: return 'D'; case 0x21: return 'F';
        case 0x22: return 'G'; case 0x23: return 'H'; case 0x24: return 'J'; case 0x25: return 'K';
        case 0x26: return 'L';
        case 0x2C: return 'Z'; case 0x2D: return 'X'; case 0x2E: return 'C'; case 0x2F: return 'V';
        case 0x30: return 'B'; case 0x31: return 'N'; case 0x32: return 'M';
        case 0x34: case 0x53: return '.';
        case 0x39: return ' ';
    }
    return '\0';
}
