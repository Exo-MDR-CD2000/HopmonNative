// Hopmon game classes (XAppMain, XTitle, XGame, XMap, XMapObject, XChara, XEnemy, XPac, XRegister).
// Member names, types and layout follow the original program's debug information.
#pragma once
#include "../framework/d3dapp.h"
#include "JLib.h"
#include "JObject3D.h"

class XApplication;
class XGameObject;
class XCamera;
class XMap;
class XPac;
class XEnemyList;
class XEnemyBase;
class XMapObjectList;
class XMapObjectBase;
class XPacFireList;
class XEnemyFireList;
class XParticleList;
class XCrystalList;
class XCrystal;
class XStatusDisplay;
class XTitleBackground;
class XPreviewMap;

// ---------------------------------------------------------------- XRegister
void LoadRegisterInfo(const char* key);
void SaveRegisterInfo(const char* key);
void DeleteRegisterInfo();
int IsRegistered();

// ------------------------------------------------------------------ helpers
WORD GetMapResId(XApplication* app);
D3DVECTOR GetVectorFromDir(int dir);
long RenderRandom(long range);  // cosmetic RNG, separate from the gameplay LCG
int CheckButtonPressed(JDInput* input);

// --------------------------------------------------------------- XAppMain
class XFPSDisplay {
public:
    explicit XFPSDisplay(XApplication* app);
    virtual ~XFPSDisplay();
    void Render();
    XApplication* app;
    JSprite* textFPS;
    JNumber* numFPS;
};

class XProcessObject;

class XApplication : public CD3DApplication {
public:
    enum STATE { NON = 0, EXIT = 1, IN_PROCESS = 2, START_LOGO = 3, START_TITLE = 4, START_SELLEVEL = 5,
                 START_GAME = 6, START_REGINFO = 7, START_ENDING = 8 };
    XApplication();
    ~XApplication() override;
    void LoadWinRegData();
    void SaveWinRegData();
    void SetState(STATE s) { state = s; }
    int GetCurrentWorld() const { return currentLevel / 15; }
    JDInput* GetDInput() const { return dinput; }
    JMidiList* GetMidiList() const { return midiList; }
    JSoundList* GetSoundList() const { return soundList; }
    JSurfaceList* GetSurfaceList() const { return surfaceList; }
    HRESULT Create(const std::vector<std::string>& args) override;
    void PlayUISound(UISound s) override;
    bool CanReturnToMainMenu() const override;
    void ReturnToMainMenu() override;
    bool InLevel() const override;
    bool CanOpenOptions() const override;
    void ReloadMusic() override { if (midiList) midiList->Reload(); }
    const char* MusicSource() const override { return midiList ? midiList->CurrentSource() : ""; }

    int currentLevel;
    int maxSelLevel;              // real progress (saved to the registry, shared with Hopmon.exe)
    int SelectableLevel() const;  // highest level offered on level select (cheat-aware)
    const int maxFreeLevel;
    const int maxLevel;

protected:
    HRESULT OneTimeSceneInit() override;
    HRESULT InitDeviceObjects() override;
    HRESULT DeleteDeviceObjects() override;
    HRESULT Render() override;
    HRESULT FrameMove(DWORD frameCount) override;
    HRESULT FinalCleanup() override;
    HRESULT RestoreSurfaces() override;
    void OnActivateApp(bool active) override;
    void OnKeyDown(int dik) override;
    void OnAutomation(const std::string& action, const std::string& arg) override;
    void OnMidiNotifySuccessful() override;

    JStringList* tempList;
    STATE state;
    JDInput* dinput;
    JMidiList* midiList;
    JSoundList* soundList;
    JSurfaceList* surfaceList;
    XProcessObject* processObj;
    XFPSDisplay* fpsDisplay;
};

int HopmonMain(const std::vector<std::string>& args);

