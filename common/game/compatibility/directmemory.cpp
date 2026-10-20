#include "directmemory.h"
#include "hookaddresses.h"
#include "logbudget.h"

#include <zenkit/DaedalusScript.hh>
#include <Tempest/Log>

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>

#include <cctype>
#include <zenkit/addon/daedalus.hh>
#include "world/objects/npc.h"
#include "world/triggers/movetrigger.h"
#include "world/world.h"
#include "gothic.h"

using namespace Tempest;
using namespace Compatibility;

static uint32_t nextPot(uint32_t x) {
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x++;
  return x;
  }

static int32_t floatBitsToInt(float f) {
  int32_t i = 0;
  std::memcpy(&i, &f, sizeof(i));
  return i;
  }

static float intBitsToFloat(int32_t i) {
  float f = 0;
  std::memcpy(&f, &i, sizeof(f));
  return f;
  }

/// The size of an element of an array member in the memory of the original engine
static Mem32::ptr32_t memberStride(const zenkit::DaedalusSymbol& sym) {
  return sym.type()==zenkit::DaedalusDataType::STRING ? 20 : 4;
  }

void DirectMemory::memory_instance::set_int(const zenkit::DaedalusSymbol& sym, uint16_t index, int32_t value) {
  if(sym.name()=="ZCARRAY.NUMINARRAY" && value>1024)
    Log::d("");
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index)*memberStride(sym);
  owner.mem32.writeInt(addr, value);
  }

int32_t DirectMemory::memory_instance::get_int(const zenkit::DaedalusSymbol& sym, uint16_t index) const {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index) * memberStride(sym);
  int32_t v = owner.mem32.readInt(addr);
  static int traced = 0;
  if(traced<12 && sym.name().compare(0,9,"C_RECIPE.")==0) {
    ++traced;
    Log::i("DMA: read ", sym.name(), "[", index, "] at ", reinterpret_cast<void*>(uint64_t(addr)), " = ", v);
    }
  return v;
  }

void DirectMemory::memory_instance::set_float(const zenkit::DaedalusSymbol& sym, uint16_t index, float value) {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index)*memberStride(sym);
  owner.mem32.writeInt(addr, *reinterpret_cast<const int32_t*>(&value));
  }

float DirectMemory::memory_instance::get_float(const zenkit::DaedalusSymbol& sym, uint16_t index) const {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index) * memberStride(sym);
  int32_t v = owner.mem32.readInt(addr);

  float f = 0;
  std::memcpy(&f, &v, 4);
  return f;
  }

void DirectMemory::memory_instance::set_string(const zenkit::DaedalusSymbol& sym, uint16_t index, std::string_view value) {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index)*memberStride(sym);
  if(auto* s = owner.mem32.deref<zString>(addr))
    owner.memAssignString(*s, value);
  }

const std::string& DirectMemory::memory_instance::get_string(const zenkit::DaedalusSymbol& sym, uint16_t index) const {
  ptr32_t addr = address + ptr32_t(sym.offset_as_member()) + ptr32_t(index)*memberStride(sym);

  if(auto zs = owner.mem32.deref<const zString>(addr)) {
    static std::string ret;
    owner.memFromString(ret, *zs);
    return ret;
    }

  if(logBudget("get_string"))
    Log::d("TODO: memory_instance::get_string ", sym.name());
  (void)addr;
  static std::string empty;
  return empty;
  }


DirectMemory::DirectMemory(GameScript& owner, zenkit::DaedalusVm& vm) : gameScript(owner), vm(vm), cpu(*this, mem32), handles(mem32) {
  if(auto v = vm.find_symbol_by_name("Ikarus_Version")) {
    const int version = v->type()==zenkit::DaedalusDataType::INT ? v->get_int() : 0;
    Log::i("DMA mod detected: Ikarus v", version);
    }
  if(auto v = vm.find_symbol_by_name("LeGo_Version")) {
    const char* version = v->type()==zenkit::DaedalusDataType::STRING ? v->get_string().c_str() : "LeGo";
    Log::i("DMA mod detected: ", version);
    }

  setupFunctionTable();
  setupIkarusLoops();
  setupEngineText();
  setupEngineMemory();

  // Inline ASM
  vm.override_function("ASMINT_Init",           [this]() { ASMINT_Init(); });
  vm.override_function("ASMINT_MyExternal",     [](){});
  vm.override_function("ASMINT_CallMyExternal", [this]() { ASMINT_CallMyExternal(); });

  // Pointers
  setupMemoryFunctions();

  // Dynamic calls by id's
  setupDirectFunctions();

  setupWinapiFunctions();
  setupFileFunctions();
  setupSaveSupport();
  setupFocusMagic();
  setupEngineCalls();

  setupUtilityFunctions();

  // LeGo hooks
  setupHookEngine();

  setupMathFunctions();
  setupStringsFunctions();

  setupzCParserFunctions();

  setupInitFileFunctions();
  setupUiFunctions();
  setupFontFunctions();
  setupNpcFunctions();
  setupWorldFunctions();

  // various
  vm.override_function("MEM_PrintStackTrace", [this](){ memPrintstacktraceImplementation(); });
  vm.override_function("MEM_SendToSpy",       [this](int cat, std::string_view msg){ return memSendToSpy(cat,msg); });
  vm.override_function("MEM_ReplaceFunc",     [this](zenkit::DaedalusFunction dest, zenkit::DaedalusFunction func){ memReplaceFunc(dest, func); });
  //
  vm.override_function("MEMINT_SetupExceptionHandler",   [](){ });
  vm.override_function("MEMINT_ReplaceSlowFunctions",    [](){ });
  //vm.override_function("MEMINT_ReplaceLoggingFunctions", [](){ });
  vm.override_function("MEM_InitStatArrs",               [](){ });
  vm.override_function("MEM_InitRepeat",                 [](){ });
  vm.override_function("MEM_GetCommandLine",             [](){ return std::string(""); });

  auto safeOverrideFunction = [&](std::string_view name, auto hook) {
    auto f = vm.find_symbol_by_name(name);
    if(f==nullptr || f->type()!=zenkit::DaedalusDataType::FUNCTION)
      return;
    vm.override_function(name, hook);
    };

  safeOverrideFunction("_RENDER_INIT", []() {
    // unclear how exactly it should behave - need to find testing sample
    Log::e("not implemented call [_RENDER_INIT]");
    });
  safeOverrideFunction("PRINT_FIXPS", [](){
    // function patches asm code of zCView::PrintTimed* to fix text coloring - we can ignore it
    });

  // override for now: need to expose way-net system and provide dma-access for npc
  safeOverrideFunction("TELEPORTNPCTOWP", [this](int npcId, std::string_view wpName){
    auto wp = gameScript.world().findPoint(wpName,false);
    if(wp==nullptr) {
      Log::d("TeleportNpcToWP: invalid waypoint: ", wpName);
      return;
      }
    auto npcRef = this->vm.find_symbol_by_index(uint32_t(npcId))->get_instance();
    if(npcRef==nullptr || npcRef->user_ptr==nullptr)
      return;
    auto& npc = *reinterpret_cast<Npc*>(npcRef->user_ptr);
    npc.setPosition (wp->position());
    npc.setDirection(wp->direction());
    });
  //
  safeOverrideFunction("LOG_MOVETOTOP", [](std::string_view topic) {
    // need to have a projection of 'OCLOGMANAGER_PTR' into mem32
    Log::e("not implemented call [LOG_MOVETOTOP]");
    });

  // Disable some high-level functions, until basic stuff is done
  auto dummyfy = [&](std::string_view name, auto hook) {
    auto f = vm.find_symbol_by_name(name);
    if(f==nullptr || f->type()!=zenkit::DaedalusDataType::FUNCTION)
      return;
    vm.override_function(name, hook);
    };
  dummyfy("WRITENOP",    [](int,int){}); // hook-related mess
  dummyfy("MEM_SETKEYS", [](std::string_view,int,int){});
  setupQuiverFunctions();
  }

bool DirectMemory::isRequired(zenkit::DaedalusScript& vm) {
  return
      vm.find_symbol_by_name("Ikarus_Version") != nullptr &&
      vm.find_symbol_by_name("MEM_InitAll") != nullptr &&
      vm.find_symbol_by_name("MEM_ReadInt") != nullptr &&
      vm.find_symbol_by_name("MEM_WriteInt") != nullptr &&
      vm.find_symbol_by_name("_@") != nullptr &&
      vm.find_symbol_by_name("_^") != nullptr;
  }

void DirectMemory::tick(uint64_t dt) {
  //TODO: propper hook-engine
  if(auto* sym = vm.find_symbol_by_name("_FF_Hook")) {
    vm.call_function(sym);
    }

  if(!hooksReported && !hooksUnanswered.empty()) {
    hooksReported = true;
    Log::i("DMA: ", hooksUnanswered.size(), " hooks of the libraries are not answered (tab Debug, Ikarus)");
    }

  if(!resolutionFired) {
    // oCGame::UpdateScreenResolution: the bars and the views of LeGo take the size of the screen
    resolutionFired = true;
    HookRegs r;
    fireHook(HookAdr::BarUpdateResolution,r);
    }

  npcListTimer += dt;
  if(npcListTimer>=1000 || npcList==0) {
    npcListTimer = 0;
    refreshNpcList();
    }

  // oCNpc.interactmob of the player: the mob that he uses
  if(auto* pl = gameScript.world().player()) {
    auto* mob = pl->interactive();
    pl->handle().compat_interact_mob = mob==nullptr ? 0 : int32_t(mobPointer(*mob));
    }

  tickTimers(dt);
  tickInfos();
  tickUi(dt);
  tickQuivers();
  }

/// What the scripts wrote to the oCInfo objects (OCINFO.TOLD) goes to the dialogs of the game
void DirectMemory::tickInfos() {
  if(pendingInfos.empty())
    return;
  auto* told = vm.find_symbol_by_name("OCINFO.TOLD");
  const int32_t off = told==nullptr ? -1 : int32_t(told->offset_as_member());
  for(auto& i:pendingInfos) {
    if(off>=0) {
      const int32_t v = mem32.readInt(i.ptr+ptr32_t(off));
      if(v!=i.told)
        gameScript.setInfoTold(i.id,v!=0);
      }
    mem32.free(i.ptr);
    }
  pendingInfos.clear();
  }

/// zTimer (the time of the frame) and oCWorldTimer (the time of the world) as the scripts of Ikarus and LeGo read them.
/// A script that changes the time of the world (SCALETIME, STOPTIMEATHOUR) writes WORLDTIME, the game takes it over.
void DirectMemory::tickTimers(uint64_t dt) {
  auto bits = [](float f) {
    int32_t i = 0;
    std::memcpy(&i,&f,sizeof(i));
    return i;
    };

  zTimer.FACTORMOTION       = bits(1.f);
  zTimer.FRAMETIMEFLOAT     = bits(float(dt));
  zTimer.FRAMETIMEFLOATSECS = bits(float(dt)/1000.f);
  zTimer.FRAMETIME          = int32_t(dt);
  zTimer.LASTTIMER          = zTimer.TOTALTIME;
  zTimer.TOTALTIME         += int32_t(dt);
  zTimer.TOTALTIMEFLOAT     = bits(float(zTimer.TOTALTIME));
  zTimer.TOTALTIMEFLOATSECS = bits(float(zTimer.TOTALTIME)/1000.f);
  zTimer.MINFRAMETIME       = 1;
  zTimer.FORCEDMAXFRAMETIME = 1000;

  auto* wt = mem32.deref<oCWorldTimer>(worldTimer);
  if(wt==nullptr)
    return;
  constexpr int64_t DayMilis      = 24ll*60*60*1000;
  constexpr double  MilisPerTick  = 3600.0*1000.0/250000.0;   // 250000 ticks make an hour
  auto& session = gameScript.world().gameSession();

  if(worldTimerInit && (wt->WORLDTIME!=worldTimerBits || wt->DAY!=worldTimerDay)) {
    // the script has changed the time
    float ticks = 0;
    std::memcpy(&ticks,&wt->WORLDTIME,sizeof(ticks));
    int64_t inDay = int64_t(double(ticks)*MilisPerTick);
    int64_t day   = wt->DAY;
    if(inDay<0) {
      inDay = 0;
      }
    if(inDay>=DayMilis) {
      day  += inDay/DayMilis;
      inDay = inDay%DayMilis;
      }
    gtime t(day,int64_t(0),int64_t(0));
    t.addMilis(uint64_t(inDay));
    session.setTime(t);
    }

  const int64_t now = session.time().toInt();
  const float   ticks = float(double(now%DayMilis)/MilisPerTick);
  wt->WORLDTIME  = bits(ticks);
  wt->DAY        = int32_t(now/DayMilis);
  worldTimerBits = wt->WORLDTIME;
  worldTimerDay  = wt->DAY;
  worldTimerInit = true;
  }

