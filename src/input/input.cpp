#include "input.h"
#include "../core/config.h"
#include "../core/log.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

namespace {

// SDL scancode -> DirectInput (set 1) scancode
uint8_t g_sdlToDik[SDL_SCANCODE_COUNT];

void BuildTable() {
    memset(g_sdlToDik, 0, sizeof g_sdlToDik);
    auto m = [](SDL_Scancode s, uint8_t d) { g_sdlToDik[s] = d; };
    m(SDL_SCANCODE_ESCAPE, 0x01);
    const SDL_Scancode digits[] = {SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_5,
                                   SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9, SDL_SCANCODE_0};
    for (int i = 0; i < 10; i++) m(digits[i], (uint8_t)(0x02 + i));
    m(SDL_SCANCODE_MINUS, 0x0C); m(SDL_SCANCODE_EQUALS, 0x0D); m(SDL_SCANCODE_BACKSPACE, 0x0E); m(SDL_SCANCODE_TAB, 0x0F);
    const char* row1 = "QWERTYUIOP";
    const SDL_Scancode r1[] = {SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R, SDL_SCANCODE_T,
                               SDL_SCANCODE_Y, SDL_SCANCODE_U, SDL_SCANCODE_I, SDL_SCANCODE_O, SDL_SCANCODE_P};
    for (int i = 0; row1[i]; i++) m(r1[i], (uint8_t)(0x10 + i));
    m(SDL_SCANCODE_LEFTBRACKET, 0x1A); m(SDL_SCANCODE_RIGHTBRACKET, 0x1B); m(SDL_SCANCODE_RETURN, 0x1C);
    m(SDL_SCANCODE_LCTRL, 0x1D);
    const SDL_Scancode r2[] = {SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F, SDL_SCANCODE_G,
                               SDL_SCANCODE_H, SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L};
    for (int i = 0; i < 9; i++) m(r2[i], (uint8_t)(0x1E + i));
    m(SDL_SCANCODE_SEMICOLON, 0x27); m(SDL_SCANCODE_APOSTROPHE, 0x28); m(SDL_SCANCODE_GRAVE, 0x29);
    m(SDL_SCANCODE_LSHIFT, 0x2A); m(SDL_SCANCODE_BACKSLASH, 0x2B);
    const SDL_Scancode r3[] = {SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V,
                               SDL_SCANCODE_B, SDL_SCANCODE_N, SDL_SCANCODE_M};
    for (int i = 0; i < 7; i++) m(r3[i], (uint8_t)(0x2C + i));
    m(SDL_SCANCODE_COMMA, 0x33); m(SDL_SCANCODE_PERIOD, 0x34); m(SDL_SCANCODE_SLASH, 0x35); m(SDL_SCANCODE_RSHIFT, 0x36);
    m(SDL_SCANCODE_KP_MULTIPLY, 0x37); m(SDL_SCANCODE_LALT, 0x38); m(SDL_SCANCODE_SPACE, 0x39); m(SDL_SCANCODE_CAPSLOCK, 0x3A);
    const SDL_Scancode fk[] = {SDL_SCANCODE_F1, SDL_SCANCODE_F2, SDL_SCANCODE_F3, SDL_SCANCODE_F4, SDL_SCANCODE_F5,
                               SDL_SCANCODE_F6, SDL_SCANCODE_F7, SDL_SCANCODE_F8, SDL_SCANCODE_F9, SDL_SCANCODE_F10};
    for (int i = 0; i < 10; i++) m(fk[i], (uint8_t)(0x3B + i));
    m(SDL_SCANCODE_NUMLOCKCLEAR, 0x45); m(SDL_SCANCODE_SCROLLLOCK, 0x46);
    m(SDL_SCANCODE_KP_7, 0x47); m(SDL_SCANCODE_KP_8, 0x48); m(SDL_SCANCODE_KP_9, 0x49); m(SDL_SCANCODE_KP_MINUS, 0x4A);
    m(SDL_SCANCODE_KP_4, 0x4B); m(SDL_SCANCODE_KP_5, 0x4C); m(SDL_SCANCODE_KP_6, 0x4D); m(SDL_SCANCODE_KP_PLUS, 0x4E);
    m(SDL_SCANCODE_KP_1, 0x4F); m(SDL_SCANCODE_KP_2, 0x50); m(SDL_SCANCODE_KP_3, 0x51); m(SDL_SCANCODE_KP_0, 0x52);
    m(SDL_SCANCODE_KP_PERIOD, 0x53); m(SDL_SCANCODE_F11, 0x57); m(SDL_SCANCODE_F12, 0x58);
    m(SDL_SCANCODE_KP_ENTER, 0x9C); m(SDL_SCANCODE_RCTRL, 0x9D); m(SDL_SCANCODE_KP_DIVIDE, 0xB5);
    m(SDL_SCANCODE_RALT, 0xB8); m(SDL_SCANCODE_PAUSE, 0xC5); m(SDL_SCANCODE_HOME, 0xC7); m(SDL_SCANCODE_UP, 0xC8);
    m(SDL_SCANCODE_PAGEUP, 0xC9); m(SDL_SCANCODE_LEFT, 0xCB); m(SDL_SCANCODE_RIGHT, 0xCD); m(SDL_SCANCODE_END, 0xCF);
    m(SDL_SCANCODE_DOWN, 0xD0); m(SDL_SCANCODE_PAGEDOWN, 0xD1); m(SDL_SCANCODE_INSERT, 0xD2); m(SDL_SCANCODE_DELETE, 0xD3);
    m(SDL_SCANCODE_LGUI, 0xDB); m(SDL_SCANCODE_RGUI, 0xDC);
}

bool g_focused = true;
uint8_t g_injected[256];
std::vector<int> g_keyBuffer;
// DIK -> list of SDL scancodes overriding the default mapping
std::map<int, std::vector<SDL_Scancode>> g_remap;
// joystick button -> gamepad buttons
std::map<int, std::vector<SDL_GamepadButton>> g_padRemap;

bool g_blocked = false, g_blockUntilReleased = false;

struct Pad { SDL_Gamepad* pad; SDL_JoystickID id; };
std::vector<Pad> g_pads;

void OpenPad(SDL_JoystickID id) {
    if (g_pads.size() >= 4) return;
    for (auto& p : g_pads)
        if (p.id == id) return;
    SDL_Gamepad* gp = SDL_OpenGamepad(id);
    if (gp) {
        g_pads.push_back({gp, id});
        Log("Gamepad connected: %s", SDL_GetGamepadName(gp));
    }
}

void ClosePad(SDL_JoystickID id) {
    for (size_t i = 0; i < g_pads.size(); i++)
        if (g_pads[i].id == id) {
            SDL_CloseGamepad(g_pads[i].pad);
            g_pads.erase(g_pads.begin() + i);
            return;
        }
}

int32_t AxisToDI(int16_t v, int deadzonePct) {
    float f = v / 32767.f;
    float dz = deadzonePct / 100.f;
    float a = fabsf(f);
    if (a < dz) return 0;
    float s = (a - dz) / (1.f - dz);
    if (s > 1.f) s = 1.f;
    return (int32_t)((f < 0 ? -s : s) * 1000.f);
}
}  // namespace

