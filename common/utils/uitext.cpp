#include "uitext.h"

#include <Tempest/Log>
#include <zenkit/DaedalusScript.hh>

#include <string>
#include <unordered_map>

using namespace Tempest;

namespace {

struct Entry {
  const char* en;
  const char* de;
  const char* pl;
  const char* ru;
  };

// UTF-8 here; converted to the game's code page once the language is known
const Entry entries[] = {
  {"Menu",            "Menü",              "Menu",                   "Меню"},
  {"Back",            "Zurück",            "Wstecz",                 "Назад"},
  {"Hold: System",    "Halten: System",    "Przytrzymaj: system",    "Удерж.: система"},
  {"Inventory",       "Inventar",          "Ekwipunek",              "Инвентарь"},
  {"Hold: Character", "Halten: Charakter", "Przytrzymaj: postać",    "Удерж.: персонаж"},
  {"Jump",            "Springen",          "Skok",                   "Прыжок"},
  {"Sheathe",         "Wegstecken",        "Schowaj",                "Убрать"},
  {"Draw",            "Ziehen",            "Dobądź",                 "Достать"},
  {"Hold: Equipment", "Halten: Ausrüstung","Przytrzymaj: ekwipunek", "Удерж.: снаряжение"},
  {"Block",           "Blocken",           "Blok",                   "Блок"},
  {"Accept",          "Annehmen",          "Akceptuj",               "Принять"},
  {"Action",          "Aktion",            "Akcja",                  "Действие"},
  {"Attack",          "Angriff",           "Atak",                   "Атака"},
  {"Use",             "Benutzen",          "Użyj",                   "Использовать"},

  {"FPS: On",                 "FPS: an",                 "FPS: wł.",                "FPS: вкл"},
  {"FPS: Off",                "FPS: aus",                "FPS: wył.",               "FPS: выкл"},
  {"Touch labels\nOn",        "Beschriftung\nan",        "Podpisy\nwł.",            "Подписи\nвкл"},
  {"Touch labels\nOff",       "Beschriftung\naus",       "Podpisy\nwył.",           "Подписи\nвыкл"},
  {"Gamepad controls\nOn",    "Gamepad-Hilfe\nan",       "Pomoc pada\nwł.",         "Схема геймпада\nвкл"},
  {"Gamepad controls\nOff",   "Gamepad-Hilfe\naus",      "Pomoc pada\nwył.",        "Схема геймпада\nвыкл"},
  {"HUD\nCentered",           "HUD\nzentriert",          "HUD\nna środku",          "Интерфейс\nпо центру"},
  {"HUD\nClassic",            "HUD\nklassisch",          "HUD\nklasyczny",          "Интерфейс\nклассика"},
  {"Combat\nClassic",         "Kampf\nklassisch",        "Walka\nklasyczna",        "Бой\nклассика"},
  {"Combat\nModern",          "Kampf\nmodern",           "Walka\nnowoczesna",       "Бой\nсовременный"},
  {"Vibration\nOn",           "Vibration\nan",           "Wibracje\nwł.",           "Вибрация\nвкл"},
  {"Vibration\nOff",          "Vibration\naus",          "Wibracje\nwył.",          "Вибрация\nвыкл"},
  {"Stats",                   "Werte",                   "Statystyki",              "Атрибуты"},
  {"Journal",                 "Tagebuch",                "Dziennik",                "Дневник"},
  {"Selected",                "Gewählt",                 "Wybrano",                 "Выбрано"},
  {"Cancel",                  "Abbrechen",               "Anuluj",                  "Отмена"},
  {"Equipment",               "Ausrüstung",              "Ekwipunek",               "Снаряжение"},
  {"System",                  "System",                  "System",                  "Система"},
  {"Character",               "Charakter",               "Postać",                  "Персонаж"},
  {"No usable items",         "Keine nutzbaren Gegenstände", "Brak przedmiotów do użycia", "Нет подходящих предметов"},
  {" (Equipped)",             " (angelegt)",             " (założone)",             " (надето)"},
  {"Hold < / > to change page", "< / > halten zum Blättern", "Przytrzymaj < / >, by zmienić stronę", "Удерживайте < / > для смены страницы"},
  {"Added to the equipment wheel",     "Zum Ausrüstungsrad hinzugefügt", "Dodano do koła ekwipunku",  "Добавлено в колесо снаряжения"},
  {"Removed from the equipment wheel", "Vom Ausrüstungsrad entfernt",    "Usunięto z koła ekwipunku", "Убрано из колеса снаряжения"},
  {"Weapons",                 "Waffen",                  "Broń",                    "Оружие"},
  {"Magic",                   "Magie",                   "Magia",                   "Магия"},
  {"Potions",                 "Tränke",                  "Mikstury",                "Зелья"},
  {"Food",                    "Nahrung",                 "Jedzenie",                "Еда"},
  {"Maps and documents",      "Karten und Dokumente",    "Mapy i dokumenty",        "Карты и документы"},

  {"Resolution",              "Auflösung",               "Rozdzielczość",           "Разрешение"},
  {"Frame rate",              "Bildrate",                "Liczba klatek",           "Частота кадров"},
  {"30 FPS|60 FPS|Unlocked",  "30 FPS|60 FPS|Unbegrenzt","30 FPS|60 FPS|Bez limitu","30 FPS|60 FPS|Без лимита"},
  {"Off|On",                  "Aus|An",                  "Wył.|Wł.",                "Выкл|Вкл"},
  {"Shadows",                 "Schatten",                "Cienie",                  "Тени"},
  {"Brightness",              "Helligkeit",              "Jasność",                 "Яркость"},
  {"Contrast",                "Kontrast",                "Kontrast",                "Контраст"},
  {"Gamma",                   "Gamma",                   "Gamma",                   "Гамма"},
  };

UiText::Lang                                 current = UiText::Lang::EN;
std::unordered_map<std::string_view,std::string> translated;

char32_t nextCodepoint(std::string_view s, size_t& i) {
  const auto c = uint8_t(s[i++]);
  if(c<0x80)
    return c;
  const int extra = c>=0xF0 ? 3 : (c>=0xE0 ? 2 : 1);
  char32_t cp = c & (0x3F>>extra);
  for(int k=0; k<extra && i<s.size(); ++k)
    cp = (cp<<6) | (uint8_t(s[i++]) & 0x3F);
  return cp;
  }

// One byte of the code page the game's fonts are drawn in
char toCodePage(char32_t cp, UiText::Lang lang) {
  if(cp<0x80)
    return char(cp);
  switch(lang) {
    case UiText::Lang::RU: // windows-1251
      if(cp>=0x410 && cp<=0x44F) return char(0xC0 + (cp-0x410));
      if(cp==0x401) return char(0xA8);
      if(cp==0x451) return char(0xB8);
      break;
    case UiText::Lang::PL: { // windows-1250
      static const std::pair<char32_t,uint8_t> pl[] = {
        {0x105,0xB9},{0x104,0xA5},{0x107,0xE6},{0x106,0xC6},{0x119,0xEA},{0x118,0xCA},{0x142,0xB3},{0x141,0xA3},
        {0x144,0xF1},{0x143,0xD1},{0xF3,0xF3},{0xD3,0xD3},{0x15B,0x9C},{0x15A,0x8C},{0x17A,0x9F},{0x179,0x8F},
        {0x17C,0xBF},{0x17B,0xAF},
        };
      for(auto& [u,b]:pl)
        if(u==cp) return char(b);
      break;
      }
    case UiText::Lang::DE: // windows-1252 matches Latin-1 for these
      if(cp>=0xA0 && cp<=0xFF) return char(cp);
      break;
    case UiText::Lang::EN:
      break;
    }
  return '?';
  }

std::string encode(std::string_view utf8, UiText::Lang lang) {
  std::string ret;
  for(size_t i=0; i<utf8.size();)
    ret.push_back(toCodePage(nextCodepoint(utf8,i),lang));
  return ret;
  }
}