void DirectMemory::eventPlayAni(Npc& npc, std::string_view ani) {
  if(ani.find("CALL ")!=0)
    return;

  std::vector<std::string_view> tokens;
  auto rest = ani.substr(5);
  for(;;) {
    const auto end = rest.find(' ');
    tokens.push_back(rest.substr(0,end));
    if(end==std::string_view::npos)
      break;
    rest.remove_prefix(end+1);
    }

  auto integer = [](std::string_view text, int32_t& value) {
    const auto result = std::from_chars(text.data(),text.data()+text.size(),value);
    return result.ec==std::errc() && result.ptr==text.data()+text.size();
    };
  int32_t functionId = 0;
  std::string_view format;
  size_t firstArg = 0;
  if(tokens.size()==1) {
    if(!integer(tokens.front(),functionId))
      return;
    } else {
    format = tokens.front();
    firstArg = 1;
    if(format.empty() || format.find_first_not_of("INS")!=std::string_view::npos ||
       tokens.size()!=format.size()+2 || !integer(tokens.back(),functionId)) {
      Log::e("Ikarus: invalid AI_Function call: ",ani);
      return;
      }
    }

  auto* function = vm.find_symbol_by_index(uint32_t(functionId));
  if(function==nullptr || function->type()!=zenkit::DaedalusDataType::FUNCTION)
    return;
  const auto params = vm.find_parameters_for_function(function);
  if(params.size()!=format.size()) {
    Log::e("Ikarus: AI_Function argument count for ",function->name());
    return;
    }

  // Decode and validate everything before changing the VM stack or global instances.
  std::vector<int32_t> ints(format.size());
  std::vector<std::string> strings(format.size());
  std::vector<std::shared_ptr<zenkit::DaedalusInstance>> instances(format.size());
  static constexpr std::string_view sequences = "0123456abtnvfr789ABCDEFGHIJKLMNO_";
  auto unescape = [](std::string_view text) {
    std::string value;
    for(size_t i=0; i<text.size(); ++i) {
      char c = text[i];
      if(c=='\\' && i+1<text.size()) {
        c = text[++i];
        if(c=='x' && i+2<text.size()) {
          unsigned byte = 0;
          const auto result = std::from_chars(text.data()+i+1,text.data()+i+3,byte,16);
          if(result.ec==std::errc() && result.ptr==text.data()+i+3) {
            value.push_back(char(byte));
            i += 2;
            continue;
            }
          }
        if(c!='\\') {
          const auto index = sequences.find(c);
          if(index!=std::string_view::npos)
            c = char(index);
          }
        }
      value.push_back(c);
      }
    return value;
    };

  for(size_t i=0; i<format.size(); ++i) {
    const auto type = params[i].type();
    if(format[i]=='S') {
      if(type!=zenkit::DaedalusDataType::STRING)
        return;
      strings[i] = unescape(tokens[firstArg+i]);
      } else {
      if(!integer(tokens[firstArg+i],ints[i]))
        return;
      if(format[i]=='N') {
        if(type!=zenkit::DaedalusDataType::INSTANCE)
          return;
        if(ints[i]!=0) {
          auto* symbol = vm.find_symbol_by_index(uint32_t(ints[i]));
          if(symbol==nullptr || symbol->type()!=zenkit::DaedalusDataType::INSTANCE)
            return;
          instances[i] = symbol->get_instance();
          }
        } else if(type!=zenkit::DaedalusDataType::INT && type!=zenkit::DaedalusDataType::FLOAT &&
                  type!=zenkit::DaedalusDataType::FUNCTION) {
        return;
        }
      }
    }

  struct Context {
    zenkit::DaedalusVm& vm;
    std::shared_ptr<zenkit::DaedalusInstance> self, other, item, instance;
    ~Context() {
      vm.global_self()->set_instance(self);
      vm.global_other()->set_instance(other);
      vm.global_item()->set_instance(item);
      vm.unsafe_set_gi(instance);
      }
    } context{vm,vm.global_self()->get_instance(),vm.global_other()->get_instance(),
              vm.global_item()->get_instance(),vm.unsafe_get_gi()};
  vm.global_self()->set_instance(npc.handlePtr());
  vm.global_other()->set_instance(nullptr);
  vm.global_item()->set_instance(nullptr);
  vm.unsafe_set_gi(npc.handlePtr());

  for(size_t i=0; i<format.size(); ++i) {
    if(format[i]=='S')
      vm.push_string(strings[i]);
    else if(format[i]=='N')
      vm.push_instance(instances[i]);
    else if(params[i].type()==zenkit::DaedalusDataType::FLOAT)
      vm.push_float(intBitsToFloat(ints[i]));
    else
      vm.push_int(ints[i]);
    }
  vm.invoke_function(function);
  if(function->has_return()) {
    switch(function->rtype()) {
      case zenkit::DaedalusDataType::STRING:   (void)vm.pop_string();   break;
      case zenkit::DaedalusDataType::INSTANCE: (void)vm.pop_instance(); break;
      case zenkit::DaedalusDataType::FLOAT:    (void)vm.pop_float();    break;
      default:                               (void)vm.pop_int();      break;
      }
    }
  }

void DirectMemory::setupFunctionTable() {
  for(auto& i:vm.symbols()) {
    if(i.type()!=zenkit::DaedalusDataType::FUNCTION)
      continue;
    if(i.address()==0)
      continue;
    symbolsByAddress.push_back(&i);
    }

  std::sort(symbolsByAddress.begin(), symbolsByAddress.end(), [](const zenkit::DaedalusSymbol* l, const zenkit::DaedalusSymbol* r){
    return l->address() < r->address();
    });
  }

void DirectMemory::setupIkarusLoops() {
  // ## Traps
  if(auto end = vm.find_symbol_by_name("END")) {
    end->set_access_trap_enable(true);
    }
  if(auto break_ = vm.find_symbol_by_name("BREAK")) {
    break_->set_access_trap_enable(true);
    }
  if(auto continue_ = vm.find_symbol_by_name("CONTINUE")) {
    continue_->set_access_trap_enable(true);
    }

  auto end       = vm.find_symbol_by_name("END");
  auto rep       = vm.find_symbol_by_name("REPEAT");
  auto whl       = vm.find_symbol_by_name("WHILE");
  auto continue_ = vm.find_symbol_by_name("CONTINUE");

  uint32_t endIdx  = (end!=nullptr       ? end->index()       : uint32_t(-1));
  uint32_t cntIdx  = (continue_!=nullptr ? continue_->index() : uint32_t(-1));
  uint32_t repAddr = (rep!=nullptr       ? rep->address()     : uint32_t(-1));
  uint32_t whlAddr = (whl!=nullptr       ? whl->address()     : uint32_t(-1));

  // auto cont = vm.find_symbol_by_name("CONTINUE");

  // ## Control-flow ##
  vm.override_function("repeat",   [this](zenkit::DaedalusVm& vm) { return repeat(vm);    });
  vm.override_function("while",    [this](zenkit::DaedalusVm& vm) { return while_(vm);    });
  vm.override_function("mem_goto", [this](zenkit::DaedalusVm& vm) { return mem_goto(vm);  });
  vm.register_access_trap([this](zenkit::DaedalusSymbol& i) { return loop_trap(&i); });

  for(auto& i:vm.symbols()) {
    if(i.type()!=zenkit::DaedalusDataType::FUNCTION)
      continue;
    }

  struct LoopDecr {
    uint32_t pc      = 0;
    uint32_t backJmp = 0;
    };

  std::vector<LoopDecr> loopStk;
  for(uint32_t i=0; i<vm.size();) {
    const auto pc = i;
    auto instr = vm.instruction_at(i);
    i += instr.size;

    if(instr.op==zenkit::DaedalusOpcode::PUSHV && instr.symbol==endIdx) {
      auto loop = loopStk.back();
      loopBacktrack[loop.pc] = i; // loop exit
      loopBacktrack[pc]      = loop.backJmp;
      loopStk.pop_back();
      }
    else if(instr.op==zenkit::DaedalusOpcode::PUSHV && instr.symbol==cntIdx) {
      if(!loopStk.empty()) {
        auto func = findSymbolByAddress(pc); (void)func;
        auto loop = loopStk.back();
        loopBacktrack[pc] = loop.pc;
        }
      }
    else if(instr.op==zenkit::DaedalusOpcode::BL && (instr.address==repAddr || instr.address==whlAddr)) {
      LoopDecr d;
      if(instr.address==whlAddr) {
        d.backJmp = traceBackExpression(vm, pc);
        d.pc      = pc;
        }
      else if(instr.address==repAddr) {
        d.pc      = pc;
        d.backJmp = pc;
        }
      loopStk.push_back(d);
      }
    }
  }

void DirectMemory::setupEngineMemory() {
  mem32.setCallbackR(Mem32::Type::zCParser, [this](zCParser& p, uint32_t){ memoryCallback(p, std::memory_order::acquire); });
  mem32.setCallbackW(Mem32::Type::zCParser, [this](zCParser& p, uint32_t){ memoryCallback(p, std::memory_order::release); });

  mem32.setCallbackR(Mem32::Type::zCParser_variables, [this](ScriptVar& v, uint32_t id) {
    memoryCallback(v, id, std::memory_order::acquire);
    });
  mem32.setCallbackW(Mem32::Type::zCParser_variables, [this](ScriptVar& v, uint32_t id) {
    memoryCallback(v, id, std::memory_order::release);
    });
  mem32.setCallbackR(Mem32::Type::zCMover, [this](zCMover& m, uint32_t id) {
    memoryCallback(m, id, std::memory_order::acquire);
    });
  mem32.setCallbackW(Mem32::Type::zCMover, [this](zCMover& m, uint32_t id) {
    memoryCallback(m, id, std::memory_order::release);
    });
  mem32.setCallbackR(Mem32::Type::zCPar_Symbol, [this](zCPar_Symbol& p, uint32_t id) {
    memoryCallback(p, id, std::memory_order::acquire);
    });
  mem32.setCallbackW(Mem32::Type::zCPar_Symbol, [this](zCPar_Symbol& p, uint32_t id) {
    memoryCallback(p, id, std::memory_order::release);
    });

  const ptr32_t BAD_BUILTIN_PTR                = 0xBAD40000;
  const ptr32_t GothicFirstInstructionAddress  = 4198400;
  const ptr32_t MEMINT_oGame_Pointer_Address   = 11208836; //0xAB0884
  const ptr32_t MEMINT_zTimer_Address          = 10073044; //0x99B3D4
  const ptr32_t MEMINT_gameMan_Pointer_Address = 9185624;  //0x8C2958
  const ptr32_t ZFACTORY                       = 9276912;
  const ptr32_t ContentParserAddress           = 11223232;
  const ptr32_t ZFONTMAN                       = 11221460;
  const ptr32_t OCNPCFOCUS__FOCUSLIST_G2       = 11208440;

  mem32.pin(&versionHint,   GothicFirstInstructionAddress,  4, "MEMINT_ReportVersionCheck");
  mem32.pin(&oGame_Pointer, MEMINT_oGame_Pointer_Address,   4, "oGame*");
  mem32.pin(&gameman_Ptr,   MEMINT_gameMan_Pointer_Address, 4, "GameMan*");
  mem32.pin(&zTimer,        MEMINT_zTimer_Address,          sizeof(zTimer), "zTimer");
  mem32.pin(&zFactory_Ptr,  ZFACTORY,                       4, "zFactory*");
  mem32.pin(&fontMan_Ptr,   ZFONTMAN,                       4, "zCFontMan*");
  parserBase = ContentParserAddress;
  mem32.pin(ContentParserAddress, sizeof(zCParser), Mem32::Type::zCParser);
  mem32.pin(&focusList, OCNPCFOCUS__FOCUSLIST_G2, sizeof(focusList), "OCNPCFOCUS__FOCUSLIST_G2");

  const ptr32_t INGAME_MENU_INSTANCE = 8980576;
  mem32.pin(menuName, INGAME_MENU_INSTANCE, sizeof(menuName)-1, "MENU_NAME");
  std::strncpy(menuName, ::menuMain.data(), std::min(sizeof(menuName), ::menuMain.size()));

  const ptr32_t ZERRPTR = 9231568;
  mem32.alloc(ZERRPTR, sizeof(zError));

  const ptr32_t LODENABLED = 8596020;
  mem32.alloc(LODENABLED, 4, "LODENABLED"); // can always ignore that

  const ptr32_t AMBIENTVOBSENABLED = 9079488;
  mem32.alloc(AMBIENTVOBSENABLED, 4, "AMBIENTVOBSENABLED");

  const ptr32_t GAME_HOLDTIME_ADDRESS = 11208840;
  mem32.alloc(GAME_HOLDTIME_ADDRESS, 4, "GAME_HOLDTIME_ADDRESS"); // need to investigate how exactly it affects game

  if(auto p = mem32.deref<std::array<ptr32_t,6>>(OCNPCFOCUS__FOCUSLIST_G2)) {
    gameScript.focusMage();
    (*p)[5] = BAD_BUILTIN_PTR; //TODO: pick-lock spells
    }

  // Trialog: need to implement reinterpret_cast to avoid in script-exception
  const ptr32_t SPAWN_INSERTRANGE = 9153744;
  mem32.pin(&spawnRange, SPAWN_INSERTRANGE, sizeof(spawnRange), "spawnRange");

  // Storage for local variables, so script may address them thru pointers
  {
    const size_t n = vm.symbols().size();
    varSlotFirst.assign(n,0);
    varSlotSym.clear();
    varSlotSym.reserve(n+n/16);
    for(size_t i=0; i<n; ++i) {
      auto* s = vm.find_symbol_by_index(uint32_t(i));
      uint32_t slots = 1;
      if(s!=nullptr && !s->is_member() && s->count()>1) {
        if(s->type()==zenkit::DaedalusDataType::INT || s->type()==zenkit::DaedalusDataType::FLOAT)
          slots = (s->count()+4)/5;
        else if(s->type()==zenkit::DaedalusDataType::STRING)
          slots = s->count();
        }
      varSlotFirst[i] = uint32_t(varSlotSym.size());
      for(uint32_t k=0; k<slots; ++k)
        varSlotSym.push_back(uint32_t(i));
      }
    }
  scriptVariables = mem32.alloc(uint32_t(varSlotSym.size() * sizeof(ScriptVar)),      Mem32::Type::zCParser_variables);
  scriptSymbols   = mem32.alloc(uint32_t(vm.symbols().size() * sizeof(zCPar_Symbol)), Mem32::Type::zCPar_Symbol);
  handles.setup();
  handles.setSync([this](uint8_t* raw, zenkit::DaedalusInstance& inst, bool write) {
    syncHandleObject(raw,inst,write);
    });
  setupInstanceAliases();
  mem32.setCallbackR(Mem32::Type::mobHandle,[this](Compatibility::MobSlot& m, uint32_t id) {
    memoryCallback(m,id,std::memory_order::acquire);
    });
  mem32.setCallbackW(Mem32::Type::mobHandle,[this](Compatibility::MobSlot& m, uint32_t id) {
    memoryCallback(m,id,std::memory_order::release);
    });
  mobBase = mem32.alloc(MobSlots*uint32_t(sizeof(Compatibility::MobSlot)),Mem32::Type::mobHandle,"oCMobLockable");
  mobs.resize(MobSlots,nullptr);
  mobPick.resize(MobSlots,0);

  // Built-in data without assumed address
  oGame_Pointer = mem32.pin(&memGame, sizeof(memGame), "oGame");
  memGame._ZCSESSION_WORLD = mem32.alloc(sizeof(oWorld));
  worldTimer               = mem32.alloc(sizeof(oCWorldTimer),"oCWorldTimer");
  memGame.WLDTIMER         = worldTimer;
  memGame.TIMESTEP         = 1; // used as boolean in anim8

  auto& mem_world = *mem32.deref<oWorld>(memGame._ZCSESSION_WORLD);
  mem_world.WAYNET       = BAD_BUILTIN_PTR; //TODO: add implement some proxy to waynet
  mem_world.VOBLIST_NPCS = 0; //BAD_BUILTIN_PTR; // zCListSort*

  gameman_Ptr = mem32.alloc(512,"CGameManager");
  if(auto v = vm.find_symbol_by_name("CurrSymbolTableLength")) {
    v->set_int(int(vm.symbols().size()));
    }

  fontMan_Ptr = mem32.alloc(sizeof(zCFontMan));

  // ## UI data
  memGame.HPBAR    = mem32.alloc(sizeof(oCViewStatusBar));
  memGame.MANABAR  = mem32.alloc(sizeof(oCViewStatusBar));
  memGame.FOCUSBAR = mem32.alloc(sizeof(oCViewStatusBar));

  memGame._ZCSESSION_VIEWPORT = mem32.alloc(sizeof(zCView));
  memGame.ARRAY_VIEW[0] = mem32.alloc(sizeof(zCView));
  // the screen of the scripts is 800x600 (the scripts read it at the start, before the first frame)
  if(auto* v = mem32.deref<zCView>(memGame._ZCSESSION_VIEWPORT)) {
    v->PSIZEX = 800;  v->PSIZEY = 600;
    v->VSIZEX = 8192; v->VSIZEY = 8192;
    }
  if(auto* v = mem32.deref<zCView>(memGame.ARRAY_VIEW[0])) {
    v->PSIZEX = 800;  v->PSIZEY = 600;
    v->VSIZEX = 8192; v->VSIZEY = 8192;
    }

  mem32.deref<oCViewStatusBar>(memGame.HPBAR)   ->RANGE_BAR = mem32.alloc(sizeof(zCView));
  mem32.deref<oCViewStatusBar>(memGame.HPBAR)   ->VALUE_BAR = mem32.alloc(sizeof(zCView));
  mem32.deref<oCViewStatusBar>(memGame.FOCUSBAR)->RANGE_BAR = mem32.alloc(sizeof(zCView));
  mem32.deref<oCViewStatusBar>(memGame.FOCUSBAR)->VALUE_BAR = mem32.alloc(sizeof(zCView));
  }