// ---------------------------------------------------------- XProcessObject
class XProcessObject {
public:
    enum STATE { NON = 0, FADEIN = 1, FADEOUT = 2, PROCESS = 3 };
    explicit XProcessObject(XApplication* app);
    virtual ~XProcessObject();
    virtual void ProcessFrame(DWORD frameCount) { (void)frameCount; }
    virtual void Render() {}
    void StartFadeIn(int type);
    void StartFadeOut(int type);
    void StartProcess(int type);
    void ProcessFadeIn();
    void ProcessFadeOut();

    XApplication* app;
    STATE state;
    unsigned tick;
    JSprite* fadeLeft;
    JSprite* fadeRight;
    int processType;
    int processFinished;

protected:
    void _ResetFadeIn();
    void _ResetFadeOut();
    void DrawFade();  // port helper: draws the two shutter halves
};

// ------------------------------------------------------------------ XTitle
class XTitleBackground {
public:
    explicit XTitleBackground(XApplication* app);
    virtual ~XTitleBackground();
    void ProcessFrame(DWORD frameCount);
    void Render();
    XApplication* app;
    JObject3D_Rect* backUpper;
    JObject3D_Rect* backLower;
    float textureOffset;
};

class XLogoObject : public XProcessObject {
public:
    explicit XLogoObject(XApplication* app);
    ~XLogoObject() override;
    void ProcessFrame(DWORD frameCount) override;
    void Render() override;
    XCamera* camera;
    JSprite* logo;
    JObject3D_Rect* backWhite;
};

class XTitleObject : public XProcessObject {
public:
    explicit XTitleObject(XApplication* app);
    ~XTitleObject() override;
    void ProcessFrame(DWORD frameCount) override;
    void Render() override;
    XCamera* camera;
    JObject3DX* pac;
    XTitleBackground* background;
    JSprite* cursorLeft;
    JSprite* cursorRight;
    JSprite* menuItems[2];
    JSprite* textHopmon;
    JSprite* textCopyright;
    JSprite* textUnreg;
    int cursorPos;
};

class XSelectLevelObject : public XProcessObject {
public:
    explicit XSelectLevelObject(XApplication* app);
    ~XSelectLevelObject() override;
    void ProcessFrame(DWORD frameCount) override;
    void Render() override;
    XCamera* camera;
    XTitleBackground* background;
    XPreviewMap* map;
    JSprite* cursorLeft;
    JSprite* cursorRight;
    JSprite* cursor2Left;
    JSprite* cursor2Right;
    JSprite* menuItems[3];
    JSprite* textUnreg;
    JSprite* textLevel;
    JSprite* levelLED;
    JNumber* numLevel;
    int cursorPos;
    int inSelectLevel;

protected:
    void CreatePreviewMap();
};

class XRegisterInfoObject : public XProcessObject {
public:
    explicit XRegisterInfoObject(XApplication* app);
    ~XRegisterInfoObject() override;
    void ProcessFrame(DWORD frameCount) override;
    void Render() override;
    XCamera* camera;
    XTitleBackground* background;
    JSprite* cursorLeft;
    JSprite* cursorRight;
    JSprite* menuBack;
    JSprite* screenShot1;
    JSprite* screenShot2;
    JSprite* regInfo;
};

class XEndingObject : public XProcessObject {
public:
    explicit XEndingObject(XApplication* app);
    ~XEndingObject() override;
    void ProcessFrame(DWORD frameCount) override;
    void Render() override;
    XCamera* camera;
    JObject3DX* chara[10];
    XTitleBackground* background;
    JSprite* textCongra;
    JSprite* textStaff;
    JSprite* textCopyright;
    int charaIndex;
    float charaMaxScale[10];
};

