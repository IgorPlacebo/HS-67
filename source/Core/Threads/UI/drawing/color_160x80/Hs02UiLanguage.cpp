// Language selection for the HS-02 colour UI strings (Hs02UiStrings.hpp).
#include "Hs02UiStrings.hpp"
#include "Settings.h"
#include "Translation.h"

#ifdef LCD_160x80

namespace Hs02UiText {
bool english() {
  switch (getSettingValue(SettingsOptions::HS02UiLanguage)) {
  case 1:
    return false; // Russian
  case 2:
    return true; // English
  default: {
    // Auto: Russian for the Cyrillic builds of the firmware, English for everything else.
    const char *c = TranslationLanguageCode;
    const bool  cyrillic = (c[0] == 'R' && c[1] == 'U') || (c[0] == 'U' && c[1] == 'K') || (c[0] == 'B' && c[1] == 'E');
    return !cyrillic;
  }
  }
}
} // namespace Hs02UiText
#endif