void DirectMemory::setupEngineText() {
  // pin functions - CoM and some other mods rewriting assembly for them
  const uint32_t OCNPC__ENABLE_EQUIPBESTWEAPONS = 7626662;
  mem32.alloc(OCNPC__ENABLE_EQUIPBESTWEAPONS, 18, "OCNPC__ENABLE_EQUIPBESTWEAPONS");

  const uint32_t OCNPC__GETNEXTENEMY = 7556941;
  mem32.alloc(OCNPC__GETNEXTENEMY, 48, "OCNPC__GETNEXTENEMY");

  const uint32_t OCITEMCONTAINER__CHECKSELECTEDITEM_ISACTIVE = 7378665;
  mem32.alloc(OCITEMCONTAINER__CHECKSELECTEDITEM_ISACTIVE, 5, ".text");

  const uint32_t OCITEMCONTAINER__CHECKSELECTEDITEM_ISACTIVEP = 7378700;
  mem32.alloc(OCITEMCONTAINER__CHECKSELECTEDITEM_ISACTIVEP, 5, ".text");

  const int OCSTEALCONTAINER__CREATELIST_ISARMOR_SP18 = 7384908;
  mem32.alloc(OCSTEALCONTAINER__CREATELIST_ISARMOR_SP18, 8, ".text");

  const int OCNPCCONTAINER__CREATELIST_ISARMOR_SP18 = 7386812;
  mem32.alloc(OCNPCCONTAINER__CREATELIST_ISARMOR_SP18, 8, ".text");

  const int OCSTEALCONTAINER__CREATELIST_ISARMOR = 7384900;
  mem32.alloc(OCSTEALCONTAINER__CREATELIST_ISARMOR, 8, ".text");

  const int OCNPCCONTAINER__CREATELIST_ISARMOR = 7386805;
  mem32.alloc(OCNPCCONTAINER__CREATELIST_ISARMOR, 5, ".text");

  const int OCNPCCONTAINER__HANDLEEVENT_ISEMPTY = 7387581;
  mem32.alloc(OCNPCCONTAINER__HANDLEEVENT_ISEMPTY, 5, ".text");

  const int OCNPCINVENTORY__HANDLEEVENT_KEYWEAPONJZ = 7402077;
  mem32.alloc(OCNPCINVENTORY__HANDLEEVENT_KEYWEAPONJZ, 4, ".text");

  const int OCNPCINVENTORY__HANDLEEVENT_KEYWEAPON = 7402065;
  mem32.alloc(OCNPCINVENTORY__HANDLEEVENT_KEYWEAPON, 8, ".text");

  const int OCAIHUMAN__CHANGECAMMODEBYSITUATION_SWITCHMOBCAM = 6935573;
  mem32.alloc(OCAIHUMAN__CHANGECAMMODEBYSITUATION_SWITCHMOBCAM, 8, ".text");

  // unknown, code segment used by INIT_RESTOREMOBFIRESTATES in CoM
  const int INIT_RESTOREMOBFIRESTATES_P0 = 7482368;
  const int INIT_RESTOREMOBFIRESTATES_P1 = 7482688;
  mem32.alloc(INIT_RESTOREMOBFIRESTATES_P0, 4, ".text");
  mem32.alloc(INIT_RESTOREMOBFIRESTATES_P1, 4, ".text");
  }

zenkit::DaedalusNakedCall DirectMemory::repeat(zenkit::DaedalusVm& vm) {
  const int               len = vm.pop_int();
  zenkit::DaedalusSymbol* i   = std::get<zenkit::DaedalusSymbol*>(vm.pop_reference());

  auto rp = vm.instruction_at(vm.pc());
  if(len==0 || i==nullptr) {
    auto jmp = loopBacktrack[vm.pc()];
    vm.unsafe_jump(jmp-rp.size);
    return zenkit::DaedalusNakedCall();
    }
  // Log::i("repeat: ", i->get_int(), " < ", len);
  auto& pl = loopPayload[vm.pc()];
  pl.i   = i;
  pl.len = len;
  i->set_int(0);
  if(len>1024)
    Log::d("");
  return zenkit::DaedalusNakedCall();
  }

zenkit::DaedalusNakedCall DirectMemory::while_(zenkit::DaedalusVm& vm) {
  const int cond = vm.pop_int();
  // Log::i("while: ", cond);
  if(cond!=0) {
    return zenkit::DaedalusNakedCall();
    }
  auto rp  = vm.instruction_at(vm.pc());
  auto jmp = loopBacktrack[vm.pc()];
  vm.unsafe_jump(jmp-rp.size);
  return zenkit::DaedalusNakedCall();
  }

zenkit::DaedalusNakedCall DirectMemory::mem_goto(zenkit::DaedalusVm& vm) {
  const int idx = vm.pop_int();

  auto at   = vm.pc();
  auto func = findSymbolByAddress(at);
  if(func==nullptr) {
    Log::e("Ikarus: invalid vm state");
    return zenkit::DaedalusNakedCall();
    }

  auto     rp      = vm.instruction_at(at);
  auto     label   = vm.find_symbol_by_name("mem_label");
  uint32_t lblAddr = (label!=nullptr ? label->address() : uint32_t(-1));
  uint32_t pc      = func->address();

  for(uint32_t i=0, prev=0; i<at; ++i) {
    auto instr = vm.instruction_at(pc);
    if(instr.op==zenkit::DaedalusOpcode::BL && instr.address==lblAddr) {
      auto lId = vm.instruction_at(prev);
      if(lId.immediate==idx) {
        vm.unsafe_jump(pc + instr.size - rp.size);
        return zenkit::DaedalusNakedCall();
        }
      }
    prev = pc;
    pc += instr.size;
    }

  Log::e("Ikarus: invalid goto");
  return zenkit::DaedalusNakedCall();
  }

void DirectMemory::loop_trap(zenkit::DaedalusSymbol* i) {
  auto instr = vm.instruction_at(vm.pc());
  if(instr.op != zenkit::DaedalusOpcode::PUSHV)
    return; // Ikarus keywords are always use pushv

  auto end = vm.find_symbol_by_name("END");
  if(end!=nullptr && instr.symbol==end->index()) {
    auto bt = loopBacktrack[vm.pc()];

    if(loopPayload.find(bt)==loopPayload.end()) {
      // while
      vm.unsafe_jump(bt-instr.size);
      return;
      }

    auto& pl = loopPayload[bt];
    if(pl.i==nullptr) {
      Log::e("bad loop end");
      assert(0);
      return;
      }
    const int32_t i = pl.i->get_int();
    pl.i->set_int(i+1);
    if(i+1 < pl.len) {
      const uint32_t BLsize = 5;
      vm.unsafe_jump(bt-instr.size+BLsize);
      } else {
      loopPayload.erase(bt);
      }
    return;
    }

  if(loopBacktrack.find(vm.pc())!=loopBacktrack.end()) {
    Log::e("bad loop trap");
    assert(0);
    }
  }

uint32_t DirectMemory::traceBackExpression(zenkit::DaedalusVm& vm, uint32_t pc) {
  /*
   NOTE: can be expression based 'while';
   while(var){}
     PUSHV @var
     BL @while

   while(i < l) {}
     PUSHV @l
     PUSHV @i
     LT
     BL

    handles by MEMINT_TraceParameter in ikarus
  */

  auto func = findSymbolByAddress(pc);
  if(func==nullptr)
    return uint32_t(-1); //error

  // if(func->name()=="LIST_END")
  //   Log::d("");

  std::vector<zenkit::DaedalusInstruction> icodes;
  for(uint32_t i=func->address(); i<pc && i<vm.size();) {
    auto instr = vm.instruction_at(i);
    i += instr.size;
    icodes.push_back(instr);
    }

  if(icodes.empty())
    return uint32_t(-1); //error

  int paramsNeeded = 1;
  int instancesNeed = 0;
  size_t at = icodes.size();
  while((paramsNeeded>0 || instancesNeed>0) && at>0) {
    --at;
    const auto instr = icodes[at];
    if(zenkit::DaedalusOpcode::ADD <= instr.op && instr.op <= zenkit::DaedalusOpcode::DIVMOVI)
      paramsNeeded += 1;
    else if(instr.op==zenkit::DaedalusOpcode::PUSHI) {
      paramsNeeded -= 1;
      }
    else if(instr.op==zenkit::DaedalusOpcode::PUSHV || instr.op==zenkit::DaedalusOpcode::PUSHVI) {
      auto* sym = vm.find_symbol_by_index(instr.symbol);
      if(sym!=nullptr && sym->is_member())
        instancesNeed = 1; // need instance to be set
      paramsNeeded -= 1;
      }
    else if(instr.op==zenkit::DaedalusOpcode::GMOVI) {
      instancesNeed = 0;
      }
    else if(instr.op==zenkit::DaedalusOpcode::BL) {
      auto sym = vm.find_symbol_by_address(instr.address);
      if(sym==nullptr)
        return uint32_t(-1); //error

      paramsNeeded += sym->count();
      if(sym->has_return())
        paramsNeeded -= 1;
      }
    else {
      // non handled
      return uint32_t(-1); //error
      }
    }

  if(paramsNeeded!=0)
    return uint32_t(-1); //error

  pc = func->address();
  for(uint32_t i=0; i<at; ++i) {
    auto instr = vm.instruction_at(pc);
    pc += instr.size;
    }

  return pc;
  }