void UiText::detect(const zenkit::DaedalusScript& script) {
  // Letters only these languages use, as their code page stores them
  size_t letters = 0, cyrillic = 0, polish = 0, german = 0;
  for(auto& sym:script.symbols()) {
    if(sym.type()!=zenkit::DaedalusDataType::STRING || !sym.is_const())
      continue;
    for(uint16_t i=0; i<sym.count(); ++i) {
      for(const char ch:sym.get_string(i)) {
        const auto c = uint8_t(ch);
        if((c>='A' && c<='Z') || (c>='a' && c<='z') || c>=0x80)
          ++letters;
        if(c>=0xC0 || c==0xA8 || c==0xB8)
          ++cyrillic;
        if(c==0xB9 || c==0xA5 || c==0xEA || c==0xCA || c==0xB3 || c==0xA3 || c==0x9C || c==0x8C || c==0xBF || c==0xAF || c==0x9F || c==0x8F)
          ++polish;
        if(c==0xE4 || c==0xFC || c==0xDF || c==0xC4 || c==0xDC)
          ++german;
        }
      }
    }

  Lang lang = Lang::EN;
  if(letters>0 && cyrillic*4>letters)
    lang = Lang::RU;
  else if(polish>2 && polish>german)
    lang = Lang::PL;
  else if(german>2)
    lang = Lang::DE;

  if(lang==current && !translated.empty())
    return;
  current = lang;
  translated.clear();
  if(lang!=Lang::EN) {
    for(auto& e:entries) {
      const char* t = lang==Lang::RU ? e.ru : (lang==Lang::PL ? e.pl : e.de);
      translated[e.en] = encode(t,lang);
      }
    }
  static const char* names[] = {"English","German","Polish","Russian"};
  Log::i("Interface language: ", names[int(lang)]);
  }

UiText::Lang UiText::language() {
  return current;
  }

std::string_view UiText::tr(std::string_view en) {
  auto it = translated.find(en);
  return it!=translated.end() ? std::string_view(it->second) : en;
  }