// ------------------------------------------------------------------- XGame
class XStatusDisplay {
public:
    explicit XStatusDisplay(XGameObject* game);
    virtual ~XStatusDisplay();
    void Draw(IDirectDrawSurface7* dest);
    XGameObject* game;
    JSprite* textLevel;
    JSprite* textCrystals;
    JSprite* textFireReady;
    JSprite* fireLED;
    JNumber* numLevel;
    JNumber* numCrystals;
};

class XGameObject : public XProcessObject {
public:
    explicit XGameObject(XApplication* app);
    ~XGameObject() override;
    void ProcessFrame(DWORD frameCount) override;
    void Render() override;
    void InitLevel();
    int GetCurrentLevel() const;
    BYTE GetVolumeOfPosition(D3DVECTOR pos);

    XCamera* camera;
    XMap* map;
    XPac* pac;
    XEnemyList* enemyList;
    XMapObjectList* mapObjList;
    XPacFireList* pacFireList;
    XEnemyFireList* enemyFireList;
    XParticleList* particleList;
    XCrystalList* crystalList;
    XStatusDisplay* statusDisplay;
};

// -------------------------------------------------------------------- XMap
class XCamera : public Interp::Tracked {
public:
    enum STATE { NON = 0, WAIT = 1, ROTATE_LEFT = 2, ROTATE_RIGHT = 3, LEVEL_COMPLETED = 4, GAME_OVER = 5 };
    explicit XCamera(XApplication* app);
    virtual ~XCamera();
    void ResetForGame(XPac* pac);
    void ResetForTitle();
    void DoSetTransform();
    STATE GetState() const { return state; }
    int GetViewDirection() const { return viewDir; }
    const D3DMATRIX& GetViewMatrix() const { return renderView_; }  // only used for rendering billboards
    void StartRotatingLeft();
    void StartRotatingRight();
    void StartLevelCompleted();
    void StartGameOver();
    void IncrVerticalAngle(DWORD frameCount);
    void DecrVerticalAngle(DWORD frameCount);
    void ProcessFrame(XPac* pac);
    void SnapshotPrev() override;

    XApplication* app;
    STATE state;
    unsigned localTime;
    D3DVECTOR lookAtPos;
    D3DVECTOR refPos;
    float distance;
    float verticalAngle;
    float horizontalAngle;
    int viewDir;
    D3DMATRIX viewMatrix;

protected:
    void UpdateViewMatrix();
    // interpolation (render only)
    D3DVECTOR prevLookAt;
    float prevDistance = 0, prevVertical = 0, prevHorizontal = 0;
    bool havePrev = false;
    D3DMATRIX renderView_;
};

class XPreviewMap {
public:
    XPreviewMap(XApplication* app, int resId);
    virtual ~XPreviewMap();
    void ProcessFrame(DWORD frameCount);
    void Render();
    XApplication* app;
    BYTE* mapArray;
    JObject3D* mapObject;
    float chipSize;
    int mapSizeX;
    int mapSizeZ;
    WORD mapTextureId;

protected:
    void _LoadMapArray(int resId);
    void _CreateMapObject();
    int _ConvertMapItemToInt(BYTE item);
    void _CreateFloor(JObject3DList& list, BYTE item, float x, float z);
    void _CreateWalls(JObject3DList& list, int item, int mapX, int mapZ, float x, float z);
};

class XMap {
public:
    XMap(XApplication* app, int resId, XPac* pac, XEnemyList* enemies, XMapObjectList* objects, XCrystalList* crystals);
    virtual ~XMap();
    void ProcessFrame();
    void Render();
    const float& GetChipSize() const { return chipSize; }
    int IsBlocked_Pac(int x, int z);
    int IsBlocked_Fire(int x, int z);
    int IsBlocked_Enemy(int x, int z);
    int IsBlocked_UFO(int x, int z);
    int IsWarpZone(int x, int z);
    int IsArrow(int x, int z);
    int GetArrowDirection(int x, int z);
    int ConvertPosXToMapX(float x);
    int ConvertPosZToMapZ(float z);
    void UpdateEnemyMap(int oldX, int oldZ, int newX, int newZ);
    void ZeroEnemyMap(int x, int z);
    void UpdateUFOMap(int oldX, int oldZ, int newX, int newZ);