const zenkit::DaedalusSymbol* DirectMemory::findSymbolByAddress(uint32_t addr) {
  if(symbolsByAddress.empty())
    return nullptr;
  auto fn = std::lower_bound(symbolsByAddress.begin(), symbolsByAddress.end(), addr, [](const zenkit::DaedalusSymbol* l, uint32_t r){
    return l->address()<r;
    });
  //auto id = std::distance(symbolsByAddress.begin(), fn); (void)id;
  if(fn==symbolsByAddress.end())
    return symbolsByAddress.back();
  if((*fn)->address()==addr)
    return *fn;
  if(fn==symbolsByAddress.begin())
    return nullptr;
  --fn;
  return *fn;
  }

std::string_view DirectMemory::demangleAddress(uint32_t addr){
  for(auto& i:vm.symbols()) {
    if(!i.is_const() || i.is_member())
      continue;
    if(i.type()!=zenkit::DaedalusDataType::INT)// && i.type() != zenkit::DaedalusDataType::FUNCTION)
      continue;
    if(ptr32_t(i.get_int())!=addr)
      continue;
    return i.name();
    }
  return "";
  }

std::string_view DirectMemory::menuMain() const {
  return menuName;
  }

void DirectMemory::memoryCallback(zCParser& p, std::memory_order ord) {
  if(ord==std::memory_order::acquire) {
    if(p.symtab_table.ptr==0) {
      // NOTE: initialize once
      // currSymbolTableAddress / currSymbolTableLength
      p.symtab_table.numInArray = int32_t(vm.symbols().size());
      p.symtab_table.numAlloc   = int32_t(nextPot(uint32_t(p.symtab_table.numInArray)));
      p.symtab_table.ptr        = mem32.realloc(p.symtab_table.ptr, uint32_t(p.symtab_table.numAlloc)*uint32_t(sizeof(uint32_t)));      
      auto v = mem32.deref<ptr32_t>(p.symtab_table.ptr);
      for(int32_t i=0; i<p.symtab_table.numInArray; ++i) {
        v[i] = scriptSymbols + uint32_t(i)*uint32_t(sizeof(zCPar_Symbol));
        }

      // TODO: currSortedSymbolTableAddress
      // TODO: currParserStackAddress
      }

    p.stack_stack = 0; //TODO: map insteructions into mem32?

    auto trap = vm.instruction_at(vm.pc());
    p.stack_stackptr = vm.pc() + trap.size;
    }
  else {
    // only the position of the stack is a jump, the other fields of the parser (the symbols, the data of a structure that
    // LeGo has unarchived) are written without a jump
    if(mem32.lastWriteAddress()!=parserBase+ptr32_t(offsetof(zCParser,stack_stackptr)))
      return;

    // the write to the position of the stack is a jump of the script; a wild value is not followed
    if(vm.pc()>=vm.size()) {
      if(logBudget("unsafe-jump-pc",4))
        Log::e("DMA: the write to the position of the stack outside the code, pc ",vm.pc());
      return;
      }
    auto instr = vm.instruction_at(vm.pc());

    auto src = findSymbolByAddress(vm.pc() - instr.size);
    auto dst = findSymbolByAddress(p.stack_stackptr);

    if(src!=dst || dst==nullptr) {
      if(logBudget("unsafe-jump",12)) {
        Log::e("FIXME: unsafe jump: the write to ",reinterpret_cast<void*>(uint64_t(mem32.lastWriteAddress())),", pc ",vm.pc()," to ",p.stack_stackptr," from `\"",
               src==nullptr ? "?" : src->name().c_str(), "\" (",src==nullptr ? 0 : src->address(),") to \"",
               dst==nullptr ? "?" : dst->name().c_str(), "\" (",dst==nullptr ? 0 : dst->address(),")");
        vm.print_stack_trace();
        }
      return;
      }
    vm.unsafe_jump(p.stack_stackptr - instr.size);
    }
  }

ptr32_t DirectMemory::varAddress(uint32_t symbol, uint32_t element) const {
  if(symbol>=varSlotFirst.size())
    return 0;
  const uint32_t first = varSlotFirst[symbol];
  auto* s = vm.find_symbol_by_index(symbol);
  if(s!=nullptr && (s->type()==zenkit::DaedalusDataType::INT || s->type()==zenkit::DaedalusDataType::FLOAT))
    return scriptVariables + (first + element/5)*ptr32_t(sizeof(ScriptVar)) + (element%5)*4;
  return scriptVariables + (first + element)*ptr32_t(sizeof(ScriptVar));
  }

int64_t DirectMemory::varSymbolAt(ptr32_t address) const {
  if(address<scriptVariables)
    return -1;
  const uint64_t slot = (address-scriptVariables)/sizeof(ScriptVar);
  if(slot>=varSlotSym.size())
    return -1;
  return varSlotSym[slot];
  }

void DirectMemory::memoryCallback(ScriptVar& v, uint32_t slot, std::memory_order ord) {
  if(slot>=varSlotSym.size() || varSlotSym[slot]>=vm.symbols().size()) {
    // should never happend
    Log::d("ikarus: symbol table is corrupted");
    assert(false);
    return;
    }

  const uint32_t index = varSlotSym[slot];
  const uint32_t sub   = slot - varSlotFirst[index];   // the part of the array
  auto& str = v.data;
  auto& sym = *vm.find_symbol_by_index(index);
  if(sym.is_member()) {
    Log::e("Ikarus: accessing member symbol (\"", sym.name(), "\")");
    return;
    }

  switch (sym.type()) {
    case zenkit::DaedalusDataType::FLOAT:
    case zenkit::DaedalusDataType::INT: {
      // a slot is five ints: the elements of an array go one after another (the memory is read before it is written, so
      // the other elements of the slot are fresh)
      int32_t*       w   = reinterpret_cast<int32_t*>(&str);
      const bool     isF = sym.type()==zenkit::DaedalusDataType::FLOAT;
      const uint32_t cnt = std::max<uint32_t>(sym.count(),1);
      const uint32_t e0  = sub*5;
      const uint32_t n   = e0<cnt ? std::min<uint32_t>(5,cnt-e0) : 0;
      for(uint32_t k=0; k<n; ++k) {
        const uint16_t el = uint16_t(e0+k);
        if(ord==std::memory_order::release) {
          if(isF) {
            float f = 0;
            std::memcpy(&f,&w[k],sizeof(f));
            sym.set_float(f,el);
            } else {
            sym.set_int(w[k],el);
            }
          } else {
          w[k] = isF ? floatBitsToInt(sym.get_float(el)) : sym.get_int(el);
          }
        }
      break;
      }
    case zenkit::DaedalusDataType::STRING: {
      // A string of the VM and the zString that the scripts see are two copies. The scripts build a text by writing the
      // pointer and the length of the zString (BR_Text of LeGo), the VM changes the text by assignments: what changed
      // since the last time wins, so the memory that a script has prepared is not overwritten by a read.
      auto& st = strVars[slot];
      if(ord==std::memory_order::release) {
        std::string dst;
        memFromString(dst, str);
        if(!st.known || dst!=st.synced) {
          sym.set_string(dst,uint16_t(sub));
          st.synced = dst;
          st.known  = true;
          }
        // the script has put its own buffer in: it may fill it after it has set the length (STR_Split of Ikarus)
        st.zptr = str.ptr;
        st.zlen = str.len;
        if(str.ptr!=st.buf && !st.owned) {
          st.owned = true;
          ownedStrings.push_back(slot);
          }
        } else {
        const std::string& cur = sym.get_string(uint16_t(sub));
        if(!st.known || cur!=st.synced) {
          // the buffer is replaced only when it is the one that was made here, a script may own the other
          const ptr32_t old = (str.ptr==st.buf) ? st.buf : 0;
          str.ptr = mem32.realloc(old, uint32_t(cur.size()+1));
          str.len = int32_t(cur.size());
          if(auto* chr = reinterpret_cast<char*>(mem32.derefv(str.ptr, uint32_t(cur.size()+1)))) {
            std::memcpy(chr, cur.data(), cur.size());
            chr[cur.size()] = '\0';
            }
          st.buf    = str.ptr;
          st.synced = cur;
          st.known  = true;
          if(st.owned) {
            st.owned = false;
            ownedStrings.erase(std::remove(ownedStrings.begin(),ownedStrings.end(),slot),ownedStrings.end());
            }
          }
        }
      break;
      }
    case zenkit::DaedalusDataType::INSTANCE: {
      // the scripts check the table of virtual functions before they trust a pointer
      if(ord==std::memory_order::release)
        break;
      const auto inst = sym.get_instance();
      if(dynamic_cast<zenkit::INpc*>(inst.get()))
        str._VTBL = 8640292;   // oCNpc_vtbl
      else if(dynamic_cast<zenkit::IItem*>(inst.get()))
        str._VTBL = 8636420;   // oCItem_vtbl
      else
        str._VTBL = 0;
      break;
      }
    default:
      Log::e("Ikarus: unable to map symbol (\"", sym.name(), "\") to virtual memory");
      break;
    }
  }

/// Where the data of an instance of the script is in the pretended memory
auto DirectMemory::instancePointer(zenkit::DaedalusSymbol& sym) -> ptr32_t {
  auto inst = sym.get_instance();
  if(auto d = dynamic_cast<memory_instance*>(inst.get()))
    return d->address;
  return handles.pointerTo(inst);
  }

void DirectMemory::memoryCallback(zCPar_Symbol& s, uint32_t index, std::memory_order ord) {
  if(index>=vm.symbols().size()) {
    // should never happend
    Log::e("ikarus: symbol table is corrupted");
    assert(false);
    return;
    }

  if(ord!=std::memory_order::acquire) {
    auto& sym = *vm.find_symbol_by_index(index);
    // the data of an instance is set by the pointer in the field "offset" of its symbol (PermMem of LeGo does it)
    if(sym.type()==zenkit::DaedalusDataType::INSTANCE && s.offset!=0 && ptr32_t(s.offset)!=instancePointer(sym)) {
      sym.set_instance(std::make_shared<memory_instance>(*this, ptr32_t(s.offset)));
      return;
      }
    if(logBudget("symbol-write"))
      Log::e("ikarus: write to symbol ", sym.name()," table is not implemented");
    return;
    }

  auto symBitfield = [](zenkit::DaedalusSymbol& sym) {
    int32_t flags = 0;
    if(sym.is_const())
      flags |= zenkit::DaedalusSymbolFlag::CONST;
    if(sym.has_return())
      flags |= zenkit::DaedalusSymbolFlag::RETURN;
    if(sym.is_member())
      flags |= zenkit::DaedalusSymbolFlag::MEMBER;
    if(sym.is_external())
      flags |= zenkit::DaedalusSymbolFlag::EXTERNAL;
    if(sym.is_merged())
      flags |= zenkit::DaedalusSymbolFlag::MERGED;

    int32_t bitfield = 0;
    bitfield |= (sym.count() & 0xFFF);       // 12 bits
    bitfield |= (int32_t(sym.type()) << 12); // 4 bits
    bitfield |= (flags << 16);               // 6 bits
    return bitfield;
    };

  auto symOffset = [this](zenkit::DaedalusSymbol& sym) {
    if(sym.is_member()) {
      return int32_t(sym.offset_as_member());
      }
    else if(sym.type()==zenkit::DaedalusDataType::INSTANCE) {
      return int32_t(instancePointer(sym));
      }
    else if(sym.type()==zenkit::DaedalusDataType::CLASS) {
      // see 'create' in LeGo
      return int32_t(sym.class_size());
      }
    else if(sym.type()==zenkit::DaedalusDataType::FUNCTION) {
      // return int32_t(sym.address());
      return int32_t(sym.rtype());
      }
     // TODO: pointer to value
    return 0;
    };

  auto symContent = [this](zenkit::DaedalusSymbol& sym) {
    // a member has a value only in an instance, reading it without one is an exception of the script
    if(sym.is_member())
      return 0;
    switch (sym.type()) {
      case zenkit::DaedalusDataType::VOID:
        return 0;
      case zenkit::DaedalusDataType::FLOAT:
        if(sym.count()>1)
          return int32_t(varAddress(sym.index()));   // an array: the pointer to the elements
        return floatBitsToInt(sym.get_float());
      case zenkit::DaedalusDataType::INT:
        if(sym.count()>1)
          return int32_t(varAddress(sym.index()));
        return int32_t(sym.get_int());
      case zenkit::DaedalusDataType::FUNCTION:
        if(sym.is_const())
          return int32_t(sym.address());
        return int32_t(sym.get_int());
      case zenkit::DaedalusDataType::STRING:
        // the pointer to the strings (PermMem of LeGo reads the structures of the classes from them)
        return int32_t(varAddress(sym.index()));
      case zenkit::DaedalusDataType::CLASS:
      case zenkit::DaedalusDataType::PROTOTYPE:
      case zenkit::DaedalusDataType::INSTANCE:
        //TODO
        return 0;
      }
    return 0;
    };

  auto& sym = *vm.find_symbol_by_index(index);
  memAssignString(s.name, sym.name());
  s.next     = scriptSymbols + uint32_t(index+1)*uint32_t(sizeof(zCPar_Symbol)); // ???
  //---
  s.content  = symContent(sym);
  s.offset   = symOffset(sym);
  //---
  s.bitfield = symBitfield(sym);
  s.filenr   = int32_t(sym.file_index());
  s.line     = int32_t(sym.line_start());
  s.line_anz = int32_t(sym.line_count());
  s.pos_beg  = int32_t(sym.char_start());
  s.pos_anz  = int32_t(sym.char_count());
  //---
  s.parent   = scriptSymbols + uint32_t(sym.parent())*uint32_t(sizeof(zCPar_Symbol));

  // other flags are internal
  // Log::d("Ikarus: accessing symbol (\"", sym.name(), "\")");
  }

