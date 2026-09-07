#pragma once

#include <Tempest/Widget>

#include <functional>
#include <unordered_map>

namespace Tempest { class Painter; }

class TouchInput : public Tempest::Widget {
  public:
    enum class Command : uint8_t {
      Up,
      Down,
      Left,
      Right,
      Accept,
      Back,
      Jump,
      Weapon,
      Inventory,
      LockTarget,
      TapAccept,
      Block,
      };

    using CommandHandler = std::function<void(Command,bool)>;

    explicit TouchInput(CommandHandler command);

    void            paintEvent(Tempest::PaintEvent& e) override;
    void            mouseDownEvent(Tempest::MouseEvent& e) override;
    void            mouseDragEvent(Tempest::MouseEvent& e) override;
    void            mouseUpEvent(Tempest::MouseEvent& e) override;

    void            setTouchEnabled(bool enabled);
    void            setAnalogMovement(bool enabled);
    void            setDebugOverlay(bool enabled);
    void            setDebugContext(bool classicCombat, bool uiActive, bool canLock, bool locked, bool canBlock);
    void            tick();
    Tempest::PointF movementAxis() const;
    Tempest::Point  takeLookDelta();
    bool            isLooking() const;

  private:
    enum class Role : uint8_t {
      Move,
      Look,
      Button,
      };

    struct Touch {
      Role           role = Role::Look;
      Tempest::Point anchor;
      Tempest::Point last;
      Command        command = Command::Accept;
      uint64_t       pressedAt = 0;
      bool           pendingAction = false;
      bool           actionSent = false;
      };

    void updateMovement(const Tempest::Point& pos);
    void setDirection(Command command, bool pressed);
    void reset();
    int  movementRadius() const;
    Tempest::Rect buttonRect(size_t index) const;
    bool blockVisible() const;
    Tempest::Rect blockRect() const;
    void drawBlock(Tempest::Painter& p) const;

    static constexpr int LookBoundaryPercent = 84;
    static constexpr int ActionBoundaryPercent = 76;
    static constexpr int ButtonEdgesPercent[] = {0,20,40,55,70,100};
    static constexpr Command Buttons[] = {Command::Back,Command::Inventory,Command::Jump,Command::Weapon,Command::Accept};
    static constexpr float DirectionThreshold = 0.35f;
    static constexpr uint64_t ActionHoldMs = 180;

    CommandHandler command;
    std::unordered_map<int,Touch> touches;
    Tempest::PointF moveAxis;
    Tempest::Point  lookDelta;
    int             movePointer = -1;
    int             lookPointer = -1;
    int             blockPointer = -1;
    bool            touchEnabled = true;
    bool            analogMovement = false;
    bool            debugOverlay = false;
    bool            classicCombat = true;
    bool            uiActive = false;
    bool            canLock = false;
    bool            targetLocked = false;
    bool            canBlock = false;
    bool            directions[4] = {};
  };

