
#include "hacks/layout_mode.hpp"
#include "hacks/show_trajectory.hpp"
#include "includes.hpp"
#include "ui/clickbot_layer.hpp"
#include "ui/game_ui.hpp"
#include "ui/macro_editor.hpp"
#include "ui/record_layer.hpp"
#include "ui/render_settings_layer.hpp"

#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/CCTouchDispatcher.hpp>

// NOTE: geode.custom-keybinds is incompatible with Geode v5
// (EventFilter/EventListener removed).
// TODO: Migrate to Geode v5 built-in KeybindSettingV3 system.
// #ifdef GEODE_IS_WINDOWS
// #include <geode.custom-keybinds/include/Keybinds.hpp>
// #include <regex>
// #endif

const std::vector<std::string> keybindIDs = {
    "open_menu",        "toggle_recording",     "toggle_playing",
    "toggle_speedhack", "toggle_frame_stepper", "step_frame",
    "toggle_render",    "toggle_noclip",        "show_trajectory"};

class $modify(CCKeyboardDispatcher) {
  bool dispatchKeyboardMSG(enumKeyCodes key, bool isKeyDown, bool isKeyRepeat,
                           double p3) {

    auto &g = Global::get();

    int keyInt = static_cast<int>(key);
    if (g.allKeybinds.contains(keyInt) && !isKeyRepeat) {
      for (size_t i = 0; i < 6; i++) {
        if (std::find(g.keybinds[i].begin(), g.keybinds[i].end(), keyInt) !=
            g.keybinds[i].end())
          g.heldButtons[i] = isKeyDown;
      }
    }

    // if (key == enumKeyCodes::KEY_L && !isKeyRepeat && isKeyDown) {
    // }

    // if (key == enumKeyCodes::KEY_F && !isKeyRepeat && isKeyDown &&
    // PlayLayer::get()) {
    //   log::debug("POS DEBUG {}", PlayLayer::get()->m_player1->getPosition());
    //   log::debug("POS2 DEBUG {}",
    //   PlayLayer::get()->m_player2->getPosition());
    // }

    // if (key == enumKeyCodes::KEY_J && !isKeyRepeat && isKeyDown &&
    // PlayLayer::get()) {
    //   std::string str =
    //   ZipUtils::decompressString(PlayLayer::get()->m_level->m_levelString.c_str(),
    //   true, 0); log::debug("{}", str);
    // }

    // NakoMod: Swift Click handling
    if (g.swiftClickEnabled && !isKeyRepeat && isKeyDown &&
        keyInt == g.swiftClickKey && PlayLayer::get() &&
        (g.state == state::recording || g.state == state::none)) {
      PlayLayer *pl = PlayLayer::get();
      if (pl && !pl->m_isPaused) {
        for (int i = 0; i < g.swiftClickCount; i++) {
          pl->handleButton(true, 1, false);
          pl->handleButton(false, 1, false);
        }
      }
    }

    if (key == enumKeyCodes::KEY_H && !isKeyRepeat) {
      g.holdingStepForward = isKeyDown;
    }

    return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown,
                                                     isKeyRepeat, p3);
  }
};

namespace {

bool shouldHandleDllBotKeybind(bool down, bool repeat) {
  if (!down || repeat)
    return false;

  return true;
}

void handleOpenMenuKeybind(Keybind const &, bool down, bool repeat, double) {
  if (!shouldHandleDllBotKeybind(down, repeat))
    return;

  auto &g = Global::get();
  if (g.layer) {
    static_cast<RecordLayer *>(g.layer)->onClose(nullptr);
    return;
  }

  RecordLayer::openMenu();
}

void handleToggleMacroKeybind(Keybind const &, bool down, bool repeat, double) {
  if (!shouldHandleDllBotKeybind(down, repeat))
    return;

  Macro::togglePlaying();
}

void handleStepForward(Keybind const &, bool down, bool repeat, double) {
  if (!shouldHandleDllBotKeybind(down, repeat))
    return;

  Global::frameStep(1);
}

void handleHoldForward(Keybind const &, bool down, bool repeat, double) {
  auto &g = Global::get();
  g.holdingStepForward = down;
}

void handleToggleStepper(Keybind const &, bool down, bool repeat, double) {
  if (!shouldHandleDllBotKeybind(down, repeat))
    return;

  Global::toggleFrameStepper();
}

} // namespace

$on_mod(Loaded) {
  geode::listenForKeybindSettingPresses(
      "keybind_open_menu",
      +[](Keybind const &keybind, bool down, bool repeat, double timestamp) {
        handleOpenMenuKeybind(keybind, down, repeat, timestamp);
      });

  geode::listenForKeybindSettingPresses(
      "keybind_toggle_macro",
      +[](Keybind const &keybind, bool down, bool repeat, double timestamp) {
        handleToggleMacroKeybind(keybind, down, repeat, timestamp);
      });

  geode::listenForKeybindSettingPresses(
      "keybind_step_forward",
      +[](Keybind const &keybind, bool down, bool repeat, double timestamp) {
        handleStepForward(keybind, down, repeat, timestamp);
      });

  geode::listenForKeybindSettingPresses(
      "keybind_toggle_stepper",
      +[](Keybind const &keybind, bool down, bool repeat, double timestamp) {
        handleToggleStepper(keybind, down, repeat, timestamp);
      });
}
