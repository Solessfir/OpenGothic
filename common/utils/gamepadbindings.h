#pragma once

#include <array>
#include <cstdint>
#include <istream>
#include <string>
#include <string_view>
#include <vector>

// Game-specific bindings are independent of the platform input backend.
class GamepadBindings final {
  public:
    enum class Context { Gameplay, ClassicMelee, ModernMelee, Ranged, UI, Inventory, EquipmentWheel, Interaction };
    enum class Action {
      Interact, Back, Jump, Journal, Inventory, Pause, Sneak, Walk, LockTarget,
      DrawSheathe, EquipmentWheel, Map, HealthPotion, ManaPotion, CharacterStats,
      FirstPerson, LookBehind, QuickSave, QuickLoad, AttackForward, AttackLeft,
      AttackRight, Block, Finish, Accept, Up, Down, Left, Right,
      PreviousPage, NextPage, Cancel, LeftPanel, RightPanel, TakeStack, Drop,
      Spell3, Spell4, Spell5, Spell6, Spell7, Spell8, Spell9, Spell10,
      Count
      };
    enum class Phase { Press, Release, Repeat, Cancel };
    struct Event { Action action; Phase phase; uint32_t mask; };
    struct Options {
      bool enabled = true;
      uint32_t explorationModifier = 1u<<14;
      float deadZone = 0.20f;
      float walkThreshold = 0.65f;
      float triggerPress = 0.55f;
      float triggerRelease = 0.40f;
      bool swapMovement = false;
      bool swapCamera = false;
      bool swapWheel = false;
      uint64_t holdMs = 400;
      uint64_t repeatDelayMs = 350;
      uint64_t repeatMs = 150;
      float switchThreshold = 0.65f;
      float switchReset = 0.25f;
      uint64_t switchCooldownMs = 250;
      float cameraSmoothing = 0.20f;
      bool cameraAssist = true;
      } options;

    GamepadBindings();
    std::vector<std::string> load(std::istream& input);
    static std::string defaults();
    static uint32_t button(std::string_view name);
    std::string hint(Action action, Context context) const;
    std::vector<Event> update(uint32_t buttons, Context context, uint64_t now);
    void reset(uint32_t held = 0);

  private:
    struct Binding {
      Action action = Action::Count;
      uint32_t mask = 0;
      uint32_t trigger = 0;
      bool hold = false;
      std::string text;
      };
    struct Section { std::string name; std::vector<Binding> bindings; };
    struct Press {
      Binding binding;
      Binding hold;
      uint64_t started = 0;
      uint64_t repeat = 0;
      bool active = false;
      bool pending = false;
      };
    std::vector<Binding> bindings(Context context) const;
    std::vector<Section> sections;
    std::array<Press,24> presses;
    uint32_t previous = 0;
    uint32_t blocked = 0;
    Context lastContext = Context::Gameplay;
    bool initialized = false;
  };
