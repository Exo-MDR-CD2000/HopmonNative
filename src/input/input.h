// SDL3 backed replacement for DirectInput 7: keyboard state indexed by DIK_*
// scancodes and up to four joysticks reported as DIJOYSTATE (axes -1000..1000).
#pragma once
#include <cstdint>
#include <vector>

union SDL_Event;

struct DIJOYSTATE {
    int32_t lX, lY, lZ, lRx, lRy, lRz;
    int32_t rglSlider[2];
    uint32_t rgdwPOV[4];
    uint8_t rgbButtons[32];
};

namespace Input {

bool Init();
void Shutdown();
void HandleEvent(const SDL_Event& e);
void SetFocus(bool focused);

// Snapshot of the current state (what IDirectInputDevice::GetDeviceState returns)
void GetKeyboardState(uint8_t out[256]);
int NumJoysticks();
bool GetJoystickState(int index, DIJOYSTATE* out);

// Buffered key presses (DIK codes) since the last call, like GetDeviceData
void GetBufferedKeys(std::vector<int>* out);

int DikFromScancode(int sdlScancode);
// Original key codes a physical key produces, honouring [keys] remapping.
void DiksForScancode(int sdlScancode, std::vector<int>* out);
void InjectKey(int dik, bool down);   // automation / virtual input

// Re-read [keys]/[gamepad] remapping from the config (options menu).
void ReloadBindings();
// While blocked (options menu open) the game sees no keys, buttons or sticks. After unblocking,
// input stays blocked until everything has been released, so menu keys don't leak into the game.
void SetBlocked(bool blocked);
// Physical keys (SDL scancodes) that produce a DIK code when it is not remapped.
void DefaultScancodesForDik(int dik, std::vector<int>* out);

}  // namespace Input

// DirectInput key codes used by the game
enum {
    DIK_ESCAPE = 0x01, DIK_1 = 0x02, DIK_0 = 0x0B, DIK_MINUS = 0x0C, DIK_BACK = 0x0E, DIK_TAB = 0x0F,
    DIK_Q = 0x10, DIK_W = 0x11, DIK_E = 0x12, DIK_R = 0x13, DIK_T = 0x14, DIK_Y = 0x15, DIK_U = 0x16,
    DIK_I = 0x17, DIK_O = 0x18, DIK_P = 0x19, DIK_RETURN = 0x1C, DIK_LCONTROL = 0x1D, DIK_A = 0x1E,
    DIK_S = 0x1F, DIK_D = 0x20, DIK_F = 0x21, DIK_G = 0x22, DIK_H = 0x23, DIK_J = 0x24, DIK_K = 0x25,
    DIK_L = 0x26, DIK_LSHIFT = 0x2A, DIK_Z = 0x2C, DIK_X = 0x2D, DIK_C = 0x2E, DIK_V = 0x2F, DIK_B = 0x30,
    DIK_N = 0x31, DIK_M = 0x32, DIK_PERIOD = 0x34, DIK_RSHIFT = 0x36, DIK_LMENU = 0x38, DIK_SPACE = 0x39,
    DIK_F1 = 0x3B, DIK_F2 = 0x3C, DIK_F3 = 0x3D, DIK_F4 = 0x3E, DIK_F5 = 0x3F, DIK_F10 = 0x44,
    DIK_NUMPAD7 = 0x47, DIK_NUMPAD8 = 0x48, DIK_NUMPAD9 = 0x49, DIK_SUBTRACT = 0x4A, DIK_NUMPAD4 = 0x4B,
    DIK_NUMPAD5 = 0x4C, DIK_NUMPAD6 = 0x4D, DIK_NUMPAD1 = 0x4F, DIK_NUMPAD2 = 0x50, DIK_NUMPAD3 = 0x51,
    DIK_NUMPAD0 = 0x52, DIK_DECIMAL = 0x53, DIK_NUMPADENTER = 0x9C, DIK_RCONTROL = 0x9D, DIK_RMENU = 0xB8,
    DIK_PAUSE = 0xC5, DIK_HOME = 0xC7, DIK_UP = 0xC8, DIK_PRIOR = 0xC9, DIK_LEFT = 0xCB, DIK_RIGHT = 0xCD,
    DIK_END = 0xCF, DIK_DOWN = 0xD0, DIK_NEXT = 0xD1, DIK_INSERT = 0xD2, DIK_DELETE = 0xD3
};