void DirectMemory::memAssignString(zString& str, std::string_view cstr) {
  str.ptr = mem32.realloc(str.ptr, uint32_t(cstr.size()+1));
  str.len = int32_t(cstr.size());

  auto* chr  = reinterpret_cast<char*>(mem32.derefv(str.ptr, uint32_t(str.len)));
  std::memcpy(chr, cstr.data(), cstr.length());
  chr[str.len] = '\0';
  }

void DirectMemory::memFromString(std::string& dst, const zString& str) {
  if(str.len<=0) {
    dst.clear();
    return;
    }

  if(const char* chr = reinterpret_cast<const char*>(mem32.deref(str.ptr, uint32_t(str.len)))) {
    dst.resize(size_t(str.len));
    std::memcpy(dst.data(), chr, dst.size());
    }
  }


void DirectMemory::memPrintstacktraceImplementation() {
  static bool enable = true;
  if(!enable)
    return;
  Log::e("[start of stacktrace]");
  vm.print_stack_trace();
  Log::e("[end of stacktrace]");
  }

void DirectMemory::memSendToSpy(int cat, std::string_view msg) {
  Log::d("[zpy]: ", msg);
  }

void DirectMemory::memReplaceFunc(zenkit::DaedalusFunction dest, zenkit::DaedalusFunction func) {
  auto* sf  = func.value;
  auto* sd  = dest.value;
  if(sd->name()=="MEM_SENDTOSPY")
    return;
  if(sd->name()=="MEM_PRINTSTACKTRACE")
    return;
  if(sd==nullptr || sf==nullptr || sd==sf)
    return;

  // Ikarus puts a jump to the other function at the beginning of the first one: every call of dest goes to func.
  // LeGo uses it for the functions that call each other (a script cannot declare a function before it is written).
  const uint32_t destId = sd->index();
  const uint32_t funcId = sf->index();
  for(auto& r:replaced) {
    if(r.first==destId) {
      r.second = funcId;
      return;
      }
    }
  replaced.emplace_back(destId,funcId);

  vm.override_function(sd->name(),[this,destId](zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
    for(auto& r:replaced) {
      if(r.first!=destId)
        continue;
      if(auto* target = vm.find_symbol_by_index(r.second))
        directCall(vm,*target);
      break;
      }
    return zenkit::DaedalusNakedCall();
    });
  Log::d("mem_replacefunc: ",sd->name()," -> ",sf->name());
  }

void DirectMemory::ASMINT_Init() {
  const int ASMINT_InternalStackSize = 1024;
  Log::i("DMA: ASMINT_Init (the inline assembler of Ikarus)");

  if(!ASMINT_InternalStack) {
    ASMINT_InternalStack = mem32.alloc(4 * ASMINT_InternalStackSize);
    }
  if(auto sym = vm.find_symbol_by_name("ASMINT_InternalStack")) {
    if(sym->type()==zenkit::DaedalusDataType::INT)
      sym->set_int(int32_t(ASMINT_InternalStack));
    }

  auto p = mem32.pin(&ASMINT_CallTarget, sizeof(ASMINT_CallTarget), "ASMINT_CallTarget");
  if(auto sym = vm.find_symbol_by_name("ASMINT_CallTarget")) {
    if(sym->type()==zenkit::DaedalusDataType::INT)
      sym->set_int(int32_t(p));
    }
  }

void DirectMemory::ASMINT_CallMyExternal() {
  auto mem = mem32.deref(ASMINT_CallTarget);
  auto ins = reinterpret_cast<const uint8_t*>(std::get<0>(mem));
  auto len = std::get<1>(mem);

  cpu.exec(ASMINT_CallTarget, ins, len);
  }

void DirectMemory::setupMemoryFunctions() {
  vm.override_function("MEM_GetAddress_Init",  [    ](){ });

  vm.override_function("MEM_PtrToInst",        [this](int address)           { return mem_ptrtoinst(ptr32_t(address)); });
  vm.override_function("_^",                   [this](int address)           { return mem_ptrtoinst(ptr32_t(address)); });
  vm.override_function("MEM_InstToPtr",        [this](int index)             { return mem_insttoptr(index); });
  vm.override_function("MEM_GetIntAddress",    [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });
  vm.override_function("_@",                   [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });
  vm.override_function("MEM_GetStringAddress", [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });
  vm.override_function("_@s",                  [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });
  vm.override_function("MEM_GetFloatAddress",  [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });
  vm.override_function("_@f",                  [this](zenkit::DaedalusVm& vm){ return _takeref(vm);         });

  vm.override_function("MEM_ReadInt",          [this](int address){ return mem_readint(address);                  });
  vm.override_function("MEM_WriteInt",         [this](int address, int val){ mem_writeint(address, val);          });
  vm.override_function("MEM_CopyBytes",        [this](int src, int dst, int size){ mem_copybytes(src, dst, size); });
  vm.override_function("MEM_ReadString",       [this](int sym){ return mem_readstring(sym);           });
  vm.override_function("MEM_ReadStatArr",      [this](zenkit::DaedalusVm& vm){ return mem_readstatarr(vm); });
  vm.override_function("MEM_WriteStatArr",     [this](zenkit::DaedalusVm& vm){ return mem_writestatarr(vm); });

  // ## MEM_Alloc and MEM_Free ##
  vm.override_function("MEM_Alloc",   [this](int amount )                      { return mem_alloc(amount);               });
  vm.override_function("MEM_Free",    [this](int address)                      { mem_free(address);                      });
  vm.override_function("MEM_Realloc", [this](int address, int oldsz, int size) { return mem_realloc(address,oldsz,size); });
  }

auto DirectMemory::_takeref(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  if(vm.top_is_reference()) {
    auto [ref, idx, context] = vm.pop_reference();
    if(idx!=0 && ref->is_member()) {
      Log::e("Ikarus: _takeref - unable take reference to array element");
      vm.push_int(int32_t(0xBAD10000));
      return zenkit::DaedalusNakedCall();
      }

    if(ref->is_member()) {
      if(auto d = dynamic_cast<memory_instance*>(context.get())) {
        const ptr32_t ptr = d->address;
        vm.push_int(int32_t(ptr + ref->offset_as_member()));
        return zenkit::DaedalusNakedCall();
        }
      // a member of an item that the script holds as C_ITEM: its place in the memory of the handle is the member of oCItem
      if(dynamic_cast<zenkit::IItem*>(context.get())!=nullptr) {
        if(const ptr32_t handle = handles.pointerTo(context)) {
          const std::string_view full = ref->name();
          const auto dot = full.find('.');
          const std::string alias = "OCITEM." + std::string(dot==std::string_view::npos ? full : full.substr(dot+1));
          if(auto* a = vm.find_symbol_by_name(alias)) {
            vm.push_int(int32_t(handle + ptr32_t(a->offset_as_member())));
            return zenkit::DaedalusNakedCall();
            }
          }
        }
      Log::e("Ikarus: _takeref - unable take reference to struct element");
      vm.push_int(int32_t(0xBAD10000));
      return zenkit::DaedalusNakedCall();
      }

    const uint32_t id  = ref->index();
    const ptr32_t  ptr = varAddress(id,idx);
    if(false && ref->type()==zenkit::DaedalusDataType::STRING) {
      if(auto* s = mem32.deref<zString>(ptr))
        memAssignString(*s, ref->get_string(0));
      ref->set_instance(std::make_shared<memory_instance>(*this, ptr));
      }
    vm.push_int(int32_t(ptr));
    return zenkit::DaedalusNakedCall();
    }

  auto symbol = vm.pop_int();
  auto sym    = vm.find_symbol_by_index(uint32_t(symbol));
  if(sym==nullptr) {
    Log::e("Ikarus: _takeref - unable to resolve symbol");
    vm.push_int(int32_t(0xBAD10000));
    return zenkit::DaedalusNakedCall();
    }

  if(sym->type()==zenkit::DaedalusDataType::INSTANCE) {
    if(auto ptr = handles.pointerTo(sym->get_instance())) {
      vm.push_int(int32_t(ptr));
      return zenkit::DaedalusNakedCall();
      }
    auto inst = sym->get_instance().get();
    if(auto d = dynamic_cast<memory_instance*>(inst)) {
      const ptr32_t ptr = d->address;
      vm.push_int(int32_t(ptr));
      return zenkit::DaedalusNakedCall();
      }
    else if(inst!=nullptr) {
      // HACK: ScriptVar doesn't really have any backing of data
      const ptr32_t ptr = varAddress(inst->symbol_index());
      vm.push_int(int32_t(ptr));
      return zenkit::DaedalusNakedCall();
      }
    }

  if(sym->type()==zenkit::DaedalusDataType::STRING) {
    if(logBudget("takeref"))
      Log::e("Ikarus: _takeref - not a memory-instance: ", sym->name());
    vm.push_int(int32_t(0xBAD20000));
    return zenkit::DaedalusNakedCall();
    }

  if(logBudget("takeref"))
    Log::e("Ikarus: _takeref - not a memory-instance: ", sym->name());
  vm.push_int(int32_t(0xBAD20000));
  return zenkit::DaedalusNakedCall();
  }

std::shared_ptr<zenkit::DaedalusInstance> DirectMemory::mem_ptrtoinst(ptr32_t address) {
  if(address==0)
    Log::d("mem_ptrtoinst: address is null");
  if(handles.contains(address)) {
    if(auto inst = handles.instanceAt(address))
      return inst;
    }
  if(const int64_t sId = varSymbolAt(address); sId>=0) {
    // HACK: need to be consistent with _takeref
    auto     sym = vm.find_symbol_by_index(uint32_t(sId));
    if(sym!=nullptr && sym->type()==zenkit::DaedalusDataType::INSTANCE)
      return sym->get_instance();
    }
  return std::make_shared<memory_instance>(*this, address);
  }

int DirectMemory::mem_insttoptr(int index) {
  auto* sym  = vm.find_symbol_by_index(uint32_t(index));
  if(sym==nullptr || sym->type() != zenkit::DaedalusDataType::INSTANCE) {
    Log::e("MEM_InstToPtr: Invalid instance: ", index);
    return 0;
    }

  const std::shared_ptr<zenkit::DaedalusInstance>& inst = sym->get_instance();
  if(auto mem = dynamic_cast<memory_instance*>(inst.get())) {
    return int32_t(mem->address);
    }
  if(auto ptr = handles.pointerTo(inst))
    return int32_t(ptr);

  // Log::d("MEM_InstToPtr: not a memory instance");
  // HACK: ScriptVar doesn't really have any backing of data
  const ptr32_t ptr = varAddress(inst->symbol_index());
  return int32_t(ptr);
  }

/// A script builds the text of a string variable in its own buffer and fills it after it has set the length: the variable
/// of the VM gets the text when the buffer is written.
void DirectMemory::syncOwnedStrings(ptr32_t address, uint32_t size) {
  for(size_t i=0; i<ownedStrings.size(); ++i) {
    const uint32_t slot  = ownedStrings[i];
    auto it = strVars.find(slot);
    if(it==strVars.end())
      continue;
    auto& st = it->second;
    if(st.zlen<=0 || st.zptr==0 || address+size<=st.zptr || address>=st.zptr+uint32_t(st.zlen))
      continue;
    auto* chr = reinterpret_cast<const char*>(mem32.deref(st.zptr,uint32_t(st.zlen)));
    if(chr==nullptr)
      continue;
    std::string dst(chr,size_t(st.zlen));
    if(dst!=st.synced) {
      if(slot<varSlotSym.size()) {
        const uint32_t index = varSlotSym[slot];
        if(auto* sym = vm.find_symbol_by_index(index))
          sym->set_string(dst,uint16_t(slot-varSlotFirst[index]));
        }
      st.synced = dst;
      }
    }
  }

int DirectMemory::mem_readint(int address) {
  if(uint32_t(address)<0x1000 && logBudget("nullpage",4)) {
    Log::e("DMA: MEM_ReadInt of the address ",address,", the stack of the script:");
    vm.print_stack_trace();
    }
  return mem32.readInt(ptr32_t(address));
  }