namespace Input {

static bool IsBlocked();

void ReloadBindings() {
    const Config& cfg = GetConfig();
    g_remap.clear();
    g_padRemap.clear();
    for (auto& kv : cfg.keyBinds) {
        std::vector<SDL_Scancode> v;
        for (auto& name : kv.second) {
            SDL_Scancode sc = SDL_GetScancodeFromName(name.c_str());
            if (sc != SDL_SCANCODE_UNKNOWN) v.push_back(sc);
            else Log("Input: unknown key name '%s'", name.c_str());
        }
        g_remap[kv.first] = v;
    }
    for (auto& kv : cfg.padBinds) {
        std::vector<SDL_GamepadButton> v;
        for (auto& name : kv.second) {
            SDL_GamepadButton b = SDL_GetGamepadButtonFromString(name.c_str());
            if (b != SDL_GAMEPAD_BUTTON_INVALID) v.push_back(b);
            else Log("Input: unknown gamepad button '%s'", name.c_str());
        }
        g_padRemap[kv.first] = v;
    }
}

bool Init() {
    BuildTable();
    ReloadBindings();
    int n = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&n);
    for (int i = 0; i < n; i++) OpenPad(ids[i]);
    SDL_free(ids);
    return true;
}

void Shutdown() {
    for (auto& p : g_pads) SDL_CloseGamepad(p.pad);
    g_pads.clear();
}

void DiksForScancode(int sc, std::vector<int>* out) {
    out->clear();
    for (auto& kv : g_remap)
        for (auto s : kv.second)
            if (s == sc) out->push_back(kv.first);
    if (out->empty() && sc >= 0 && sc < SDL_SCANCODE_COUNT && g_sdlToDik[sc] && !g_remap.count(g_sdlToDik[sc]))
        out->push_back(g_sdlToDik[sc]);
}

void HandleEvent(const SDL_Event& e) {
    switch (e.type) {
        case SDL_EVENT_GAMEPAD_ADDED: OpenPad(e.gdevice.which); break;
        case SDL_EVENT_GAMEPAD_REMOVED: ClosePad(e.gdevice.which); break;
        case SDL_EVENT_KEY_DOWN:
            if (!e.key.repeat && g_focused && !IsBlocked()) {
                // buffered input reports the DIK code the physical key is bound to
                std::vector<int> diks;
                DiksForScancode(e.key.scancode, &diks);
                for (int d : diks) g_keyBuffer.push_back(d);
                if (g_keyBuffer.size() > 64) g_keyBuffer.erase(g_keyBuffer.begin());
            }
            break;
        default: break;
    }
}

