// Ports of Hopmon's utility library: JList, JString, JStringList, JWinReg,
// JSurface, JSprite, JAscii (JNumber), JSound, JMidi, JDinput.
#pragma once
#include "../core/d3dtypes.h"
#include "../input/input.h"
#include <string>
#include <vector>

class CD3DApplication;
class Surface;
class BackBuffer;
typedef BackBuffer IDirectDrawSurface7;  // the only DirectDraw surface the game blits to

enum JAXIS { JAXIS_X = 0, JAXIS_Y = 1, JAXIS_Z = 2 };

// ---------------------------------------------------------------- JList.cpp
class JBaseList {
public:
    JBaseList() {}
    virtual ~JBaseList() { Empty(); }
    void* GetData(DWORD index) const { return items_[index]; }
    DWORD Count() const { return (DWORD)items_.size(); }
    bool IsEmpty() const { return items_.empty(); }
    void Append(void* data) { items_.push_back(data); }
    void InsertBefore(DWORD index, void* data) { items_.insert(items_.begin() + index, data); }
    void Remove(DWORD index) { items_.erase(items_.begin() + index); }
    void Empty() { items_.clear(); }

private:
    std::vector<void*> items_;
};

// -------------------------------------------------------------- JString.cpp
class JString {
public:
    enum StripType { LEADING = 0, TRAILING = 1, BOTH = 2 };
    JString() {}
    JString(const char* s) : s_(s ? s : "") {}
    JString(const std::string& s) : s_(s) {}
    JString(char c) : s_(1, c) {}
    virtual ~JString() {}
    const char* c_str() const { return s_.c_str(); }
    unsigned Length() const { return (unsigned)strlen(s_.c_str()); }
    int FindChar(char c, unsigned start) const;
    JString ExtractString(unsigned start, unsigned endInclusive) const;
    void ReplaceChar(char from, char to);
    void ToUpper();
    void ToLower();
    void Strip(StripType type, char c);
    JString& operator+=(char c) { s_ += c; return *this; }
    char operator[](unsigned i) const { return i < s_.size() ? s_[i] : 0; }
    const std::string& str() const { return s_; }

private:
    std::string s_;
};

class JStringList {
public:
    DWORD Count() const { return (DWORD)list_.size(); }
    bool IsEmpty() const { return list_.empty(); }
    void Append(const JString& s) { list_.push_back(s); }
    void Remove(DWORD i) { list_.erase(list_.begin() + i); }
    void Empty() { list_.clear(); }
    const JString& GetString(DWORD i) const { return list_[i]; }

private:
    std::vector<JString> list_;
};

int GetStringListFromResource(JStringList& list, int resId);

// --------------------------------------------------------------- JWinReg.cpp
// Persistent settings store. On Windows it uses the same registry key as the
// original game (so progress is shared); elsewhere an ini file next to the port.
class JWinReg {
public:
    JWinReg();
    virtual ~JWinReg();
    int Create(const JString& key);
    int Open(const JString& key);
    int Close();
    int SetDWord(const JString& name, DWORD value);
    int GetDWord(const JString& name, DWORD* value);
    int SetWord(const JString& name, const WORD* value) { return SetDWord(name, *value); }
    int GetWord(const JString& name, WORD* value) {
        DWORD v = 0;
        int ok = GetDWord(name, &v);
        if (ok) *value = (WORD)v;
        return ok;
    }

private:
    void* key_ = nullptr;
    std::string path_;
};

// -------------------------------------------------------------- JSurface.cpp
class JSurface {
public:
    JSurface(CD3DApplication* app, WORD resId, int useColorKey);
    virtual ~JSurface();
    WORD GetResID() const { return resId; }
    Surface* GetDDSurface() const { return lpSurface; }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    int UseColorKey() const { return useColorKey; }
    void SetColorKey(DWORD key);
    int Restore();
    void Invalidate();

protected:
    void _Create();
    CD3DApplication* app;
    WORD resId;
    Surface* lpSurface;
    int width, height;
    int useColorKey;
    DWORD colorKey;
};

class JSurfaceList {
public:
    explicit JSurfaceList(CD3DApplication* app);
    virtual ~JSurfaceList();
    int CreateSurface(WORD resId, int useColorKey);
    void DestroySurface(WORD resId);
    void DestroyAllSurfaces();
    JSurface* GetByResId(WORD resId);
    JSurface* GetByIndex(DWORD index);
    int RestoreAllSurfaces();
    void InvalidateAllSurfaces();

protected:
    int FindIndexOfResId(WORD resId);
    CD3DApplication* app;
    std::vector<JSurface*> list;
};

int FillBufferRect(IDirectDrawSurface7* surface, const RECT* rect, DWORD rgb);

