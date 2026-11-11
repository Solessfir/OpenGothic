#include "musicdefinitions.h"

#include <Tempest/Log>

#include "gothic.h"
#include "resources.h"
#include <algorithm>
#include <cctype>

using namespace Tempest;

MusicDefinitions::MusicDefinitions() {
  vm = Gothic::inst().createPhoenixVm("Music.dat");

  vm->enumerate_instances_by_class_name("C_MusicTheme", [this](zenkit::DaedalusSymbol& s) {
    themes.push_back(vm->init_instance<zenkit::IMusicTheme>(&s));
    });
  try {
    loadRecordedMusic();
    }
  catch(const std::exception& e) {
    recorded.clear();
    recordedIds.clear();
    recordedThemes.clear();
    Log::e("CoM music definitions: ",e.what());
    }
  }

MusicDefinitions::~MusicDefinitions() {
  }

const zenkit::IMusicTheme* MusicDefinitions::operator [](std::string_view name) const {
  std::string key(name);
  for(auto& c:key)
    c = char(std::toupper(static_cast<unsigned char>(c)));
  if(auto it = recordedThemes.find(key); it!=recordedThemes.end())
    return &it->second;
  if(!vm)
    return nullptr;

  auto id = vm->find_symbol_by_name(name);
  if(id==nullptr)
    return nullptr;
  for(auto& i:themes) {
    auto* sym = vm->find_symbol_by_instance(i);
    if(sym->index() == id->index())
      return i.get();
    }
  return nullptr;
  }

const MusicDefinitions::RecordedTrack* MusicDefinitions::recordedTrack(std::string_view file) const {
  auto it = recorded.find(std::string(file));
  return it!=recorded.end() ? &it->second : nullptr;
  }

const MusicDefinitions::RecordedTrack* MusicDefinitions::recordedTrack(uint32_t instance) const {
  auto it = recordedIds.find(instance);
  return it!=recordedIds.end() ? recordedTrack(it->second) : nullptr;
  }

void MusicDefinitions::loadRecordedMusic() {
  // CoM's KmLib reads MUSICTRACK/MUSICZONE from GOTHIC.DAT, not MUSIC.DAT.
  // A temporary VM initializes just these data instances; no mod startup runs.
  if(!Resources::hasFile("02.OGG"))
    return;
  auto scripts = Gothic::inst().createPhoenixVm(Gothic::inst().defaultGameDatFile());
  if(scripts->find_symbol_by_name("MUSICTRACK")==nullptr || scripts->find_symbol_by_name("MUSICZONE")==nullptr)
    return;
  struct Track : zenkit::DaedalusInstance {
    std::string file;
    int32_t loop=0, fadeOut=0, fadeIn=0;
    };
  struct Zone : zenkit::DaedalusInstance {
    int32_t day=0, night=0, dayFight=0, nightFight=0;
    };
  scripts->register_member("MUSICTRACK.FILENAME",&Track::file);
  scripts->register_member("MUSICTRACK.LOOPOFFSETDURATION",&Track::loop);
  scripts->register_member("MUSICTRACK.FADEOUTSLIDEDURATION",&Track::fadeOut);
  scripts->register_member("MUSICTRACK.FADEINSLIDEDURATION",&Track::fadeIn);
  scripts->register_member("MUSICZONE.DAY",&Zone::day);
  scripts->register_member("MUSICZONE.NIGHT",&Zone::night);
  scripts->register_member("MUSICZONE.DAYFIGHT",&Zone::dayFight);
  scripts->register_member("MUSICZONE.NIGHTFIGHT",&Zone::nightFight);
  scripts->enumerate_instances_by_class_name("MUSICTRACK",[&](zenkit::DaedalusSymbol& sym) {
    auto t = scripts->init_instance<Track>(&sym);
    if(t->file.empty())
      return;
    RecordedTrack track;
    track.theme.file = "_WORK/DATA/MUSIC/KM/"+t->file+".OGG";
    track.theme.vol = 1.f;
    track.theme.loop = 1;
    track.loopStart = std::max(t->loop,0);
    track.fadeOut = std::max(t->fadeOut,0);
    track.fadeIn = std::max(t->fadeIn,0);
    recordedIds[sym.index()] = track.theme.file;
    recorded[track.theme.file] = std::move(track);
    });
  scripts->enumerate_instances_by_class_name("MUSICZONE",[&](zenkit::DaedalusSymbol& sym) {
    auto z = scripts->init_instance<Zone>(&sym);
    const int32_t ids[] = {z->day,z->night,z->dayFight,z->nightFight};
    const char* suffix[] = {"_DAY_STD","_NGT_STD","_DAY_FGT","_NGT_FGT"};
    for(size_t i=0; i<4; ++i)
      if(const auto* track = recordedTrack(uint32_t(ids[i])))
        recordedThemes[sym.name()+suffix[i]] = track->theme;
    });
  RecordedTrack menu;
  menu.theme.file = "_WORK/DATA/MUSIC/KM/02.OGG";
  menu.theme.vol = 1.f;
  menu.theme.loop = 1;
  menu.fadeIn = 1000;
  menu.fadeOut = 2000;
  recordedThemes["SYS_MENU"] = menu.theme;
  recorded[menu.theme.file] = std::move(menu);
  Log::i("CoM music: ",recorded.size()," OGG tracks, ",recordedThemes.size()," zone/menu themes");
  }