void SetFocus(bool f) {
    g_focused = f;
    if (!f) g_keyBuffer.clear();
}

static bool AnyPhysicalInputHeld() {
    int n = 0;
    const bool* ks = SDL_GetKeyboardState(&n);
    for (int i = 0; i < n; i++)
        if (ks[i]) return true;
    for (auto& p : g_pads) {
        for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++)
            if (SDL_GetGamepadButton(p.pad, (SDL_GamepadButton)b)) return true;
        for (int a = 0; a < SDL_GAMEPAD_AXIS_COUNT; a++)
            if (abs(SDL_GetGamepadAxis(p.pad, (SDL_GamepadAxis)a)) > 16000) return true;
    }
    return false;
}

static bool IsBlocked() {
    if (g_blocked) return true;
    if (g_blockUntilReleased) {
        if (AnyPhysicalInputHeld()) return true;
        g_blockUntilReleased = false;
    }
    return false;
}

void SetBlocked(bool blocked) {
    if (blocked) {
        g_blocked = true;
        g_keyBuffer.clear();
    } else if (g_blocked) {
        g_blocked = false;
        g_blockUntilReleased = true;  // keys used to close the menu must not reach the game
    }
}

void DefaultScancodesForDik(int dik, std::vector<int>* out) {
    out->clear();
    for (int sc = 1; sc < SDL_SCANCODE_COUNT; sc++)
        if (g_sdlToDik[sc] == dik) out->push_back(sc);
}

void GetKeyboardState(uint8_t out[256]) {
    memset(out, 0, 256);
    int n = 0;
    if (!g_focused || IsBlocked()) {
        for (int i = 0; i < 256; i++)
            if (g_injected[i]) out[i] = 0x80;
        return;
    }
    const bool* ks = SDL_GetKeyboardState(&n);
    // default mapping, skipping DIK codes that have been remapped
    for (int sc = 0; sc < n && sc < SDL_SCANCODE_COUNT; sc++) {
        uint8_t d = g_sdlToDik[sc];
        if (d && ks[sc] && !g_remap.count(d)) out[d] = 0x80;
    }
    for (auto& kv : g_remap)
        for (auto sc : kv.second)
            if (sc < n && ks[sc]) out[kv.first] = 0x80;
    for (int i = 0; i < 256; i++)
        if (g_injected[i]) out[i] = 0x80;
}

int DikFromScancode(int sc) {
    if (sc <= 0 || sc >= SDL_SCANCODE_COUNT) return 0;
    for (auto& kv : g_remap)
        for (auto s : kv.second)
            if (s == sc) return kv.first;
    return g_sdlToDik[sc];
}

void InjectKey(int dik, bool down) {
    if (dik <= 0 || dik > 255) return;
    g_injected[dik] = down ? 1 : 0;
    if (down) g_keyBuffer.push_back(dik);
}

int NumJoysticks() { return (int)g_pads.size(); }

bool GetJoystickState(int index, DIJOYSTATE* js) {
    if (index < 0 || index >= (int)g_pads.size()) return false;
    memset(js, 0, sizeof *js);
    for (auto& p : js->rgdwPOV) p = 0xFFFFFFFF;
    if (!g_focused || IsBlocked()) return true;
    SDL_Gamepad* gp = g_pads[index].pad;
    int dz = GetConfig().gamepadDeadzone;
    js->lX = AxisToDI(SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTX), dz);
    js->lY = AxisToDI(SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTY), dz);
    js->lZ = AxisToDI(SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_RIGHTX), dz);
    js->lRz = AxisToDI(SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_RIGHTY), dz);
    // D-pad drives the main axes too (classic pads reported it that way)
    if (SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) js->lX = -1000;
    if (SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) js->lX = 1000;
    if (SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_UP)) js->lY = -1000;
    if (SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) js->lY = 1000;
    static const SDL_GamepadButton defaults[] = {
        SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
        SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_GAMEPAD_BUTTON_BACK,
        SDL_GAMEPAD_BUTTON_START, SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK};
    for (int b = 0; b < 32; b++) {
        bool down = false;
        auto it = g_padRemap.find(b);
        if (it != g_padRemap.end()) {
            for (auto gb : it->second) down |= SDL_GetGamepadButton(gp, gb);
        } else if (b < (int)(sizeof defaults / sizeof defaults[0])) {
            down = SDL_GetGamepadButton(gp, defaults[b]);
        }
        js->rgbButtons[b] = down ? 0x80 : 0;
    }
    // Triggers act as extra buttons 10/11
    if (!g_padRemap.count(10) && SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 16000) js->rgbButtons[10] = 0x80;
    if (!g_padRemap.count(11) && SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 16000) js->rgbButtons[11] = 0x80;
    return true;
}

void GetBufferedKeys(std::vector<int>* out) {
    out->swap(g_keyBuffer);
    g_keyBuffer.clear();
}

}  // namespace Input