    XApplication* app;
    BYTE* mapArray;
    BYTE* enemyMapArray;
    BYTE* ufoMapArray;
    JObject3D* mapObject;
    JObject3D_Rect* background;
    float chipSize;
    int mapSizeX;
    int mapSizeZ;
    WORD mapTextureId;

protected:
    void _LoadMapArray(int resId);
    void _CreateMapObject(XPac* pac, XEnemyList* enemies, XMapObjectList* objects, XCrystalList* crystals);
    void _CreateBackground();
    int _ConvertMapItemToInt(BYTE item);
    void _CreateFloor(JObject3DList& list, BYTE item, float x, float z);
    void _CreateWalls(JObject3DList& list, int item, int mapX, int mapZ, float x, float z);
    int _IsOutsideMap(int x, int z);
};

// -------------------------------------------------------------- XMapObject
class XMapObjectBase : public JObject3DX {
public:
    enum STATE { NON = 0, PROCESS = 1 };
    XMapObjectBase(XGameObject* game, WORD resId);
    ~XMapObjectBase() override;
    void Render() override;
    STATE GetState() const { return state; }
    void SetDirection(int dir, int rotate);
    virtual void ProcessRotate(DWORD frameCount) { (void)frameCount; }
    virtual void ProcessFrame() {}

    XGameObject* game;
    STATE state;
    unsigned localTime;
    int direction;
    JObject3D_Shadow* shadow;
};

class XVolcano : public XMapObjectBase {
public:
    explicit XVolcano(XGameObject* game);
    void ProcessFrame() override;
};
class XCannonUp : public XMapObjectBase {
public:
    explicit XCannonUp(XGameObject* game);
    void ProcessFrame() override;
};
class XCannon : public XMapObjectBase {
public:
    XCannon(XGameObject* game, int dir);
    void ProcessFrame() override;
};
class XPole : public XMapObjectBase {
public:
    explicit XPole(XGameObject* game);
};
class XShortPole : public XMapObjectBase {
public:
    explicit XShortPole(XGameObject* game);
};
class XTree : public XMapObjectBase {
public:
    explicit XTree(XGameObject* game);
};
class XSteam : public XMapObjectBase {
public:
    explicit XSteam(XGameObject* game);
    void ProcessFrame() override;
};

class XMapObjectList {
public:
    explicit XMapObjectList(XGameObject* game);
    virtual ~XMapObjectList();
    XMapObjectBase* CreateMapObject(BYTE item);
    void Empty();
    void ProcessRotate(DWORD frameCount);
    void ProcessFrame();
    void Render();
    XGameObject* game;
    XMapObjectBase* objArray[25];

protected:
    int GetNullIndex();
};

// ------------------------------------------------------------------ XChara
class XCrystal : public JObject3DX {
public:
    enum STATE { NON = 0, PROCESS = 1, TAKEN = 2, VANISHING = 3, GAME_OVER = 4 };
    explicit XCrystal(XGameObject* game);
    ~XCrystal() override;
    void Render() override;
    STATE GetState() const { return state; }
    void SetState(STATE s) { state = s; }
    int GetTakenPosition() const { return takenPosition; }
    void StartTaken(int pos);
    void StartVanishing();
    void StartGameOver();
    void ProcessRotate(DWORD frameCount);
    void ProcessFrame();

    XGameObject* game;
    STATE state;
    unsigned localTime;
    JObject3D_Shadow* shadow;
    int takenPosition;
    D3DVECTOR dirVector;
    D3DVECTOR referencePos;
};

