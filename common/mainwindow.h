#pragma once

#include "camera.h"
#include "resources.h"

#include <Tempest/Window>
#include <Tempest/CommandBuffer>
#include <Tempest/Fence>
#include <Tempest/VulkanApi>
#include <Tempest/Device>
#include <Tempest/VertexBuffer>
#include <Tempest/UniformBuffer>
#include <Tempest/VectorImage>
#include <Tempest/Event>
#include <Tempest/Pixmap>
#include <Tempest/Sprite>
#include <Tempest/Font>
#include <Tempest/TextureAtlas>
#include <Tempest/Timer>
#include <Tempest/Swapchain>

#include <vector>
#include <thread>
#include <fstream>

#include "world/world.h"
#include "world/focus.h"
#include "game/playercontrol.h"
#include "graphics/renderer.h"
#include "ui/dialogmenu.h"
#include "ui/inventorymenu.h"
#include "ui/chapterscreen.h"
#include "ui/documentmenu.h"
#include "ui/videowidget.h"
#include "ui/menuroot.h"
#include "ui/consolewidget.h"
#if defined(__MOBILE_PLATFORM__)
#include "ui/touchinput.h"
#endif

#include "utils/keycodec.h"
#include "utils/gamepadbindings.h"
#include "resources.h"

class MenuRoot;
class GameSession;
class Interactive;

class MainWindow : public Tempest::Window {
  public:
    explicit MainWindow(Tempest::Device& device);
    ~MainWindow() override;

    float uiScale() const;

  private:
    void paintEvent     (Tempest::PaintEvent& event) override;
    void resizeEvent    (Tempest::SizeEvent & event) override;

    void mouseDownEvent (Tempest::MouseEvent& event) override;
    void mouseUpEvent   (Tempest::MouseEvent& event) override;
    void mouseDragEvent (Tempest::MouseEvent& event) override;
    void mouseMoveEvent (Tempest::MouseEvent& event) override;
    void mouseWheelEvent(Tempest::MouseEvent& event) override;

    void keyDownEvent   (Tempest::KeyEvent&   event) override;
    void keyRepeatEvent (Tempest::KeyEvent&   event) override;
    void keyUpEvent     (Tempest::KeyEvent&   event) override;

    void focusEvent     (Tempest::FocusEvent&  event) override;

    void paintFocus     (Tempest::Painter& p, const Focus& fc, const Tempest::Matrix4x4& vp);
    void paintFocus     (Tempest::Painter& p, Tempest::Rect rect);

    void drawBar(Tempest::Painter& p, const Tempest::Texture2d *bar, int x, int y, float v, Tempest::AlignFlag flg);
    void drawMsg(Tempest::Painter& p);
    void drawProgress(Tempest::Painter& p, int x, int y, int w, int h, float v);
    void drawLoading (Tempest::Painter& p,int x,int y,int w,int h);
    void drawSaving  (Tempest::Painter& p);
    void drawSaving  (Tempest::Painter& p, const Tempest::Texture2d& back, int w, int h, float scale);

    void startGame(std::string_view slot);
    void loadGame (std::string_view slot);
    void saveGame (std::string_view slot, std::string_view name);

    void onVideo(std::string_view fname);
    void onStartLoading();
    void onWorldLoaded();
    void onSessionExit();
    void onBenchmarkFinished();
    void setGameImpl(std::unique_ptr<GameSession>&& w);
    void clearInput();
    void setFullscreen(bool fs);

    void processMouse(Tempest::MouseEvent& event, bool enable);
    void tickMouse(uint64_t dt);
    void tickGamepad();
    GamepadBindings::Context controllerContext() const;
    void controllerAction(const GamepadBindings::Event& event);
    void controllerUiKey(Tempest::Event::KeyType key, bool repeat);
#if defined(__MOBILE_PLATFORM__)
    void onTouchCommand(TouchInput::Command command, bool pressed);
#endif
    void onSettings();

    void setupUi();

    void render() override;

    uint64_t tick();
    void     updateAnimation(uint64_t dt);
    void     tickCamera(uint64_t dt);
    void     isDialogClosed(bool& ret);

    template<Tempest::KeyEvent::KeyType k>
    void     onMarvinKey();

    Camera::Mode solveCameraMode() const;

    enum RuntimeMode : uint8_t {
      R_Normal,
      R_Suspended,
      R_Step,
      };

    Tempest::Device&      device;
    Tempest::Swapchain    swapchain;
    Tempest::TextureAtlas atlas;
    Shaders               shaders;
    Renderer              renderer;

    Tempest::VectorImage  uiLayer, numOverlay;
    Tempest::VectorImage::Mesh uiMesh [Resources::MaxFramesInFlight];
    Tempest::VectorImage::Mesh numMesh[Resources::MaxFramesInFlight];

    Tempest::Fence         fence   [Resources::MaxFramesInFlight];
    Tempest::CommandBuffer commands[Resources::MaxFramesInFlight];
    uint8_t                cmdId = 0;

    Tempest::Texture2d        background;
    const Tempest::Texture2d* loadBox=nullptr;
    const Tempest::Texture2d* loadVal=nullptr;

    const Tempest::Texture2d* barBack=nullptr;
    const Tempest::Texture2d* barHp  =nullptr;
    const Tempest::Texture2d* barMisc=nullptr;
    const Tempest::Texture2d* barMana=nullptr;

    const Tempest::Texture2d* focusImg=nullptr;

    const Tempest::Texture2d* saveback=nullptr;

    bool                      mouseP[Tempest::MouseEvent::ButtonBack]={};

    KeyCodec                  keycodec;

    MenuRoot                  rootMenu;
    VideoWidget               video;
    InventoryMenu             inventory;
    DialogMenu                dialogs;
    DocumentMenu              document;
    ChapterScreen             chapter;
    ConsoleWidget             console;
    RuntimeMode               runtimeMode = R_Normal;

    Tempest::Widget*          uiKeyUp=nullptr;
    Tempest::Point            dMouse;
    PlayerControl             player;
#if defined(__ANDROID__)
    Tempest::Timer            controllerTimer;
    GamepadBindings           controllerBindings;
    bool                      controllerConnected=false;
    bool                      controllerExploration=false;
    bool                      controllerFocused=true;
    bool                      controllerAxesBlocked=true;
    uint32_t                  controllerButtons=0;
    uint32_t                  controllerTriggers=0;
    uint32_t                  wheelHeldMask=0;
    uint64_t                  controllerLastPoll=0;
    uint64_t                  controllerLookIdle=0;
    uint64_t                  controllerLastSwitch=0;
    bool                      controllerSwitchReady=true;
#endif
#if defined(__MOBILE_PLATFORM__)
    TouchInput                mobileUi;
    uint64_t                  touchLookIdle=0;
#endif
    uint64_t                  lastTick=0;

    Tempest::Shortcut         funcKey[11];
    Tempest::Shortcut         displayPos;

    struct BenchmarkData {
      std::vector<uint64_t> low1procent;
      size_t                numFrames = 0;
      double                fpsSum = 0;
      void                  push(uint64_t t);
      void                  clear();
      };
    struct Fps {
      uint64_t dt[10]={};
      double   get() const;
      void     push(uint64_t t);
      };
    Fps           fps;
    bool          profileGpu = false;
    uint32_t      gpuProfileFrames = 0;
    uint32_t      gpuProfileAttempts = 0;
    std::ofstream gpuProfileLog;
#if defined(__ANDROID__)
    uint64_t      fpsOverlayUpdated = 0;
    bool          showFps = false;
#endif
    BenchmarkData benchmark;
    uint64_t      maxFpsInv = 0;
  };