void DirectMemory::mem_writeint(int address, int val) {
  if(uint32_t(address)<0x1000 && logBudget("nullpage",4)) {
    auto sv = [this](const char* name) -> int {
      auto* sym = vm.find_symbol_by_name(name);
      return sym!=nullptr && sym->type()==zenkit::DaedalusDataType::INT ? sym->get_int() : -12345;
      };
    Log::e("DMA: (ASMINT_InternalStack ",sv("ASMINT_InternalStack")," of ",int(ASMINT_InternalStack),", walker ",
           sv("ASMINT_InternalStackWalker"),")");
    Log::e("DMA: MEM_WriteInt to the address ",address,", the stack of the script:");
    vm.print_stack_trace();
    }
  mem32.writeInt(uint32_t(address),val);
  if(!ownedStrings.empty())
    syncOwnedStrings(uint32_t(address),4);
  }

void DirectMemory::mem_copybytes(int src, int dst, int size) {
  mem32.copyBytes(ptr32_t(src),ptr32_t(dst),ptr32_t(size));
  if(!ownedStrings.empty() && size>0)
    syncOwnedStrings(ptr32_t(dst),uint32_t(size));
  }

std::string DirectMemory::mem_readstring(int address) {
  if(auto s = mem32.deref<const zString>(ptr32_t(address))) {
    std::string str;
    memFromString(str, *s);
    return str;
    }
  return "";
  }

zenkit::DaedalusNakedCall DirectMemory::mem_readstatarr(zenkit::DaedalusVm& vm) {
  const int  index = vm.pop_int();
  auto [ref, idx, context] = vm.pop_reference();

  const int ret = vm.get_int(context, ref, uint16_t(idx+index));

  vm.push_int(ret);
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::mem_writestatarr(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  const int  value = vm.pop_int();
  const int  index = vm.pop_int();
  auto [ref, idx, context] = vm.pop_reference();

  vm.set_int(context, ref, uint16_t(idx+index), value);
  return zenkit::DaedalusNakedCall();
  }

int DirectMemory::mem_alloc(int amount) {
  if(amount==0) {
    Log::d("alocation zero bytes");
    return 0;
    }
  auto ptr = mem32.alloc(uint32_t(amount));
  return int32_t(ptr);
  }

int DirectMemory::mem_alloc(int amount, const char* comment) {
  if(amount==0) {
    Log::d("alocation zero bytes");
    return 0;
    }
  auto ptr = mem32.alloc(uint32_t(amount), comment);
  return int32_t(ptr);
  }

void DirectMemory::mem_free(int ptr) {
  mem32.free(Mem32::ptr32_t(ptr));
  }

int DirectMemory::mem_realloc(int address, int oldsz, int size) {
  auto ptr = mem32.realloc(Mem32::ptr32_t(address), uint32_t(size));
  return int32_t(ptr);
  }


void DirectMemory::setupDirectFunctions() {
  vm.override_function("MEM_GetFuncIdByOffset", [this](int off) { return mem_getfuncidbyoffset(off); });
  vm.override_function("MEM_AssignInst",        [this](int index, int ptr) { mem_assigninst(index, ptr); });

  vm.override_function("MEM_CallByID",          [this](zenkit::DaedalusVm& vm) { return mem_callbyid(vm);         });
  vm.override_function("MEM_CallByPtr",         [this](zenkit::DaedalusVm& vm) { return mem_callbyptr(vm);        });

  vm.override_function("memint_stackpushint",   [this](zenkit::DaedalusVm& vm) { return memint_stackpushint (vm); });
  vm.override_function("memint_stackpushinst",  [this](zenkit::DaedalusVm& vm) { return memint_stackpushinst(vm); });
  vm.override_function("memint_stackpushvar",   [this](zenkit::DaedalusVm& vm) { return memint_stackpushvar (vm); });

  // The text of a zSTRING that a call of the engine has returned by value (the place was given by CALL_RetValIsZString): the
  // original takes it with the patched code of the parser (MEMINT_StrAssign), here it is read at once. The zSTRING is freed.
  vm.override_function("CALL_RetValAszString",[this]() -> std::string {
    auto* res = vm.find_symbol_by_name("CALLINT_RESULT");
    if(res==nullptr || res->type()!=zenkit::DaedalusDataType::INT)
      return std::string();
    const ptr32_t ptr = ptr32_t(res->get_int());
    std::string   ret;
    if(ptr!=0) {
      if(auto* z = mem32.deref<zString>(ptr)) {
        memFromString(ret,*z);
        mem32.free(z->ptr);
        }
      mem32.free(ptr);
      res->set_int(0);
      }
    return ret;
    });

  vm.override_function("memint_popstring",      [this](zenkit::DaedalusVm& vm) { return memint_popstring(vm);     });

  vm.override_function("mem_popintresult",      [this](zenkit::DaedalusVm& vm) { return mem_popintresult(vm);     });
  vm.override_function("mem_popstringresult",   [this](zenkit::DaedalusVm& vm) { return mem_popstringresult(vm);  });
  vm.override_function("mem_popinstresult",     [this](zenkit::DaedalusVm& vm) { return mem_popinstresult(vm);    });
  }

int DirectMemory::mem_getfuncidbyoffset(int off) {
  if(off==0) {
    // MEM_INITALL
    return 0;
    }
  // the function that contains this place of the code
  if(auto* sym = findSymbolByAddress(uint32_t(off)))
    return int(sym->index());
  if(logBudget("getfuncidbyoffset"))
    Log::e("mem_getfuncidbyoffset: there is no function at ", off);
  return -1;
  }

void DirectMemory::mem_assigninst(int index, int ptr) {
  auto* sym  = vm.find_symbol_by_index(uint32_t(index));
  if(sym==nullptr) {
    Log::e("MEM_AssignInst: Invalid instance: ",index);
    return;
    }
  sym->set_instance(std::make_shared<memory_instance>(*this, ptr32_t(ptr)));
  }

void DirectMemory::directCall(zenkit::DaedalusVm& vm, zenkit::DaedalusSymbol& func) {
  if(func.type()!=zenkit::DaedalusDataType::FUNCTION) {
    Log::e("Bad unsafe function call");
    return;
    }

  //vm.call_function(sym);
  //zenkit::StackGuard guard {&vm, func.rtype()};
  vm.unsafe_call(&func);
  }

zenkit::DaedalusNakedCall DirectMemory::mem_callbyid(zenkit::DaedalusVm& vm) {
  const uint32_t symbId = uint32_t(vm.pop_int());
  auto* sym = vm.find_symbol_by_index(symbId);
  if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::FUNCTION) {
    Log::e("MEM_CallByID: Provided symbol is not callable (not function, prototype or instance): ", symbId);
    return zenkit::DaedalusNakedCall();
    }
  directCall(vm, *sym);
  return zenkit::DaedalusNakedCall();
  }

