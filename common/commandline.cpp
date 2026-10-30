#include "commandline.h"

#include <Tempest/Log>
#include <Tempest/Platform>
#include <Tempest/TextCodec>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#if defined(__WINDOWS__)
#include <windows.h>
#endif

#if defined(__APPLE__)
#include <filesystem>
#endif

#include <algorithm>

#include "utils/installdetect.h"
#include "utils/fileutil.h"
#include "utils/string_frm.h"
#include "utils/saveslot.h"

using namespace Tempest;
using namespace FileUtil;

static CommandLine* instance = nullptr;

static const char16_t* toString(ScriptLang lang) {
  switch(lang) {
    case ScriptLang::EN: return u"Scripts_EN";
    case ScriptLang::DE: return u"Scripts_DE";
    case ScriptLang::PL: return u"Scripts_PL";
    case ScriptLang::RU: return u"Scripts_RU";
    case ScriptLang::FR: return u"Scripts_FR";
    case ScriptLang::ES: return u"Scripts_ES";
    case ScriptLang::IT: return u"Scripts_IT";
    case ScriptLang::CZ: return u"Scripts_CZ";
    case ScriptLang::NONE:
      break;
    }
  return u"Scripts";
  }

static const char* helpText =
  "Usage: Gothic2Notr [options]\n"
  "  -g <path>              path containing the Gothic game data\n"
  "  -game:<modfile.ini>    game modification manifest (GothicStarter compatibility)\n"
  "  -nomenu                skip the main menu\n"
  "  -devmode               enable marvin mode at the start of the game\n"
  "  -w <worldname.zen>     startup world; newworld.zen is the default\n"
  "  -save <number>         load save_slot_<number>.sav on start\n"
  "  -save q                load the quicksave (save_slot_0.sav) on start\n"
  "  -save q<number>        load the rotating quicksave save_quick_<number>.sav on start\n"
  "  -lastSave              load the newest save on start, or show the main menu if there is none\n"
  "  -window                windowed mode\n"
  "  -benchmark             run the benchmark\n"
  "  -v, -validation        enable validation layers for the graphics API\n"
  "  -dx12                  use DirectX 12 instead of Vulkan (Windows only)\n"
  "  -g1, -g2c, -g2         assume a Gothic 1, Gothic 2 classic or Gothic 2 Night of the Raven installation\n"
  "  -rt <boolean>          enable or disable ray queries\n"
  "  -gi <number>           ray-traced global illumination method, 0 is off\n"
  "  -ms <boolean>          enable or disable meshlets\n"
  "  -aa <number>           anti-aliasing: 0 off, 1 medium, 2 ultra; overrides the game setting\n"
  "  -help, -h, -?          show this help\n";