// --------------------------------------------------------------- JSprite.cpp
class JSprite {
public:
    JSprite(JSurfaceList* list, WORD resId, int numFrames);
    virtual ~JSprite() {}
    float GetPosX() const { return posX; }
    float GetPosY() const { return posY; }
    int GetPosIntX() const;
    int GetPosIntY() const;
    int GetCenterIntX() const;
    int GetCenterIntY() const;
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    int GetCurrentFrame() const { return currentFrame; }
    int GetNumFrames() const { return numFrames; }
    void SetPosition(int x, int y) { posX = (float)x; posY = (float)y; }
    void SetPosX(int x) { posX = (float)x; }
    void SetPosY(int y) { posY = (float)y; }
    void ShiftPosX(float d) { posX += d; }
    void ShiftPosY(float d) { posY += d; }
    void SetCenterPos(int x, int y);
    void SetCurrentFrame(int f) { currentFrame = f; }
    void IncrCurrentFrame() { currentFrame = (currentFrame + 1) % numFrames; }
    void SetDrawSize(int w, int h) { drawWidth = w; drawHeight = h; }
    void Draw(IDirectDrawSurface7* dest);
    void DrawScale(IDirectDrawSurface7* dest, float s) { DrawScaleXY(dest, s, s); }
    void DrawScaleX(IDirectDrawSurface7* dest, float s) { DrawScaleXY(dest, s, 1.0f); }
    void DrawScaleY(IDirectDrawSurface7* dest, float s) { DrawScaleXY(dest, 1.0f, s); }
    void DrawScaleXY(IDirectDrawSurface7* dest, float sx, float sy);

protected:
    JSurfaceList* surfaceList;
    WORD resId;
    float posX, posY;
    int width, height;
    int drawWidth, drawHeight;
    int currentFrame;
    int numFrames;
};

// ---------------------------------------------------------------- JAscii.cpp
class JNumber {
public:
    JNumber(JSurfaceList* list, WORD resId, int numFrames);
    virtual ~JNumber();
    void SetPosition(int x, int y) { posX = x; posY = y; }
    void SetDisplayZero(int z) { displayZero = z; }
    void SetNumDigits(int n) { numDigits = n; }
    void SetNumber(int n) { number = n; }
    void Draw(IDirectDrawSurface7* dest);
    int GetPosX() const { return posX; }
    int GetPosY() const { return posY; }
    int GetDigitWidth() const;
    int GetWidth() const { return GetDigitWidth() * numDigits; }
    int GetHeight() const;

protected:
    JSprite* numberSprite;
    int posX, posY;
    int displayZero;
    int numDigits;
    int number;
};

// ---------------------------------------------------------------- JSound.cpp
class JSound {
public:
    JSound(WORD resId, DWORD count);
    virtual ~JSound();
    WORD GetResId() const { return resId; }
    void SetVolume(BYTE v);
    void Play(int loop);
    void Stop();

protected:
    void CreateWaveData(WORD resId);
    WORD resId;
    DWORD count;
    std::vector<uint8_t> waveData;
    DWORD playIndex;
    BYTE volume;
    std::vector<int> bufferList;  // mixer buffer handles
};

class JSoundList {
public:
    JSoundList();
    virtual ~JSoundList();
    int Create();
    void CreateSound(WORD resId, DWORD count);
    void Play(WORD resId, BYTE volume, int loop);
    void Stop(WORD resId);
    void StopAll();

protected:
    int GetNullIndex();
    int FindIndexOfResId(WORD resId);
    bool created_ = false;
    JSound* soundArray[30];
};

// ----------------------------------------------------------------- JMidi.cpp
class JMidiList {
public:
    JMidiList();
    virtual ~JMidiList();
    void Append(const char* file);
    void Play(int index);
    void Replay();
    void Pause();
    void Resume();
    void Stop();
    void Reload();  // native: restart the current track after the music source setting changed
    const char* CurrentSource() const;  // native: "MP3/FLAC file", "MIDI" or "" (nothing playing)

protected:
    int paused;
    int currentMidiIndex;
    bool streaming = false;  // native: current track is an MP3/FLAC replacement
    JStringList* midiFileList;
};

// --------------------------------------------------------------- JDinput.cpp
struct JINPUTSTATE {
    BYTE keyState[256];
    DIJOYSTATE joyState[4];
};

class JDInput {
public:
    JDInput();
    virtual ~JDInput();
    int Create();
    void Destroy();
    void UpdateInputState();
    void SetAcquireKeyboard(int a) { acquireKeyboard = a; }
    int GetKeyboardState(int key) const { return inputState1.keyState[key] & 0x80; }
    int GetKeyboardPressed(int key) const;
    int GetKeyboardReleased(int key) const;
    void GetKeyboardBuffer(JString& out);
    void ClearKeyboardBuffer();
    void SetAcquireJoystick(int a) { acquireJoystick = a; }
    int GetJoyButtonState(int joy, int button) const;
    int GetJoyButtonPressed(int joy, int button) const;
    int GetJoyButtonReleased(int joy, int button) const;
    int GetJoyDirState(int joy, int dir) const;
    int GetJoyDirPressed(int joy, int dir) const;
    int GetJoyDirReleased(int joy, int dir) const;

protected:
    int _GetJoyDirStatePrev(int joy, int dir) const;
    static int DirFromState(const DIJOYSTATE& s, int dir);
    bool joystickPresent[4];
    JINPUTSTATE inputState1;  // current
    JINPUTSTATE inputState2;  // previous
    int acquireKeyboard;
    int acquireJoystick;
};

char ConvertKeyCodeToAscii(DWORD dik);
