#pragma once

#include <string_view>

namespace zenkit { class DaedalusScript; }

// Texts OpenGothic adds to the game's own UI (touch buttons, radial wheels, mobile settings), in the game's language.
// Gothic fonts are single-byte code pages, so the language follows the installed game rather than the phone.
namespace UiText {
  enum class Lang { EN, DE, PL, RU };

  // Reads the game's language from the strings of a script, such as MENU.DAT
  void detect(const zenkit::DaedalusScript& script);
  Lang language();

  // The translation of an English text in the game's code page, or the text itself
  std::string_view tr(std::string_view en);
  }