class XCrystalList {
public:
    explicit XCrystalList(XGameObject* game);
    virtual ~XCrystalList();
    XCrystal* CreateCrystal();
    void Empty();
    void CheckCollisionWithPac();
    void DoVanishCrystal();
    void StartGameOver();
    int GetTakenCount() const { return takenCount; }
    int GetCrystalLeft() const { return crystalLeft; }
    void ProcessRotate(DWORD frameCount);
    void ProcessFrame();
    void Render();
    XGameObject* game;
    XCrystal* crystalArray[30];
    int takenCount;
    int crystalLeft;

protected:
    int GetNullIndex();
};

class XParticleBase : public JObject3D_Particle {
public:
    enum STATE { NON = 0, PROCESS = 1 };
    XParticleBase(XGameObject* game, D3DVECTOR pos, float size);
    ~XParticleBase() override;
    void Render2(const D3DMATRIX& view) override;
    STATE GetState() const { return state; }
    virtual void ProcessFrame() {}
    XGameObject* game;
    STATE state;
    unsigned localTime;
    D3DVECTOR refrencePos;
};

class XParticle_EnemyExplode : public XParticleBase {
public:
    XParticle_EnemyExplode(XGameObject* game, D3DVECTOR pos);
    void SetDirVector(D3DVECTOR v) { dirVector = v; }
    void ProcessFrame() override;
    D3DVECTOR dirVector;
};
class XParticle_EnemyFireSmokePurple : public XParticleBase {
public:
    XParticle_EnemyFireSmokePurple(XGameObject* game, D3DVECTOR pos);
    void ProcessFrame() override;
};
class XParticle_EnemyFireSmokeGreen : public XParticleBase {
public:
    XParticle_EnemyFireSmokeGreen(XGameObject* game, D3DVECTOR pos);
    void ProcessFrame() override;
};
class XParticle_PacFireHit : public XParticleBase {
public:
    XParticle_PacFireHit(XGameObject* game, D3DVECTOR pos, float size, unsigned frames);
    void Render2(const D3DMATRIX& view) override;
    void ProcessFrame() override;
    unsigned processFrame;
    unsigned renderCount;

private:
    void _SetTextureRandomly();
};

class XParticleList {
public:
    explicit XParticleList(XGameObject* game);
    virtual ~XParticleList();
    void Empty();
    void ProcessFrame();
    void Render(const D3DMATRIX& view);
    void CreateEnemyExplode(D3DVECTOR pos);
    void CreateEnemyFireSmokePurple(D3DVECTOR pos);
    void CreateEnemyFireSmokeGreen(D3DVECTOR pos);
    void CreatePacFireHit(D3DVECTOR pos);
    void CreatePacVanish(D3DVECTOR pos);
    XGameObject* game;
    WORD _arrayPointer;
    XParticleBase* particleArray[150];

protected:
    int GetNullIndex();
};

class XEnemyFireBase : public JObject3D_Particle {
public:
    enum STATE { NON = 0, MOVE = 1 };
    XEnemyFireBase(XGameObject* game, D3DVECTOR pos, float size);
    ~XEnemyFireBase() override;
    void Render2(const D3DMATRIX& view) override;
    STATE GetState() const { return state; }
    void SetState(STATE s) { state = s; }
    virtual void ProcessFrame() {}
    XGameObject* game;
    STATE state;
    unsigned localTime;
    D3DVECTOR refrencePos;
};

class XEnemyFire_CannonUp : public XEnemyFireBase {
public:
    XEnemyFire_CannonUp(XGameObject* game, D3DVECTOR pos);
    void SetValues(float dist, D3DVECTOR dir);
    void ProcessFrame() override;
    float distance;
    D3DVECTOR dirVector;
};
class XEnemyFire_Cannon : public XEnemyFireBase {
public:
    XEnemyFire_Cannon(XGameObject* game, D3DVECTOR pos);
    void SetValues(D3DVECTOR dir) { dirVector = dir; }
    void ProcessFrame() override;
    D3DVECTOR dirVector;
};
class XEnemyFire_UFO : public XEnemyFireBase {
public:
    XEnemyFire_UFO(XGameObject* game, D3DVECTOR pos);
    void ProcessFrame() override;
    D3DVECTOR dirVector;
    int turned;
};
class XEnemyFire_Steam : public XEnemyFireBase {
public:
    XEnemyFire_Steam(XGameObject* game, D3DVECTOR pos);
    void ProcessFrame() override;
};