static void printHelp() {
#if defined(__WINDOWS__)
  // The game is a GUI program: write to a redirected output, else to the console that started it
  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  if((out==nullptr || out==INVALID_HANDLE_VALUE) && AttachConsole(ATTACH_PARENT_PROCESS))
    out = CreateFileA("CONOUT$",GENERIC_WRITE,FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
  if(out==nullptr || out==INVALID_HANDLE_VALUE)
    return;
  DWORD written = 0;
  WriteFile(out,helpText,DWORD(std::strlen(helpText)),&written,nullptr);
#else
  std::fputs(helpText,stdout);
  std::fflush(stdout);
#endif
  }

static bool boolArg(std::string_view v) {
  return std::string_view(v)!="0" && std::string_view(v)!="false";
  }

static int intArg(std::string_view v) {
  if(v=="false")
    return 0;
  return (std::stoi(std::string(v)));
  }

CommandLine::CommandLine(int argc, const char** argv) {
  instance = this;
  if(argc<1)
    return;

  std::string_view mod;
  for(int i=1;i<argc;++i) {
    std::string_view arg = argv[i];
    if(arg=="-help" || arg=="-h" || arg=="-?" || arg=="--help") {
      printHelp();
      std::exit(0);
      }
    else if(arg.find("-game:")==0) {
      if(!mod.empty())
        Log::e("-game specified twice");
      mod = arg.substr(6);
      }
    else if(arg=="-g") {
      ++i;
      if(i<argc)
        gpath.assign(argv[i],argv[i]+std::strlen(argv[i]));
      }
    else if(arg=="-devmode") {
      // http://www.gothic-library.ru/publ/marvin/1-1-0-547
      devmode = true;
      }
    else if(arg=="-save") {
      ++i;
      if(i<argc){
        if(std::strcmp(argv[i],"q")==0) {
          saveDef = "save_slot_0.sav";
          }
        else if(argv[i][0]=='q') {
          // Rotating quicksaves: -save q3 is save_quick_3.sav
          saveDef = string_frm("save_quick_",argv[i]+1,".sav");
          } else {
          saveDef = string_frm("save_slot_",argv[i],".sav");
          }
        }
      }
    else if(arg=="-lastSave" || arg=="-lastsave") {
      // Without any save the game starts at the main menu as usual
      const auto file = SaveSlot::newest(".");
      if(!file.empty())
        saveDef = file.filename().string();
      }
    else if(arg=="-w") {
      ++i;
      if(i<argc)
        wrldDef = argv[i];
      }
    else if(arg=="-window") {
      isWindow = true;
      }
    else if(arg=="-nomenu") {
      noMenu = true;
      }
    else if(arg=="-benchmark") {
      isBenchmark = Benchmark::Normal;
      if(i+1<argc && argv[i+1][0]!='-') {
        ++i;
        isBenchmark = std::string_view(argv[i])=="ci" ? Benchmark::CiTooling : isBenchmark;
        }
      }
    else if(arg=="-g1") {
      forceG1 = true;
      }
    else if(arg=="-g2c") {
      forceG2 = true;
      }
    else if(arg=="-g2") {
      forceG2NR = true;
      }
    else if(arg=="-dx12") {
      graphics = GraphicBackend::DirectX12;
      }
    else if(arg=="-validation" || arg=="-v") {
      isDebug  = true;
      }
    else if(arg=="-rt") {
      ++i;
      if(i<argc)
        isRQuery = boolArg(argv[i]);
      }
    else if(arg=="-aa") {
      ++i;
      if(i<argc) {
        try {
          const int arg = std::max(0, intArg(argv[i]));
          aaPresetId = std::clamp(arg, 0, int32_t(AaPreset::PRESETS_COUNT)-1);
          }
        catch (const std::exception& e) {
          Log::i("failed to read cmaa2 preset: \"", std::string(argv[i]), "\"");
          }
        }
      }
    else if(arg=="-gi") {
      ++i;
      if(i<argc) {
        const int arg = std::max(0, intArg(argv[i]));
        isGi = GiMethod(std::clamp(arg, 0, GiMethod::Count-1));
        }
      }
    else if(arg=="-ms") {
      ++i;
      if(i<argc)
        isMeshSh = boolArg(argv[i]);
      }
    else if(arg=="-bl") {
      // not to document - debug only
      ++i;
      if(i<argc)
        isBindlessSh = boolArg(argv[i]);
      }
    else if(arg=="-vsm") {
      // not to document - debug only
      ++i;
      if(i<argc)
        isVsm = boolArg(argv[i]);
      }
    else if(arg=="-rtsm") {
      // not to document - debug only
      ++i;
      if(i<argc)
        isRtSm = boolArg(argv[i]);
      }
    else {
      Log::i("unrecognized commandline option: \"", arg, "\"");
      }
    }

  if(gpath.empty()) {
    InstallDetect inst;
    gpath = inst.detectG2();
#if defined(__APPLE__)
    if(!gpath.empty() && gpath==inst.applicationSupportDirectory()) {
      std::filesystem::current_path(gpath);
      }
#endif
    }

  for(auto& i:gpath)
    if(i=='\\')
      i='/';

  if(gpath.size()>0 && gpath.back()!='/')
    gpath.push_back('/');

  gscript   = nestedPath({u"_work",u"Data",u"Scripts",   u"_compiled"},Dir::FT_Dir);
  gcutscene = nestedPath({u"_work",u"Data",u"Scripts",   u"content",u"CUTSCENE"},Dir::FT_Dir);

#if defined(__ANDROID__)
  // Archolos ships its starting world in a separate launcher INI and its scripts in .mod archives.
  if(mod.empty() && FileUtil::exists(nestedPath({u"system",u"TheChroniclesOfMyrtana.ini"},Dir::FT_File))) {
    mod = "TheChroniclesOfMyrtana.ini";
    if(!forceG1 && !forceG2)
      forceG2NR = true;
    }
#endif

  gmod    = TextCodec::toUtf16(mod);
  if(!gmod.empty())
    gmod = nestedPath({u"system",gmod.c_str()},Dir::FT_File);

  if(!validateGothicPath()) {
    if(gpath.empty()) {
      Log::e("Gothic path is not provided. Please use command line argument -g <path>");
      } else {
      Log::e("Invalid gothic path: \"",TextCodec::toUtf8(gpath),"\"");
      }
    throw GothicNotFoundException("gothic not found!"); // TODO: user-friendly message-box
    }
  }

const CommandLine& CommandLine::inst() {
  assert(instance!=nullptr);
  return *instance;
  }

CommandLine::GraphicBackend CommandLine::graphicsApi() const {
  return graphics;
  }

std::u16string_view CommandLine::rootPath() const {
  return gpath;
  }

std::u16string CommandLine::scriptPath() const {
  return gscript;
  }

std::u16string CommandLine::scriptPath(ScriptLang lang) const {
  const char16_t* scripts = toString(lang);
  return nestedPath({u"_work",u"Data",scripts,u"_compiled"},Dir::FT_Dir);
  }

std::u16string CommandLine::cutscenePath() const {
  return gcutscene;
  }

std::u16string CommandLine::cutscenePath(ScriptLang lang) const {
  const char16_t* scripts = toString(lang);
  return nestedPath({u"_work",u"Data",scripts},Dir::FT_Dir);
  }

std::u16string CommandLine::nestedPath(const std::initializer_list<const char16_t*>& name, Tempest::Dir::FileType type) const {
  return FileUtil::nestedPath(gpath, name, type);
  }

bool CommandLine::validateGothicPath() const {
  if(gpath.empty()) {
    Log::e("Gothic path is empty");
    return false;
    }
  if(!FileUtil::exists(gscript)) {
    Log::e("Missing scripts directory: \"",TextCodec::toUtf8(gscript),"\"");
    return false;
    }
  auto dataPath = nestedPath({u"Data"},Dir::FT_Dir);
  if(!FileUtil::exists(dataPath)) {
    Log::e("Missing Data directory: \"",TextCodec::toUtf8(dataPath),"\"");
    return false;
    }
  auto workDataPath = nestedPath({u"_work",u"Data"},Dir::FT_Dir);
  if(!FileUtil::exists(workDataPath)) {
    Log::e("Missing _work/Data directory: \"",TextCodec::toUtf8(workDataPath),"\"");
    return false;
    }
  return true;
  }
