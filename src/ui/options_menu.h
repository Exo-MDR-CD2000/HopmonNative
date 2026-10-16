// In-game options menu (overlay). Opened with F10 or the gamepad Start button from any screen;
// the game is paused underneath and receives no input while it is open. Every change is applied
// immediately and saved to hopmon_native.ini. Display-mode changes ask for confirmation and
// revert automatically after 10 seconds.
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "../core/config.h"
#include "../core/d3dtypes.h"
#include "ui_draw.h"

union SDL_Event;
class CD3DApplication;
class Device;

class OptionsMenu {
public:
    explicit OptionsMenu(CD3DApplication* app);
    bool IsOpen() const { return open_; }
    void Open();
    void Close();
    // Returns true if the event was consumed by the menu (always, while open, for input events).
    bool HandleEvent(const SDL_Event& e);
    void Update(DWORD nowMs);
    void Render(Device* d);

private:
    struct Item {
        std::string label;
        std::string help;
        std::function<std::string()> value;    // empty function = no value column
        std::function<void(int)> change;       // Left/Right (-1/+1)
        std::function<void()> activate;        // Enter (defaults to change(+1))
        std::function<bool()> enabled;         // greyed out when false
        int (*bar)() = nullptr;                // draws a 0..100 level bar next to the value
        std::function<std::string()> helpFn;   // dynamic help text (overrides `help` when set)
        bool focusWhenDisabled = false;        // cursor may rest on it while greyed out (to show why)
        bool submenu = false;                  // shows ">" after its value: Enter opens a page
        // list rows with a tick box (add-ons): number shown in front, on/off state, and the
        // Space / gamepad X toggle and Shift+Up/Down / LB/RB reordering actions
        int number = 0;                        // 0 = none
        std::function<int()> check;            // tick box: 1 ticked, 0 empty (empty function = none)
        std::function<void()> toggle;
        std::function<void(int)> reorder;      // -1 = move up, +1 = move down
    };
    struct Page {
        std::string title;
        std::vector<Item> items;
        int cursor = 0;
        int scroll = 0;  // first visible row when the page is taller than the panel
        std::string note;      // a line under the title ("" = none)
        std::string controls;  // replaces the standard controls line at the bottom ("" = standard)
    };
    enum class Mode { Normal, CaptureKey, CaptureButton, ConfirmDisplay, ConfirmAction };

    // navigation
    void Move(int d);
    void Change(int d);
    void Activate();
    void Back(bool sound = true);
    void PushPage(Page p);
    Page& Cur() { return stack_.back(); }

    // pages
    Page MainPage();
    Page VideoPage();
    Page GraphicsPage();
    Page AudioPage();
    Page ControlsPage();
    Page CheatsPage();
    Page ModsPage();
    Page LevelPackPage();
    Page AddonsPage();
    Page AddonDetailPage(const std::string& id);
    Page KeysPage();
    Page PadPage();

    // helpers
    void Save();
    void ResetAllSettings();
    void BeginDisplayChange();      // snapshot before a display change
    void ApplyDisplay();
    void RevertDisplay();
    // Display settings: Window/Borderless changes apply at once; anything that ends in exclusive
    // fullscreen is first held as a pending choice (shown with *) and applied with Enter, followed
    // by the keep/revert confirmation.
    struct DisplayCfg {
        FullscreenMode mode;
        int fw, fh;   // exclusive fullscreen resolution (0 = desktop)
        int display;  // monitor index (0 = primary)
        bool operator==(const DisplayCfg& o) const {
            return mode == o.mode && fw == o.fw && fh == o.fh && display == o.display;
        }
    };
    DisplayCfg CurrentDisplay() const;
    DisplayCfg ShownDisplay() const;  // the pending choice if there is one, else the current settings
    void ProposeDisplay(const DisplayCfg& d);
    void ApplyPending();
    void DiscardPending(bool tell);
    std::string DisplayHelp(const std::string& base) const;
    bool pendingActive_ = false;
    DisplayCfg pending_{};
    // sizes for a mode, smallest first; for exclusive the desktop resolution ({0,0}) comes last
    std::vector<std::pair<int, int>> ResolutionList(FullscreenMode mode, int display) const;
    std::vector<std::string> SynthList() const;
    std::string KeyNamesFor(int dik) const;
    std::string PadNamesFor(int button) const;
    void SetMessage(const std::string& m, DWORD ms = 3000);
    void Sound(bool confirm);
    std::string LostLine() const;
    void DrawTickBox(float x, float y, bool on, bool enabled);
    void AskConfirm(const std::string& title, const std::string& line, std::function<void()> yes);

    CD3DApplication* app_;
    bool open_ = false;
    std::vector<Page> stack_;
    Mode mode_ = Mode::Normal;
    int captureDik_ = 0;          // key being rebound
    int captureButton_ = 0;       // gamepad button being rebound
    std::string captureLabel_;
    // display confirmation
    struct DisplaySnapshot { int fullscreen, w, h, fw, fh, display; } snapshot_{};
    DWORD confirmDeadline_ = 0;
    int confirmChoice_ = 1;       // 0 = keep / yes, 1 = revert / no (default)
    // yes/no confirmation (main menu, quit)
    std::string askTitle_, askLine_;
    std::function<void()> askYes_;
    // gamepad stick repeat
    int stickDir_ = 0;            // repeating direction: -1 up, +1 down, -2 left, +2 right, 0 none
    int stickX_ = 0, stickY_ = 0; // per-axis state: -1, 0, +1
    unsigned stickPad_ = 0;       // SDL_JoystickID of the controller that set stickDir_
    DWORD stickNext_ = 0;
    static constexpr int kStickPress = 16384;    // 50%
    static constexpr int kStickRelease = 11500;  // 35%
    void ResetStick();
    DWORD now_ = 0;
    std::string message_;
    DWORD messageUntil_ = 0;
    UIDraw ui_;
    // mods / level pack changed in this visit: applied (assets reloaded, title restarted) on close
    bool modsChanged_ = false;
    bool ModsLocked() const;  // true during a level: mods can only be changed from the title screen
};