class XEnemyFireList {
public:
    explicit XEnemyFireList(XGameObject* game);
    virtual ~XEnemyFireList();
    void CheckCollisionWithPac();
    void Empty();
    void ProcessFrame();
    void Render(const D3DMATRIX& view);
    void CreateCannonUpFire(D3DVECTOR pos);
    void CreateVolcanoFire(D3DVECTOR pos);
    void CreateCannonFire(D3DVECTOR pos, int dir);
    void CreateUFOFire(D3DVECTOR pos);
    void CreateSteamFire(D3DVECTOR pos);
    XGameObject* game;
    XEnemyFireBase* fireArray[60];

protected:
    int GetNullIndex();
};

// ------------------------------------------------------------------ XEnemy
class XEnemyBase : public JObject3DX {
public:
    enum STATE { NON = 0, WAIT = 1, MOVE = 2, FIRE = 3, DYING = 4 };
    XEnemyBase(XGameObject* game, WORD resId);
    ~XEnemyBase() override;
    void Render() override;
    STATE GetState() const { return state; }
    int Breakable() const { return breakable; }
    void SetMapPos(int x, int z) { mapPosX = x; mapPosZ = z; }
    void SetDirection(int dir);
    void StartMoving(int dir);
    void StartDying(int dir);
    void StartFiring();
    void DecreaseShield(int amount);
    void ProcessRotate(DWORD frameCount);
    virtual void SubProcessDyingRotate(DWORD frameCount);
    virtual void SubProcessMoveRotate(DWORD frameCount) { (void)frameCount; }
    virtual void SubProcessFireRotate(DWORD frameCount) { (void)frameCount; }
    void ProcessFrame();
    virtual void SubProcessDying();
    virtual void SubProcessMove() {}
    virtual void SubProcessWait() {}
    virtual void SubProcessFire() {}

    XGameObject* game;
    STATE state;
    unsigned localTime;
    int mapPosX;
    int mapPosZ;
    D3DVECTOR refrencePos;
    int direction;
    JObject3D_Shadow* shadow;
    int faceToDir;
    int breakable;
    int shield;

protected:
    int IsBlocked(int dir);
    int FindDirectionToPac();
    void DefaultProcessWait();
};

class XEnemy_Ghost : public XEnemyBase {
public:
    explicit XEnemy_Ghost(XGameObject* game);
    void SubProcessMove() override;
    void SubProcessWait() override;
};
class XEnemy_Cube : public XEnemyBase {
public:
    explicit XEnemy_Cube(XGameObject* game);
    void SubProcessMoveRotate(DWORD frameCount) override;
    void SubProcessMove() override;
    void SubProcessWait() override;
};
class XEnemy_Kurage : public XEnemyBase {
public:
    explicit XEnemy_Kurage(XGameObject* game);
    void SubProcessMove() override;
    void SubProcessWait() override;
};
class XEnemy_UFO : public XEnemyBase {
public:
    explicit XEnemy_UFO(XGameObject* game);
    void SubProcessMoveRotate(DWORD frameCount) override;
    void SubProcessFireRotate(DWORD frameCount) override;
    void SubProcessMove() override;
    void SubProcessWait() override;
    void SubProcessFire() override;
    int moveCount;
    int countDownToFire;

private:
    void StartMoving_UFO(int dir);
    int IsBlocked_UFO(int dir);
};
class XEnemy_Sesame : public XEnemyBase {
public:
    explicit XEnemy_Sesame(XGameObject* game);
    void SubProcessMove() override;
    void SubProcessWait() override;
    int moveCountDown;
    int movePhase;
};
class XNeedleBall : public XEnemyBase {
public:
    XNeedleBall(XGameObject* game, int dir);
    void SubProcessMoveRotate(DWORD frameCount) override;
    void SubProcessMove() override;
    void SubProcessWait() override;
};
class XNeedleBallFast : public XEnemyBase {
public:
    XNeedleBallFast(XGameObject* game, int dir);
    void SubProcessMoveRotate(DWORD frameCount) override;
    void SubProcessMove() override;
    void SubProcessWait() override;
};
class XNeedleBox : public XEnemyBase {
public:
    explicit XNeedleBox(XGameObject* game);
    void SubProcessMove() override;
    void SubProcessWait() override;
};
class XNeedleBoxDrop : public XEnemyBase {
public:
    explicit XNeedleBoxDrop(XGameObject* game);
    void SubProcessMove() override;
    void SubProcessWait() override;
};
class XGate : public XEnemyBase {
public:
    XGate(XGameObject* game, int dir);
};