zenkit::DaedalusNakedCall DirectMemory::mem_callbyptr(zenkit::DaedalusVm& vm) {
  //FIXME: map function into memory for real!
  const auto address = uint32_t(vm.pop_int());
  auto sym = vm.find_symbol_by_address(address);
  if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::FUNCTION) {
    char buf[16] = {};
    std::snprintf(buf, sizeof(buf), "%x", address);
    Log::e("MEM_CallByPtr: Provided symbol is not callable (not function, prototype or instance): 0x", buf);
    return zenkit::DaedalusNakedCall();
    }
  directCall(vm, *sym);
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::memint_stackpushint(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::memint_stackpushinst(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  int id = vm.pop_int();
  auto sym = vm.find_symbol_by_index(uint32_t(id));
  if(sym!=nullptr && sym->type()==zenkit::DaedalusDataType::INSTANCE) {
    vm.push_instance(sym->get_instance());
    return zenkit::DaedalusNakedCall();
    }
  vm.push_instance(nullptr);
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::memint_stackpushvar(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  //NOTE: works by accident with _AI_FUNCTION_EVENT for now
  const int ptr = vm.pop_int(); (void)ptr;
  Log::d("TODO: memint_stackpushvar");
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::memint_popstring(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::mem_popintresult(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::mem_popstringresult(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  return zenkit::DaedalusNakedCall();
  }

auto DirectMemory::mem_popinstresult(zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
  return zenkit::DaedalusNakedCall();
  }

void DirectMemory::setupMathFunctions() {
  vm.override_function("MKF",    [](int i) { return mkf(i); });
  vm.override_function("TRUNCF", [](int i) { return truncf(i); });
  vm.override_function("ROUNDF", [](int i) { return roundf(i); });
  vm.override_function("ADDF",   [](int a, int b) { return addf(a,b); });
  vm.override_function("SUBF",   [](int a, int b) { return subf(a,b); });
  vm.override_function("MULF",   [](int a, int b) { return mulf(a,b); });
  vm.override_function("DIVF",   [](int a, int b) { return divf(a,b); });
  }

int DirectMemory::mkf(int v) {
  return floatBitsToInt(float(v));
  }

int DirectMemory::truncf(int v) {
  float ret = intBitsToFloat(v);
  ret = std::truncf(ret);
  return int(ret); //floatBitsToInt(ret);
  }

int DirectMemory::roundf(int v) {
  float ret = intBitsToFloat(v);
  ret = std::roundf(ret);
  return int(ret); //floatBitsToInt(ret);
  }

int DirectMemory::addf(int ia, int ib) {
  float a = intBitsToFloat(ia);
  float b = intBitsToFloat(ib);
  return floatBitsToInt(a+b);
  }

int DirectMemory::subf(int ia, int ib) {
  float a = intBitsToFloat(ia);
  float b = intBitsToFloat(ib);
  return floatBitsToInt(a-b);
  }

int DirectMemory::mulf(int ia, int ib) {
  float a = intBitsToFloat(ia);
  float b = intBitsToFloat(ib);
  return floatBitsToInt(a*b);
  }

int DirectMemory::divf(int ia, int ib) {
  float a = intBitsToFloat(ia);
  float b = intBitsToFloat(ib);
  return floatBitsToInt(a/b);
  }

void DirectMemory::setupStringsFunctions() {
  // TODO: allow writes to mapped memory
  vm.override_function("STR_SubStr", [](std::string_view str, int start, int count) -> std::string {
    if(start < 0 || (count < 0)) {
      Log::e("STR_SubStr: start and count may not be negative.");
      return "";
      };

    if(size_t(start)>=str.size()) {
      Log::e("STR_SubStr: The desired start of the substring lies beyond the end of the string.");
      return "";
      };

    return std::string(str.substr(size_t(start), size_t(count)));
    });

  //NOTE: native implementation requires coherency protocol
  // const ptr32_t ZSTRING__UPPER_G2 = 4631296;
  vm.override_function("STR_Upper", [](std::string_view str){
    std::string s = std::string(str);
    for(auto& c:s)
      c = char(std::toupper(c));
    return s;
    });

  // LeGo immplementation requires 'rw' access to 'callback memory'
  vm.override_function("SB_TOSTRING", [this]() -> std::string {
    struct StringBuilder {
      ptr32_t ptr;
      int     cln;
      int     CAL;
      };
    const auto _SB_CURRENT = this->vm.find_symbol_by_name("_SB_CURRENT");
    if(_SB_CURRENT==nullptr || _SB_CURRENT->type()!=zenkit::DaedalusDataType::INT)
      return "";

    auto text = mem32.deref<StringBuilder>(ptr32_t(_SB_CURRENT->get_int()));
    if(text->cln<=0 || text->ptr==0)
      return "";
    auto cstr = reinterpret_cast<const char*>(mem32.deref(text->ptr, uint32_t(text->cln)));
    return std::string(cstr, size_t(text->cln));
    });

  const ptr32_t STR_FROMCHAR = 4198592;
  cpu.register_thiscall(STR_FROMCHAR, [this](ptr32_t self, ptr32_t pchr) {
    const int64_t id = varSymbolAt(self);
    if(id<0)
      return; // error

    const auto     sx = vm.find_symbol_by_index(uint32_t(id));
    if(sx==nullptr || sx->type()!=zenkit::DaedalusDataType::STRING)
      return;
    const uint16_t el = uint16_t((self-scriptVariables)/ptr32_t(sizeof(ScriptVar)) - varSlotFirst[size_t(id)]);

    auto [ptr, size] = mem32.deref(pchr);
    const char* chr = reinterpret_cast<const char*>(ptr);
    if(chr==nullptr || size==0) {
      sx->set_string("",el);
      return;
      }
    size_t strsz = 0;
    while(chr[strsz] && strsz<size)
      ++strsz;
    sx->set_string(std::string(chr, strsz),el);
    });
  }

void DirectMemory::setupWinapiFunctions() {
  const ptr32_t WINAPI__LOADLIBRARY_ptr    = 8577604;
  const ptr32_t WINAPI__GETPROCADDRESS_ptr = 8577688;
  mem32.alloc(WINAPI__LOADLIBRARY_ptr,    4, "WINAPI__LOADLIBRARY*");
  mem32.alloc(WINAPI__GETPROCADDRESS_ptr, 4, "WINAPI__GETPROCADDRESS*");

  const ptr32_t WINAPI__LOADLIBRARY = mem32.alloc(4);
  mem32.writeInt(WINAPI__LOADLIBRARY_ptr, int32_t(WINAPI__LOADLIBRARY));

  const ptr32_t WINAPI__GETPROCADDRESS = mem32.alloc(4);
  mem32.writeInt(WINAPI__GETPROCADDRESS_ptr, int32_t(WINAPI__GETPROCADDRESS));

  cpu.register_stdcall(WINAPI__LOADLIBRARY, [this](ptr32_t pname) {
    auto [ptr, size] = mem32.deref(pname);
    auto name = std::string_view(reinterpret_cast<const char*>(ptr), size_t(size));
    Log::d("suppress LoadLibrary: ", name);
    return 0x1;
    });

  cpu.register_stdcall(WINAPI__GETPROCADDRESS, [this](ptr32_t module, ptr32_t pname) {
    auto [ptr, size] = mem32.deref(pname);
    auto name = std::string_view(reinterpret_cast<const char*>(ptr), size_t(size));
    Log::d("suppress GetProcAddress: ",name);
    return 0x1;
    });

  const ptr32_t GETUSERNAMEA = 8080162;
  cpu.register_stdcall(GETUSERNAMEA, [this](ptr32_t lpBuffer, ptr32_t pcbBuffer){
    std::string_view uname = "OpenGothic";

    const uint32_t max = pcbBuffer ? uint32_t(mem32.readInt(pcbBuffer)) : 0;
    if(auto ptr = reinterpret_cast<char*>(mem32.derefv(lpBuffer, max))) {
      if(max>=uname.size()) {
        std::strncpy(reinterpret_cast<char*>(ptr), "OpenGothic", max);
        ptr[uname.size()] = '\0';
        return 1;
        }
      }
    // buffer is too small
    mem32.writeInt(pcbBuffer, int32_t(uname.size()+1));
    return -1;
    });

  const ptr32_t GETLOCALTIME = 8079184;
  cpu.register_stdcall(GETLOCALTIME, [this](ptr32_t lpSystemTime) {
    struct SystemTime {
      uint16_t wYear;
      uint16_t wMonth;
      uint16_t wDayOfWeek;
      uint16_t wDay;
      uint16_t wHour;
      uint16_t wMinute;
      uint16_t wSecond;
      uint16_t wMilliseconds;
      };

    if(auto ptr = mem32.deref<SystemTime>(lpSystemTime)) {
      const auto        timePoint = std::chrono::system_clock::now();
      const std::time_t time      = std::chrono::system_clock::to_time_t(timePoint);
      const auto        t         = std::localtime(&time);
      const auto        ms        = std::chrono::time_point_cast<std::chrono::milliseconds>(timePoint);

      ptr->wYear         = uint16_t(t->tm_year + 1900);
      ptr->wMonth        = uint16_t(t->tm_mon + 1);
      ptr->wDayOfWeek    = uint16_t(t->tm_wday);
      ptr->wDay          = uint16_t(t->tm_mday);
      ptr->wHour         = uint16_t(t->tm_hour);
      ptr->wMinute       = uint16_t(t->tm_min);
      ptr->wSecond       = uint16_t(t->tm_sec);
      ptr->wMilliseconds = uint16_t(ms.time_since_epoch().count()%1000);
      }
    });
  }

void DirectMemory::setupUtilityFunctions() {
  static auto generateCrcTable = []() {
    std::array<uint32_t, 256> table;
    uint32_t polynomial = 0xEDB88320;
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i;
      for (size_t j = 0; j < 8; j++) {
        if(c & 1) {
          c = polynomial ^ (c >> 1);
          } else {
          c >>= 1;
          }
        }
      table[i] = c;
      }
    return table;
    };

  const ptr32_t GETBUFFERCRC32_G2 = 6265360;
  cpu.register_cdecl(GETBUFFERCRC32_G2, [this](ptr32_t buf, int bufLen, int unused) {
    if(buf==0 || bufLen<=0)
      return 0;

    const auto  ptr   = reinterpret_cast<const uint8_t*>(mem32.deref(buf,uint32_t(bufLen)));
    static auto table = generateCrcTable();

    uint32_t initial = 0;
    uint32_t c       = initial ^ 0xFFFFFFFF;
    for(int i = 0; i < bufLen; ++i) {
      c = table[(c ^ ptr[i]) & 0xFF] ^ (c >> 8);
      }
    return int32_t(c ^ 0xFFFFFFFF);
    });

  const ptr32_t QSORT_G2 = 8195951;
  cpu.register_thiscall(QSORT_G2, [](ptr32_t base, int numElt, int eltSize, ptr32_t pCmp) {
    Log::e("LeGo: TODO: QSORT_G2(...)");
    });

  const ptr32_t ZERROR__SETTARGET = 4513616;
  cpu.register_thiscall(ZERROR__SETTARGET, [](ptr32_t, int) {
    // nop
    });

  const int SYSGETTIMEPTR_G2 = 5264000;
  cpu.register_cdecl(SYSGETTIMEPTR_G2, [this](){
    return uint32_t(gameScript.tickCount());
    });

  // functions of C, the parameters are in the order of the declaration
  const ptr32_t MEMSET_G2 = 8243856;
  cpu.register_cdecl(MEMSET_G2, [this](ptr32_t dst, int value, int size) {
    if(size>0)
      if(auto p = reinterpret_cast<uint8_t*>(mem32.derefv(dst,uint32_t(size))))
        std::memset(p,value,size_t(size));
    return int32_t(dst);
    });

  const ptr32_t SWAP_G2 = 8196369;
  cpu.register_cdecl(SWAP_G2, [this](ptr32_t a, ptr32_t b, int size) {
    if(size<=0)
      return;
    auto pa = reinterpret_cast<uint8_t*>(mem32.derefv(a,uint32_t(size)));
    auto pb = reinterpret_cast<uint8_t*>(mem32.derefv(b,uint32_t(size)));
    if(pa!=nullptr && pb!=nullptr)
      std::swap_ranges(pa,pa+size,pb);
    });

  // math: the result is in the register of the floating point
  const ptr32_t SINF_G2   = 8123910;
  const ptr32_t ACOSF_G2  = 8123794;
  const ptr32_t ATAN2F_G2 = 8123804;
  const ptr32_t STRNCMP_G2 = 8254144;
  cpu.register_cdecl(STRNCMP_G2, [this](ptr32_t a, ptr32_t b, int n) -> int32_t {
    auto read = [this](ptr32_t p) -> std::string {
      auto [ptr, size] = mem32.deref(p);
      if(ptr==nullptr)
        return std::string();
      return std::string(reinterpret_cast<const char*>(ptr),strnlen(reinterpret_cast<const char*>(ptr),size_t(size)));
      };
    const auto sa = read(a);
    const auto sb = read(b);
    return int32_t(std::strncmp(sa.c_str(),sb.c_str(),size_t(std::max(n,0))));
    });

  cpu.register_cdecl(SINF_G2,   [](float x)          { return std::sin(x);      });
  cpu.register_cdecl(ACOSF_G2,  [](float x)          { return std::acos(x);     });
  cpu.register_cdecl(ATAN2F_G2, [](float y, float x) { return std::atan2(y,x); });

  // the box of the message: it goes to the log
  const ptr32_t WINAPI__MESSAGEBOX_G2 = 8079592;
  cpu.register_stdcall(WINAPI__MESSAGEBOX_G2, [this](int, ptr32_t txt, ptr32_t caption, int) {
    auto text = [this](ptr32_t p) {
      auto [ptr, size] = mem32.deref(p);
      return ptr==nullptr ? std::string() : std::string(reinterpret_cast<const char*>(ptr),strnlen(reinterpret_cast<const char*>(ptr),size_t(size)));
      };
    Log::i("[message box] ", text(caption), ": ", text(txt));
    return 1; // IDOK
    });
  }

void DirectMemory::setupHookEngine() {
  vm.override_function("HookEngineI", [this](int address, int oldInstr, zenkit::DaedalusFunction function){
    auto sym  = function.value;
    auto name = sym==nullptr ? "" : sym->name().c_str();
    // the frame and the events of the animations are answered by tick and eventPlayAni
    if(std::strcmp(name,"_FF_Hook")==0 || std::strcmp(name,"_AI_FUNCTION_EVENT")==0)
      return;
    if(sym!=nullptr) {
      auto& list = hooks[uint32_t(address)];
      if(std::find(list.begin(),list.end(),sym->index())==list.end())
        list.push_back(sym->index());
      }
    if(isHookAnswered(uint32_t(address)))
      return;
    if(hooksUnanswered.size()<1000)
      hooksUnanswered.emplace_back(uint32_t(address),name);
    Log::d("not implemented call [HookEngineI] (", reinterpret_cast<void*>(uint64_t(address)),
           " -> ", name, ")");
    });

  const ptr32_t ZCCONSOLE__REGISTER = 7875296;
  cpu.register_thiscall(ZCCONSOLE__REGISTER, [](ptr32_t, ptr32_t pcmd, ptr32_t pdescr) {
    (void)pcmd;
    (void)pdescr;
    // auto sym  = func.value;
    // auto name = sym==nullptr ? "" : sym->name().c_str();
    // Log::e("not implemented call [ZCCONSOLE__REGISTER] (", prefix, " -> ", name, ")");
    });

  // PermMem
  vm.override_function("LOCALS", [this](zenkit::DaedalusVm& vm) -> zenkit::DaedalusNakedCall {
    auto sym = findSymbolByAddress(vm.pc());
    if(sym!=nullptr) {
      auto sx = vm.find_symbol_by_index(sym->index());
      sx->set_local_variables_enable(true);
      }
    return zenkit::DaedalusNakedCall();
    });
  vm.override_function("FINAL", []() {
    Log::e("LeGo: 'final' is not implemented");
    return 0;
    });
  }


void DirectMemory::setupzCParserFunctions() {
  const ptr32_t ZCPARSER__GETINDEX_G2 = 7943280;
  cpu.register_thiscall(ZCPARSER__GETINDEX_G2, [this](ptr32_t, std::string sym) {
    if(auto s = vm.find_symbol_by_name(sym))
      return int32_t(s->index());
    return -1;
    });

  const ptr32_t ZCPARSER__CREATEINSTANCE = 7942048;
  cpu.register_thiscall(ZCPARSER__CREATEINSTANCE, [this](ptr32_t, int32_t instId, ptr32_t ptr){
    auto *sym = vm.find_symbol_by_index(uint32_t(instId));
    auto *cls = sym;
    if(sym != nullptr && sym->type() == zenkit::DaedalusDataType::INSTANCE) {
      cls = vm.find_symbol_by_index(sym->parent());
      }

    if(sym==nullptr || cls == nullptr) {
      Log::e("LeGo::createInstance invalid symbold id (", instId, ")");
      return 0;
      }

    auto inst = std::make_shared<memory_instance>(*this, ptr);

    auto self = vm.find_symbol_by_name("SELF");
    auto prevSelf = self != nullptr ? self->get_instance() : nullptr;
    auto prevGi   = vm.unsafe_get_gi();
    if(self!=nullptr)
      self->set_instance(inst);

    vm.unsafe_set_gi(inst);
    vm.unsafe_call(sym);

    vm.unsafe_set_gi(prevGi);
    if(self!=nullptr)
      self->set_instance(prevSelf);

    static std::unordered_map<std::string,int> traced;
    if(traced.size()<40 && ++traced[cls->name()]<=2) {
      const int32_t* w = mem32.deref<const int32_t>(ptr);
      Log::i("DMA: zCParser::CreateInstance ", sym->name(), " (class ", cls->name(), ", ", cls->class_size(),
             " bytes) at ", reinterpret_cast<void*>(uint64_t(ptr)), ": ",
             w==nullptr ? 0 : w[0], " ", w==nullptr ? 0 : w[1], " ", w==nullptr ? 0 : w[2]);
      }

    sym->set_instance(inst);
    return int(ptr); // no idea, what return-value suppose to be - unused in LeGo
    });
  }

void DirectMemory::setupInitFileFunctions() {
  vm.override_function("MEM_GetGothOpt", [](std::string_view sec, std::string_view opt) {
    return std::string(Gothic::inst().settingsGetS(sec, opt));
    });
  // the ini of the mod: the file that the parameter -game names
  vm.override_function("MEM_GetModOpt", [](std::string_view sec, std::string_view opt) {
    return std::string(Gothic::modSettingsGetS(sec,opt));
    });
  vm.override_function("MEM_GothOptSectionExists", [](std::string_view sec) {
    return Gothic::settingsHasSection(sec);
    });
  vm.override_function("MEM_GothOptExists", [](std::string_view sec, std::string_view opt) {
    if(sec=="INTERNAL" && opt=="UnionActivated") {
      // Fake Union, so ikarus won't setup crazy workarounds for unpatched G2.
      return true;
      }
    return Gothic::settingsHas(sec,opt);
    });
  vm.override_function("MEM_ModOptSectionExists", [](std::string_view sec) {
    return Gothic::modSettingsHasSection(sec);
    });
  vm.override_function("MEM_ModOptExists", [](std::string_view sec, std::string_view opt) {
    return Gothic::modSettingsHas(sec,opt);
    });
  vm.override_function("MEM_SetGothOpt", [](std::string_view sec, std::string_view opt, std::string_view v) {
    // kept in Gothic.ini, so it stays between the games
    Gothic::settingsSetS(sec,opt,v);
    });
  }

void DirectMemory::setupNpcFunctions() {
  const ptr32_t OCNPC__GETSLOTITEM = 7544720;
  cpu.register_thiscall(OCNPC__GETSLOTITEM, [](ptr32_t pHero, std::string slot) {
    Log::e("LeGo: OCNPC__GETSLOTITEM(", slot, ")");
    return 0x0;
    });

  const ptr32_t OCNPC__EQUIPWEAPON = 7577648;
  cpu.register_thiscall(OCNPC__EQUIPWEAPON, [](ptr32_t pHero, ptr32_t pItem) {
    Log::e("LeGo: OCNPC__EQUIPWEAPON(", pItem, ")");
    });
  }

static bool equalsNoCase(std::string_view a, std::string_view b) {
  if(a.size()!=b.size())
    return false;
  for(size_t i=0; i<a.size(); ++i)
    if(std::toupper(static_cast<unsigned char>(a[i]))!=std::toupper(static_cast<unsigned char>(b[i])))
      return false;
  return true;
  }

/// The movers of the world are mirrored as zCMover objects: the scripts of mods find them by the name of the vob
/// (MEM_SearchVobByName) and wait for the end of the movement by reading moverState.
auto DirectMemory::report() -> std::string {
  char buf[256];
  std::string ret;
  std::snprintf(buf,sizeof(buf),"memory: %zu regions, %llu bytes\n",mem32.regionCount(),
                static_cast<unsigned long long>(mem32.bytesInUse()));
  ret += buf;

  buildMoverPool();
  std::snprintf(buf,sizeof(buf),"movers: %zu\n",movers.list.size());
  ret += buf;
  for(size_t i=0; i<movers.list.size() && i<200; ++i) {
    std::snprintf(buf,sizeof(buf),"  mover %s state %d\n",std::string(movers.list[i]->name()).c_str(),movers.list[i]->moverState());
    ret += buf;
    }

  std::snprintf(buf,sizeof(buf),"hooks that nobody answers: %zu\n",hooksUnanswered.size());
  ret += buf;
  for(auto& h:hooksUnanswered) {
    std::snprintf(buf,sizeof(buf),"  0x%x %s\n",h.first,h.second.c_str());
    ret += buf;
    }
  return ret;
  }

void DirectMemory::buildMoverPool() {
  auto& world = gameScript.world();
  if(movers.world==&world)
    return;

  if(movers.base!=0)
    mem32.free(movers.base);
  movers = MoverPool();
  movers.world = &world;

  world.forEachTrigger([this](AbstractTrigger& t) {
    if(auto* mv = dynamic_cast<MoveTrigger*>(&t))
      movers.list.push_back(mv);
    });
  if(movers.list.empty())
    return;

  movers.base = mem32.alloc(uint32_t(movers.list.size()*sizeof(zCMover)), Mem32::Type::zCMover, "zCMover");
  }

void DirectMemory::memoryCallback(zCMover& m, uint32_t index, std::memory_order ord) {
  // the state and the position of a mover are up to the engine, what a script writes is lost
  if(index>=movers.list.size() || ord==std::memory_order::release)
    return;

  const int32_t ZCMOVER_VTBL_G2 = 8627324;
  auto*         mv  = movers.list[index];
  m._VTBL      = ZCMOVER_VTBL_G2;
  m.MOVERSTATE = mv->moverState();
  if(m.OBJECTNAME.ptr==0)
    memAssignString(m.OBJECTNAME,mv->name());

  // the matrix of the world: the position of the mover, the rotation is not mirrored
  const auto pos = mv->position();
  for(float& v:m.TRAFOOBJTOWORLD)
    v = 0;
  m.TRAFOOBJTOWORLD[0] = m.TRAFOOBJTOWORLD[5] = m.TRAFOOBJTOWORLD[10] = m.TRAFOOBJTOWORLD[15] = 1;
  m.TRAFOOBJTOWORLD[3]  = pos.x;
  m.TRAFOOBJTOWORLD[7]  = pos.y;
  m.TRAFOOBJTOWORLD[11] = pos.z;
  }

auto DirectMemory::searchVobsByName(std::string_view name) -> std::vector<ptr32_t> {
  std::vector<ptr32_t> ret;
  buildMoverPool();
  for(size_t i=0; i<movers.list.size(); ++i) {
    if(equalsNoCase(movers.list[i]->name(),name))
      ret.push_back(movers.base+ptr32_t(i*sizeof(zCMover)));
    }
  return ret;
  }

void DirectMemory::setupWorldFunctions() {
  const ptr32_t OCWORLD__SEARCHVOBBYNAME_G2 = 7865872;
  cpu.register_thiscall(OCWORLD__SEARCHVOBBYNAME_G2, [this](ptr32_t, std::string vob) -> ptr32_t {
    auto found = searchVobsByName(vob);
    if(found.empty()) {
      Log::d("Ikarus: MEM_SearchVobByName: there is no vob ", vob);
      return 0;
      }
    return found.front();
    });

  // fills zCArray<zCVob*>, returns the number of vobs
  const ptr32_t OCWORLD__SEARCHVOBLISTBYNAME_G2 = 7866048;
  cpu.register_thiscall(OCWORLD__SEARCHVOBLISTBYNAME_G2, [this](ptr32_t, std::string vob, ptr32_t pOutArr) -> int32_t {
    auto found = searchVobsByName(vob);
    auto* arr  = mem32.deref<zCArray>(pOutArr);
    if(arr==nullptr || found.empty())
      return 0;

    const ptr32_t buf = mem32.alloc(uint32_t(found.size()*sizeof(ptr32_t)), "zCArray<zCVob*>");
    arr = mem32.deref<zCArray>(pOutArr);   // the memory can move
    if(buf==0 || arr==nullptr)
      return 0;
    for(size_t i=0; i<found.size(); ++i)
      mem32.writeInt(buf+ptr32_t(i*sizeof(ptr32_t)),int32_t(found[i]));
    arr->ptr        = buf;
    arr->numAlloc   = int32_t(found.size());
    arr->numInArray = int32_t(found.size());
    return int32_t(found.size());
    });
  }

/// MEM_World.voblist_npcs: the characters of the world as a list, every node has the pointer of a character
void DirectMemory::refreshNpcList() {
  auto& world = gameScript.world();

  std::vector<ptr32_t> ptrs;
  for(uint32_t i=0, n=world.npcCount(); i<n; ++i) {
    auto* npc = world.npcById(i);
    if(npc==nullptr || npc->handlePtr()==nullptr)
      continue;
    if(auto ptr = handles.pointerTo(npc->handlePtr()))
      ptrs.push_back(ptr);
    }

  ptr32_t list = 0;
  if(!ptrs.empty()) {
    list = mem32.alloc(uint32_t(ptrs.size()*sizeof(zCListSort)),"zCListSort<oCNpc>");
    for(size_t i=0; list!=0 && i<ptrs.size(); ++i) {
      auto* node = mem32.deref<zCListSort>(list+ptr32_t(i*sizeof(zCListSort)));
      if(node==nullptr)
        break;
      node->compareFunc = 0;
      node->data        = ptrs[i];
      node->next        = i+1<ptrs.size() ? list+ptr32_t((i+1)*sizeof(zCListSort)) : 0;
      }
    }

  if(auto* w = mem32.deref<oWorld>(memGame._ZCSESSION_WORLD))
    w->VOBLIST_NPCS = list;
  mem32.free(npcList);
  npcList = list;
  }

/// The members of oCNpc and oCItem (the classes of Ikarus) are the members of C_NPC and C_ITEM of the VM, so a
/// script that reads npc.attribute or item.value through a pointer reads the real character or item.
void DirectMemory::setupInstanceAliases() {
  auto reg = [this](std::string_view name, auto field) {
    try {
      vm.register_member(name,field);
      }
    catch(const std::exception&) {
      // the mod has not such a member
      }
    };

  using N = zenkit::INpc;
  reg("OCNPC.IDX",             &N::id);
  reg("OCNPC.NAME",            &N::name);
  reg("OCNPC.SLOT",            &N::slot);
  reg("OCNPC.EFFECT",          &N::effect);
  reg("OCNPC.NPCTYPE",         &N::type);
  reg("OCNPC.VARIOUSFLAGS",    &N::flags);
  reg("OCNPC.ATTRIBUTE",       &N::attribute);
  reg("OCNPC.HITCHANCE",       &N::hitchance);
  reg("OCNPC.PROTECTION",      &N::protection);
  reg("OCNPC.DAMAGE",          &N::damage);
  reg("OCNPC.DAMAGETYPE",      &N::damage_type);
  reg("OCNPC.GUILD",           &N::guild);
  reg("OCNPC.LEVEL",           &N::level);
  // the flags of the character (movlock, drunk, ...): the five ints of the missions are not used by the game
  reg("OCNPC.BITFIELD",        &N::mission);
  reg("OCNPC.FIGHTTACTIC",     &N::fight_tactic);
  reg("OCNPC.FMODE",           &N::weapon);
  reg("OCNPC.VOICE",           &N::voice);
  // the members that C_NPC does not have: the pointer to the vob in focus (DirectMemory::eventFocus) and the mob that
  // the character uses. They have their own place, VOICEPITCH and SPAWNDELAY of the scripts are left alone.
  reg("OCNPC.FOCUS_VOB",       &N::compat_focus_vob);
  reg("OCNPC.INTERACTMOB",     &N::compat_interact_mob);
  reg("OCNPC.MASS",            &N::body_mass);
  reg("OCNPC.DAILY_ROUTINE",   &N::daily_routine);
  reg("OCNPC.STARTAISTATE",    &N::start_aistate);
  reg("OCNPC.SPAWNPOINT",      &N::spawnpoint);
  reg("OCNPC.SENSES",          &N::senses);
  reg("OCNPC.SENSES_RANGE",    &N::senses_range);
  reg("OCNPC.AISCRIPTVARS",    &N::aivar);
  reg("OCNPC.WPNAME",          &N::wp);
  reg("OCNPC.EXPERIENCE_POINTS",           &N::exp);
  reg("OCNPC.EXPERIENCE_POINTS_NEXT_LEVEL",&N::exp_next);
  reg("OCNPC.LEARN_POINTS",    &N::lp);
  reg("OCNPC.BODYSTATEINTERRUPTABLEOVERRIDE",&N::bodystate_interruptable_override);
  reg("OCNPC.NOFOCUS",         &N::no_focus);

  using I = zenkit::IItem;
  reg("OCITEM.ID",             &I::id);
  reg("OCITEM.NAME",           &I::name);
  reg("OCITEM.NAMEID",         &I::name_id);
  reg("OCITEM.HP",             &I::hp);
  reg("OCITEM.HP_MAX",         &I::hp_max);
  reg("OCITEM.MAINFLAG",       &I::main_flag);
  reg("OCITEM.FLAGS",          &I::flags);
  reg("OCITEM.WEIGHT",         &I::weight);
  reg("OCITEM.VALUE",          &I::value);
  reg("OCITEM.DAMAGETYPE",     &I::damage_type);
  reg("OCITEM.DAMAGETOTAL",    &I::damage_total);
  reg("OCITEM.DAMAGE",         &I::damage);
  reg("OCITEM.WEAR",           &I::wear);
  reg("OCITEM.PROTECTION",     &I::protection);
  reg("OCITEM.NUTRITION",      &I::nutrition);
  reg("OCITEM.COND_ATR",       &I::cond_atr);
  reg("OCITEM.COND_VALUE",     &I::cond_value);
  reg("OCITEM.CHANGE_ATR",     &I::change_atr);
  reg("OCITEM.CHANGE_VALUE",   &I::change_value);
  reg("OCITEM.MAGIC",          &I::magic);
  reg("OCITEM.ON_EQUIP",       &I::on_equip);
  reg("OCITEM.ON_UNEQUIP",     &I::on_unequip);
  reg("OCITEM.ON_STATE",       &I::on_state);
  reg("OCITEM.OWNER",          &I::owner);
  reg("OCITEM.OWNERGUILD",     &I::owner_guild);
  reg("OCITEM.DISGUISEGUILD",  &I::disguise_guild);
  reg("OCITEM.VISUAL",         &I::visual);
  reg("OCITEM.VISUAL_CHANGE",  &I::visual_change);
  reg("OCITEM.EFFECT",         &I::effect);
  reg("OCITEM.VISUAL_SKIN",    &I::visual_skin);
  reg("OCITEM.SCEMENAME",      &I::scheme_name);
  reg("OCITEM.MATERIAL",       &I::material);
  reg("OCITEM.MUNITION",       &I::munition);
  reg("OCITEM.SPELL",          &I::spell);
  reg("OCITEM.RANGE",          &I::range);
  reg("OCITEM.MAG_CIRCLE",     &I::mag_circle);
  reg("OCITEM.DESCRIPTION",    &I::description);
  reg("OCITEM.TEXT",           &I::text);
  reg("OCITEM.COUNT",          &I::count);
  reg("OCITEM.INV_ZBIAS",      &I::inv_zbias);
  reg("OCITEM.INV_ROTX",       &I::inv_rot_x);
  reg("OCITEM.INV_ROTY",       &I::inv_rot_y);
  reg("OCITEM.INV_ROTZ",       &I::inv_rot_z);
  reg("OCITEM.INV_ANIMATE",    &I::inv_animate);
  }