class XEnemyList {
public:
    explicit XEnemyList(XGameObject* game);
    virtual ~XEnemyList();
    XEnemyBase* CreateEnemy(BYTE item);
    XEnemyBase* GetAt(DWORD index) const { return enemyArray[index]; }
    void Empty();
    void CheckCollisionWithPac();
    void ProcessRotate(DWORD frameCount);
    void ProcessFrame();
    void Render();
    XGameObject* game;
    XEnemyBase* enemyArray[30];

protected:
    int GetNullIndex();
};

// -------------------------------------------------------------------- XPac
class XPacFire : public JObject3D_Particle {
public:
    enum STATE { NON = 0, MOVE = 1 };
    XPacFire(XGameObject* game, int dir);
    ~XPacFire() override;
    void Render2(const D3DMATRIX& view) override;
    STATE GetState() const { return state; }
    void SetState(STATE s) { state = s; }
    int GetDirection() const { return direction; }
    void ProcessFrame();
    XGameObject* game;
    STATE state;
    unsigned localTime;
    int direction;

protected:
    void _SetTextureRandomly();
};

class XPacFireList {
public:
    explicit XPacFireList(XGameObject* game);
    virtual ~XPacFireList();
    void CreateFire();
    void Empty();
    void CheckCollisionWithEnemy();
    void ProcessFrame();
    void Render(const D3DMATRIX& view);
    XGameObject* game;
    XPacFire* fireArray[3];

protected:
    int GetNullIndex();
};

class XPac : public JObject3DX {
public:
    enum STATE { NON = 0, WAIT = 1, MOVE = 2, ON_ARROW = 3, LEVEL_COMPLETED = 4, DYING = 5 };
    explicit XPac(XGameObject* game);
    ~XPac() override;
    void Render() override;
    void Reset();
    void StartLevelCompleted();
    STATE GetState() const { return state; }
    int GetFaceDirection() const { return faceDirection; }
    void SetMapPos(int x, int z) { mapPosX = x; mapPosZ = z; }
    int GetMapPosX() const { return mapPosX; }
    int GetMapPosZ() const { return mapPosZ; }
    int GetFireEnergy() const { return fireEnergy; }
    void ResetFireEnergy() { fireEnergy = 0; }
    void SetFaceDirection(int dir);
    void StartMoving(int dir);
    void StartArrow(int dir);
    void StartDying();
    void ProcessRotate(DWORD frameCount);
    void ProcessFrame();

    XGameObject* game;
    STATE state;
    unsigned localTime;
    int mapPosX;
    int mapPosZ;
    int faceDirection;
    int moveDirection;
    D3DVECTOR refrencePos;
    JObject3D_Shadow* shadow;
    unsigned pacMoveFrame;
    int fireEnergy;
};
