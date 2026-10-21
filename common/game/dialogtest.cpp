#include "dialogtest.h"

#include <zenkit/DaedalusVm.hh>
#include <zenkit/addon/daedalus.hh>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <exception>
#include <fstream>
#include <functional>
#include <optional>
#include <set>
#include <sstream>
#include <unordered_map>

#include "game/gamescript.h"
#include "utils/gamelog.h"
#include "world/objects/interactive.h"
#include "world/objects/npc.h"
#include "world/waypoint.h"
#include "world/world.h"
#include "gothic.h"

namespace {

using zenkit::DaedalusOpcode;
using zenkit::DaedalusDataType;
using zenkit::DaedalusSymbol;

template<class T>
struct ScopeInst final {
  ScopeInst(DaedalusSymbol& sym, const std::shared_ptr<T>& h) : prev(sym.get_instance()), sym(sym) { sym.set_instance(h); }
  ScopeInst(const ScopeInst&) = delete;
  ~ScopeInst() { sym.set_instance(prev); }

  std::shared_ptr<zenkit::DaedalusInstance> prev;
  DaedalusSymbol&                           sym;
  };

std::string upper(std::string_view s) {
  std::string r(s);
  for(auto& c:r)
    c = char(std::toupper(static_cast<unsigned char>(c)));
  return r;
  }

/// The texts of the scripts of the Polish mods are in Windows-1250 (and the log has them as they are); the menu wants UTF-8.
std::string u8(const std::string& in) {
  bool ascii = true, valid = true;
  for(size_t i=0; i<in.size() && valid; ++i) {
    const unsigned char c = static_cast<unsigned char>(in[i]);
    if(c<0x80)
      continue;
    ascii = false;
    const size_t n = (c>=0xF0) ? 3 : (c>=0xE0) ? 2 : (c>=0xC0) ? 1 : 0;
    if(n==0) {
      valid = false;
      break;
      }
    for(size_t k=1; k<=n; ++k)
      if(i+k>=in.size() || (static_cast<unsigned char>(in[i+k])&0xC0)!=0x80) {
        valid = false;
        break;
        }
    i += n;
    }
  if(ascii || valid)
    return in;
  static const uint16_t hi[128] = {
    0x20AC,0x0081,0x201A,0x0083,0x201E,0x2026,0x2020,0x2021,0x0088,0x2030,0x0160,0x2039,0x015A,0x0164,0x017D,0x0179,
    0x0090,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,0x0098,0x2122,0x0161,0x203A,0x015B,0x0165,0x017E,0x017A,
    0x00A0,0x02C7,0x02D8,0x0141,0x00A4,0x0104,0x00A6,0x00A7,0x00A8,0x00A9,0x015E,0x00AB,0x00AC,0x00AD,0x00AE,0x017B,
    0x00B0,0x00B1,0x02DB,0x0142,0x00B4,0x00B5,0x00B6,0x00B7,0x00B8,0x0105,0x015F,0x00BB,0x013D,0x02DD,0x013E,0x017C,
    0x0154,0x00C1,0x00C2,0x0102,0x00C4,0x0139,0x0106,0x00C7,0x010C,0x00C9,0x0118,0x00CB,0x011A,0x00CD,0x00CE,0x010E,
    0x0110,0x0143,0x0147,0x00D3,0x00D4,0x0150,0x00D6,0x00D7,0x0158,0x016E,0x00DA,0x0170,0x00DC,0x00DD,0x0162,0x00DF,
    0x0155,0x00E1,0x00E2,0x0103,0x00E4,0x013A,0x0107,0x00E7,0x010D,0x00E9,0x0119,0x00EB,0x011B,0x00ED,0x00EE,0x010F,
    0x0111,0x0144,0x0148,0x00F3,0x00F4,0x0151,0x00F6,0x00F7,0x0159,0x016F,0x00FA,0x0171,0x00FC,0x00FD,0x0163,0x02D9};
  std::string out;
  for(unsigned char c:in) {
    if(c<0x80) {
      out += char(c);
      continue;
      }
    const uint16_t u = hi[c-0x80];
    if(u<0x800) {
      out += char(0xC0|(u>>6));
      out += char(0x80|(u&0x3F));
      } else {
      out += char(0xE0|(u>>12));
      out += char(0x80|((u>>6)&0x3F));
      out += char(0x80|(u&0x3F));
      }
    }
  return out;
  }

bool containsNoCase(std::string_view text, std::string_view word) {
  if(word.empty())
    return true;
  auto it = std::search(text.begin(),text.end(),word.begin(),word.end(),[](char a, char b){
    return std::toupper(static_cast<unsigned char>(a))==std::toupper(static_cast<unsigned char>(b));
    });
  return it!=text.end();
  }

// --- the expressions of the symbolic execution of a function of the scripts

struct Expr;
using ExprP = std::shared_ptr<const Expr>;

struct Expr final {
  enum Kind : uint8_t { Const, Str, Var, Inst, Member, Call, Cmp, And, Or, Not, Arith, Opaque };
  Kind               kind  = Opaque;
  int32_t            value = 0;        // Const; Cmp: the operator (an opcode)
  uint32_t           sym   = 0;        // Var, Inst, Member (the member), Call (the function)
  uint32_t           inst  = 0;        // Member: the symbol of the instance
  int                index = 0;        // Var, Member: the index of the array
  std::string        text;             // Str, Opaque
  std::vector<ExprP> args;             // Call: the arguments; Cmp: both sides; And, Or: both; Not: the one
  };

ExprP mkConst(int32_t v) {
  auto e = std::make_shared<Expr>();
  e->kind  = Expr::Const;
  e->value = v;
  return e;
  }

ExprP mkOpaque(std::string_view t) {
  auto e = std::make_shared<Expr>();
  e->kind = Expr::Opaque;
  e->text = t;
  return e;
  }

ExprP mkBin(Expr::Kind k, ExprP a, ExprP b, int32_t op = 0) {
  auto e = std::make_shared<Expr>();
  e->kind  = k;
  e->value = op;
  e->args  = {std::move(a),std::move(b)};
  return e;
  }

ExprP mkNot(ExprP a) {
  auto e = std::make_shared<Expr>();
  e->kind = Expr::Not;
  e->args = {std::move(a)};
  return e;
  }

using Cons = std::vector<std::pair<ExprP,bool>>;

struct Path final {
  ExprP ret;     // the value that the function returns, null: nothing
  Cons  cons;    // what has to hold on the way
  };

int32_t opInvert(int32_t op) {
  switch(DaedalusOpcode(op)) {
    case DaedalusOpcode::LT:  return int32_t(DaedalusOpcode::GTE);
    case DaedalusOpcode::GT:  return int32_t(DaedalusOpcode::LTE);
    case DaedalusOpcode::LTE: return int32_t(DaedalusOpcode::GT);
    case DaedalusOpcode::GTE: return int32_t(DaedalusOpcode::LT);
    case DaedalusOpcode::EQ:  return int32_t(DaedalusOpcode::NEQ);
    case DaedalusOpcode::NEQ: return int32_t(DaedalusOpcode::EQ);
    default: return op;
    }
  }

/// a op b  ->  b op' a
int32_t opSwap(int32_t op) {
  switch(DaedalusOpcode(op)) {
    case DaedalusOpcode::LT:  return int32_t(DaedalusOpcode::GT);
    case DaedalusOpcode::GT:  return int32_t(DaedalusOpcode::LT);
    case DaedalusOpcode::LTE: return int32_t(DaedalusOpcode::GTE);
    case DaedalusOpcode::GTE: return int32_t(DaedalusOpcode::LTE);
    default: return op;
    }
  }

const char* opText(int32_t op) {
  switch(DaedalusOpcode(op)) {
    case DaedalusOpcode::LT:  return "<";
    case DaedalusOpcode::GT:  return ">";
    case DaedalusOpcode::LTE: return "<=";
    case DaedalusOpcode::GTE: return ">=";
    case DaedalusOpcode::EQ:  return "==";
    case DaedalusOpcode::NEQ: return "!=";
    default: return "?";
    }
  }

bool opHolds(int32_t op, int32_t a, int32_t b) {
  switch(DaedalusOpcode(op)) {
    case DaedalusOpcode::LT:  return a<b;
    case DaedalusOpcode::GT:  return a>b;
    case DaedalusOpcode::LTE: return a<=b;
    case DaedalusOpcode::GTE: return a>=b;
    case DaedalusOpcode::EQ:  return a==b;
    case DaedalusOpcode::NEQ: return a!=b;
    default: return false;
    }
  }

/// the value that makes "x op c" true
int32_t desiredFor(int32_t op, int32_t c) {
  switch(DaedalusOpcode(op)) {
    case DaedalusOpcode::LT:  return c-1;
    case DaedalusOpcode::GT:  return c+1;
    case DaedalusOpcode::LTE: return c;
    case DaedalusOpcode::GTE: return c;
    case DaedalusOpcode::EQ:  return c;
    case DaedalusOpcode::NEQ: return c==0 ? 1 : 0;
    default: return c;
    }
  }

}

struct DialogTest::Impl final {
  std::unordered_map<uint32_t,uint32_t> fnByAddr;
  bool                                  fnInit = false;
  std::map<std::string,std::pair<int,std::string>> problems;   // what the game lacks -> how many dialogs, one example
  };

namespace {

struct Env final {
  GameScript&         sc;
  zenkit::DaedalusVm& vm;
  DialogTest::Impl&   impl;
  };

DaedalusSymbol* symOf(Env& e, uint32_t index) {
  return e.vm.find_symbol_by_index(index);
  }

std::string symName(Env& e, uint32_t index) {
  auto* s = symOf(e,index);
  return s==nullptr ? "#"+std::to_string(index) : s->name();
  }

void initFunctions(Env& e) {
  if(e.impl.fnInit)
    return;
  e.impl.fnInit = true;
  for(auto& s:e.vm.symbols())
    if(s.type()==DaedalusDataType::FUNCTION && !s.is_external() && !s.is_member() && s.address()!=0 && s.address()!=uint32_t(-1))
      e.impl.fnByAddr.emplace(s.address(),s.index());
  }

/// The paths of a function of the scripts: the conditions on the way to each return.
struct Walker final {
  Env&                 env;
  const DaedalusSymbol& fn;
  std::vector<Path>    out;
  int                  steps = 0;

  void walk(uint32_t pc, std::vector<ExprP> st, Cons cons, uint32_t curInst) {
    auto pop = [&]() -> ExprP {
      if(st.empty())
        return mkOpaque("?");
      auto e = st.back();
      st.pop_back();
      return e;
      };

    while(steps++<5000 && out.size()<64) {
      auto ins = env.vm.instruction_at(pc);
      pc += ins.size;
      switch(ins.op) {
        case DaedalusOpcode::PUSHI:
          st.push_back(mkConst(ins.immediate));
          break;
        case DaedalusOpcode::PUSHVI: {
          auto e = std::make_shared<Expr>();
          e->kind = Expr::Inst;
          e->sym  = ins.symbol;
          st.push_back(e);
          break;
          }
        case DaedalusOpcode::GMOVI:
          curInst = ins.symbol;
          break;
        case DaedalusOpcode::PUSHV:
        case DaedalusOpcode::PUSHVV: {
          auto* s = symOf(env,ins.symbol);
          const int idx = ins.op==DaedalusOpcode::PUSHVV ? int(ins.index) : 0;
          if(s==nullptr) {
            st.push_back(mkOpaque("?"));
            break;
            }
          auto e = std::make_shared<Expr>();
          e->sym   = ins.symbol;
          e->index = idx;
          if(s->is_member()) {
            e->kind = Expr::Member;
            e->inst = curInst;
            } else
          if(s->type()==DaedalusDataType::INT && s->is_const()) {
            e->kind  = Expr::Const;
            try { e->value = s->get_int(uint16_t(idx)); } catch(...) {}
            } else
          if(s->type()==DaedalusDataType::STRING && s->is_const()) {
            e->kind = Expr::Str;
            try { e->text = s->get_string(uint16_t(idx)); } catch(...) {}
            } else
          if(s->type()==DaedalusDataType::INT) {
            e->kind = Expr::Var;
            } else
          if(s->type()==DaedalusDataType::INSTANCE) {
            e->kind = Expr::Inst;
            } else
          if(s->type()==DaedalusDataType::FUNCTION) {
            e->kind  = Expr::Const;
            e->value = int32_t(s->index());
            } else {
            e->kind = Expr::Opaque;
            e->text = s->name();
            }
          st.push_back(e);
          break;
          }
        case DaedalusOpcode::LT:
        case DaedalusOpcode::GT:
        case DaedalusOpcode::LTE:
        case DaedalusOpcode::EQ:
        case DaedalusOpcode::NEQ:
        case DaedalusOpcode::GTE: {
          auto a = pop();   // the left side: the compiler pushes the right side first
          auto b = pop();
          if(a->kind==Expr::Const && b->kind==Expr::Const)
            st.push_back(mkConst(opHolds(int32_t(ins.op),a->value,b->value) ? 1 : 0));
          else
            st.push_back(mkBin(Expr::Cmp,a,b,int32_t(ins.op)));
          break;
          }
        case DaedalusOpcode::AND:
        case DaedalusOpcode::ORR: {
          auto b = pop();
          auto a = pop();
          if(a->kind==Expr::Const && b->kind==Expr::Const) {
            const bool r = ins.op==DaedalusOpcode::AND ? (a->value!=0 && b->value!=0) : (a->value!=0 || b->value!=0);
            st.push_back(mkConst(r ? 1 : 0));
            } else
            st.push_back(mkBin(ins.op==DaedalusOpcode::AND ? Expr::And : Expr::Or,a,b));
          break;
          }
        case DaedalusOpcode::NOT: {
          auto a = pop();
          if(a->kind==Expr::Const)
            st.push_back(mkConst(a->value==0 ? 1 : 0)); else
            st.push_back(mkNot(a));
          break;
          }
        case DaedalusOpcode::ADD:
        case DaedalusOpcode::SUB:
        case DaedalusOpcode::MUL:
        case DaedalusOpcode::DIV:
        case DaedalusOpcode::MOD:
        case DaedalusOpcode::OR:
        case DaedalusOpcode::ANDB:
        case DaedalusOpcode::LSL:
        case DaedalusOpcode::LSR: {
          auto a = pop();
          auto b = pop();
          if(a->kind==Expr::Const && b->kind==Expr::Const) {
            int32_t r = 0;
            switch(ins.op) {
              case DaedalusOpcode::ADD:  r = a->value+b->value; break;
              case DaedalusOpcode::SUB:  r = a->value-b->value; break;
              case DaedalusOpcode::MUL:  r = a->value*b->value; break;
              case DaedalusOpcode::DIV:  r = b->value!=0 ? a->value/b->value : 0; break;
              case DaedalusOpcode::MOD:  r = b->value!=0 ? a->value%b->value : 0; break;
              case DaedalusOpcode::OR:   r = a->value|b->value; break;
              case DaedalusOpcode::ANDB: r = a->value&b->value; break;
              case DaedalusOpcode::LSL:  r = a->value<<(b->value&31); break;
              default:                   r = a->value>>(b->value&31); break;
              }
            st.push_back(mkConst(r));
            } else
            st.push_back(mkBin(Expr::Arith,a,b,int32_t(ins.op)));
          break;
          }
        case DaedalusOpcode::PLUS:
          break;
        case DaedalusOpcode::NEGATE: {
          auto a = pop();
          st.push_back(a->kind==Expr::Const ? mkConst(-a->value) : mkOpaque("negate"));
          break;
          }
        case DaedalusOpcode::CMPL: {
          auto a = pop();
          st.push_back(a->kind==Expr::Const ? mkConst(~a->value) : mkOpaque("complement"));
          break;
          }
        case DaedalusOpcode::MOVI:
        case DaedalusOpcode::MOVS:
        case DaedalusOpcode::MOVSS:
        case DaedalusOpcode::MOVVF:
        case DaedalusOpcode::MOVF:
        case DaedalusOpcode::MOVVI:
        case DaedalusOpcode::ADDMOVI:
        case DaedalusOpcode::SUBMOVI:
        case DaedalusOpcode::MULMOVI:
        case DaedalusOpcode::DIVMOVI:
          pop();
          pop();
          break;
        case DaedalusOpcode::BL:
        case DaedalusOpcode::BE: {
          const DaedalusSymbol* f = nullptr;
          if(ins.op==DaedalusOpcode::BE) {
            f = symOf(env,ins.symbol);
            } else {
            initFunctions(env);
            auto it = env.impl.fnByAddr.find(ins.address);
            if(it!=env.impl.fnByAddr.end())
              f = symOf(env,it->second);
            }
          if(f==nullptr) {
            st.push_back(mkOpaque("call"));
            break;
            }
          auto call = std::make_shared<Expr>();
          call->kind = Expr::Call;
          call->sym  = f->index();
          const size_t n = f->count();
          std::vector<ExprP> args(n);
          for(size_t i=n; i>0; --i)
            args[i-1] = pop();
          call->args = std::move(args);
          if(f->has_return() || f->rtype()!=DaedalusDataType::VOID)
            st.push_back(call);
          break;
          }
        case DaedalusOpcode::B:
          pc = ins.address;
          break;
        case DaedalusOpcode::BZ: {
          auto c = pop();
          if(c->kind==Expr::Const) {
            if(c->value==0)
              pc = ins.address;
            break;
            }
          // both ways
          {
          Cons jump = cons;
          jump.emplace_back(c,false);
          walk(ins.address,st,jump,curInst);
          }
          cons.emplace_back(c,true);
          break;
          }
        case DaedalusOpcode::RSR: {
          Path p;
          p.cons = cons;
          if(!st.empty() && (fn.has_return() || fn.rtype()!=DaedalusDataType::VOID))
            p.ret = st.back(); else
          if(fn.has_return() || fn.rtype()!=DaedalusDataType::VOID)
            p.ret = mkConst(0);
          out.push_back(std::move(p));
          return;
          }
        default:
          break;
        }
      }
    }
  };

/// the paths of a function; a function that cannot be read has none
std::vector<Path> pathsOf(Env& env, const DaedalusSymbol& fn) {
  try {
    Walker w{env,fn,{},0};
    w.walk(fn.address(),{},{},0);
    return std::move(w.out);
    }
  catch(const std::exception&) {
    return {};
    }
  }

std::string str(Env& e, const ExprP& x);

std::string strArgs(Env& e, const std::vector<ExprP>& a) {
  std::string r;
  for(size_t i=0; i<a.size(); ++i) {
    if(i>0)
      r += ", ";
    r += a[i]==nullptr ? "?" : str(e,a[i]);
    }
  return r;
  }

std::string str(Env& e, const ExprP& x) {
  if(x==nullptr)
    return "?";
  switch(x->kind) {
    case Expr::Const: {
      // a constant that is an instance or a function of the scripts is easier to read as a name
      if(x->value>1000 && uint32_t(x->value)<e.vm.symbols().size()) {
        auto* s = symOf(e,uint32_t(x->value));
        if(s!=nullptr && (s->type()==DaedalusDataType::INSTANCE || s->type()==DaedalusDataType::FUNCTION) && !s->name().empty())
          return s->name();
        }
      return std::to_string(x->value);
      }
    case Expr::Str:     return "\""+x->text+"\"";
    case Expr::Var:     return symName(e,x->sym)+(x->index>0 ? "["+std::to_string(x->index)+"]" : "");
    case Expr::Inst:    return symName(e,x->sym);
    case Expr::Member:  return symName(e,x->inst)+"."+symName(e,x->sym)+(x->index>0 ? "["+std::to_string(x->index)+"]" : "");
    case Expr::Call:    return symName(e,x->sym)+"("+strArgs(e,x->args)+")";
    case Expr::Cmp:     return "("+str(e,x->args[0])+" "+opText(x->value)+" "+str(e,x->args[1])+")";
    case Expr::And:     return "("+str(e,x->args[0])+" && "+str(e,x->args[1])+")";
    case Expr::Or:      return "("+str(e,x->args[0])+" || "+str(e,x->args[1])+")";
    case Expr::Not:     return "!"+str(e,x->args[0]);
    case Expr::Arith: {
      const char* o = "?";
      switch(DaedalusOpcode(x->value)) {
        case DaedalusOpcode::ADD: o = "+"; break;
        case DaedalusOpcode::SUB: o = "-"; break;
        case DaedalusOpcode::MUL: o = "*"; break;
        case DaedalusOpcode::DIV: o = "/"; break;
        case DaedalusOpcode::MOD: o = "%"; break;
        default: break;
        }
      return "("+str(e,x->args[0])+" "+o+" "+str(e,x->args[1])+")";
      }
    case Expr::Opaque:  return "<"+x->text+">";
    }
  return "?";
  }

// --- the state of the game that the conditions read and the solver changes

struct Ctx final {
  Env&                      env;
  Npc*                      self = nullptr;
  Npc*                      hero = nullptr;
  bool                      dry  = false;      // only say what would be done
  int                       depth = 0;
  std::vector<std::string>* applied  = nullptr;
  std::vector<std::string>* unsolved = nullptr;
  std::map<uint32_t,ExprP>  subst;             // the parameters of the function that is read: the arguments of the call

  void did(std::string s)  { if(applied)  applied->push_back(std::move(s)); }
  void fail(std::string s) { if(unsolved) unsolved->push_back(std::move(s)); }
  };

Npc* npcOf(Ctx& c, const ExprP& x) {
  if(x==nullptr || x->kind!=Expr::Inst)
    return nullptr;
  {
  auto it = c.subst.find(x->sym);
  if(it!=c.subst.end() && it->second!=nullptr && it->second.get()!=x.get())
    return npcOf(c,it->second);
  }
  const std::string n = upper(symName(c.env,x->sym));
  if(n=="SELF")
    return c.self;
  if(n=="OTHER" || n=="HERO")
    return c.hero;
  if(n=="VICTIM" || n=="ITEM")
    return nullptr;
  auto& w = c.env.sc.world();
  Npc* npc = w.findNpcByInstance(x->sym);
  if(npc==nullptr && !c.dry && c.hero!=nullptr && !n.empty() && n[0]!='$') {
    // a character that the condition needs and the world does not have: he is put next to the player
    auto* s = symOf(c.env,x->sym);
    if(s!=nullptr && s->type()==DaedalusDataType::INSTANCE) {
      try {
        npc = w.addNpc(size_t(x->sym),c.hero->position()+Tempest::Vec3(400,0,0));
        if(npc!=nullptr)
          c.did("the character "+n+" was not in the world: put next to the player");
        }
      catch(const std::exception&) {
        npc = nullptr;
        }
      }
    }
  return npc;
  }

Npc* instOf(Ctx& c, uint32_t sym) {
  auto e = std::make_shared<Expr>();
  e->kind = Expr::Inst;
  e->sym  = sym;
  return npcOf(c,ExprP(e));
  }

std::string constStr(Ctx& c, const ExprP& x) {
  if(x==nullptr)
    return {};
  if(x->kind==Expr::Str)
    return x->text;
  if(x->kind==Expr::Member && upper(symName(c.env,x->sym))=="C_NPC.WP") {
    if(Npc* n = instOf(c,x->inst))
      return std::string(n->handle().wp);
    }
  if(x->kind==Expr::Call && upper(symName(c.env,x->sym))=="NPC_GETNEARESTWP" && !x->args.empty()) {
    if(Npc* n = npcOf(c,x->args[0]))
      if(auto* wp = c.env.sc.world().findWayPoint(n->position()))
        return wp->name;
    }
  return {};
  }

/// the members of a character that the conditions read: a reference to the value
int32_t* npcMember(Ctx& c, const Expr& x, int32_t& tmp, bool& isTmp) {
  isTmp = false;
  Npc* npc = instOf(c,x.inst);
  if(npc==nullptr)
    return nullptr;
  const std::string m = upper(symName(c.env,x.sym));
  auto& h = npc->handle();
  if(m=="C_NPC.AIVAR" && x.index>=0 && x.index<100)
    return &h.aivar[x.index];
  if(m=="C_NPC.GUILD")
    return &h.guild;
  if(m=="C_NPC.LEVEL")
    return &h.level;
  if(m=="C_NPC.EXP")
    return &h.exp;
  if(m=="C_NPC.LP")
    return &h.lp;
  if(m=="C_NPC.NPCTYPE") {
    tmp = int32_t(h.type);
    isTmp = true;
    return &tmp;
    }
  if(m=="C_NPC.FLAGS") {
    tmp = int32_t(h.flags);
    isTmp = true;
    return &tmp;
    }
  return nullptr;
  }

bool constInt(const ExprP& x, int32_t& v) {
  if(x!=nullptr && x->kind==Expr::Const) {
    v = x->value;
    return true;
    }
  return false;
  }

std::optional<int32_t> live(Ctx& c, const ExprP& x);

std::optional<int32_t> liveCall(Ctx& c, const Expr& x) {
  const std::string fn = upper(symName(c.env,x.sym));
  auto& a = x.args;
  auto  A = [&](size_t i) -> ExprP { return i<a.size() ? a[i] : nullptr; };

  if(fn=="NPC_KNOWSINFO") {
    Npc* npc = npcOf(c,A(0));
    int32_t info = 0;
    if(npc==nullptr || !constInt(A(1),info))
      return std::nullopt;
    return c.env.sc.npcKnowsInfo(*npc,size_t(info)) ? 1 : 0;
    }
  if(fn=="NPC_HASITEMS" || fn=="NPC_HASITEM") {
    Npc* npc = npcOf(c,A(0));
    int32_t item = 0;
    if(npc==nullptr || !constInt(A(1),item))
      return std::nullopt;
    return int32_t(npc->itemCount(size_t(item)));
    }
  if(fn=="NPC_GETDISTTOWP") {
    Npc* npc = npcOf(c,A(0));
    const auto wpName = constStr(c,A(1));
    if(npc==nullptr || wpName.empty())
      return std::nullopt;
    auto* wp = c.env.sc.world().findWayPoint(wpName);
    if(wp==nullptr)
      return std::nullopt;
    const float d = std::sqrt(npc->qDistTo(wp));
    return d<2.0e9f ? int32_t(d) : INT32_MAX;
    }
  if(fn=="NPC_GETDISTTONPC") {
    Npc* a0 = npcOf(c,A(0));
    Npc* b0 = npcOf(c,A(1));
    if(a0==nullptr || b0==nullptr)
      return std::nullopt;
    return int32_t((a0->position()-b0->position()).length());
    }
  if(fn=="LOG_GETSTATUS" || fn=="LOG_GETSTATUS_GLOBAL")
    return live(c,A(0));
  if(fn=="NPC_ISDEAD") {
    Npc* npc = npcOf(c,A(0));
    if(npc==nullptr)
      return std::nullopt;
    return npc->isDead() ? 1 : 0;
    }
  if(fn=="HLP_GETINSTANCEID") {
    Npc* npc = npcOf(c,A(0));
    if(npc==nullptr)
      return std::nullopt;
    return int32_t(npc->instanceSymbol());
    }
  if(fn=="WLD_GETDAY")
    return int32_t(c.env.sc.world().time().day());
  if(fn=="MOB_HASITEMS") {
    const auto tag = constStr(c,A(0));
    int32_t item = 0;
    if(tag.empty() || !constInt(A(1),item))
      return std::nullopt;
    return int32_t(c.env.sc.world().hasItems(tag,size_t(item)));
    }
  if(fn=="HLP_ISVALIDNPC")
    return npcOf(c,A(0))!=nullptr ? 1 : 0;
  if(fn=="NPC_REFUSETALK") {
    Npc* npc = npcOf(c,A(0));
    if(npc==nullptr)
      return std::nullopt;
    return npc->isRefuseTalk() ? 1 : 0;
    }
  if(fn=="HLP_STRCMP") {
    const auto s0 = constStr(c,A(0));
    const auto s1 = constStr(c,A(1));
    if(s0.empty() || s1.empty())
      return std::nullopt;
    return upper(s0)==upper(s1) ? 1 : 0;
    }
  if(fn=="NPC_ISINSTATE") {
    Npc* npc = npcOf(c,A(0));
    int32_t st = 0;
    if(npc==nullptr || !constInt(A(1),st))
      return std::nullopt;
    // a dialog is held in the state of the talk
    if(npc==c.self && upper(symName(c.env,uint32_t(st)))=="ZS_TALK")
      return 1;
    return npc->isInState(ScriptFn(size_t(st))) ? 1 : 0;
    }
  if(fn=="NPC_GETTALENTSKILL" || fn=="NPC_GETTALENTVALUE") {
    Npc* npc = npcOf(c,A(0));
    int32_t t = 0;
    if(npc==nullptr || !constInt(A(1),t) || t<0 || t>=int32_t(TALENT_MAX_G2))
      return std::nullopt;
    return fn=="NPC_GETTALENTSKILL" ? npc->talentSkill(Talent(t)) : npc->talentValue(Talent(t));
    }
  if(fn=="WLD_ISTIME") {
    int32_t h1=0,m1=0,h2=0,m2=0;
    if(!constInt(A(0),h1) || !constInt(A(1),m1) || !constInt(A(2),h2) || !constInt(A(3),m2))
      return std::nullopt;
    auto t = c.env.sc.world().time();
    const int now = int(t.hour()*60+t.minute());
    const int s   = h1*60+m1;
    const int e   = h2*60+m2;
    return (s<=e ? (now>=s && now<e) : (now>=s || now<e)) ? 1 : 0;
    }
  return std::nullopt;
  }

std::optional<int32_t> live(Ctx& c, const ExprP& x) {
  if(x==nullptr)
    return std::nullopt;
  if(x->kind==Expr::Var) {
    auto it = c.subst.find(x->sym);
    if(it!=c.subst.end() && it->second!=nullptr && it->second.get()!=x.get())
      return live(c,it->second);
    }
  switch(x->kind) {
    case Expr::Const:
      return x->value;
    case Expr::Var: {
      auto* s = symOf(c.env,x->sym);
      if(s==nullptr)
        return std::nullopt;
      try { return s->get_int(uint16_t(x->index)); } catch(...) { return std::nullopt; }
      }
    case Expr::Member: {
      int32_t tmp = 0;
      bool    isTmp = false;
      if(auto* p = npcMember(c,*x,tmp,isTmp))
        return *p;
      return std::nullopt;
      }
    case Expr::Call:
      return liveCall(c,*x);
    case Expr::Cmp: {
      auto a = live(c,x->args[0]);
      auto b = live(c,x->args[1]);
      if(!a || !b)
        return std::nullopt;
      return opHolds(x->value,*a,*b) ? 1 : 0;
      }
    case Expr::And: {
      auto a = live(c,x->args[0]);
      auto b = live(c,x->args[1]);
      if(a && !*a)
        return 0;
      if(b && !*b)
        return 0;
      if(!a || !b)
        return std::nullopt;
      return 1;
      }
    case Expr::Or: {
      auto a = live(c,x->args[0]);
      auto b = live(c,x->args[1]);
      if((a && *a) || (b && *b))
        return 1;
      if(!a || !b)
        return std::nullopt;
      return 0;
      }
    case Expr::Not: {
      auto a = live(c,x->args[0]);
      if(!a)
        return std::nullopt;
      return *a==0 ? 1 : 0;
      }
    case Expr::Arith: {
      auto a = live(c,x->args[0]);
      auto b = live(c,x->args[1]);
      if(!a || !b)
        return std::nullopt;
      switch(DaedalusOpcode(x->value)) {
        case DaedalusOpcode::ADD: return *a + *b;
        case DaedalusOpcode::SUB: return *a - *b;
        case DaedalusOpcode::MUL: return *a * *b;
        case DaedalusOpcode::DIV: return *b!=0 ? *a / *b : 0;
        case DaedalusOpcode::MOD: return *b!=0 ? *a % *b : 0;
        default: return std::nullopt;
        }
      }
    default:
      return std::nullopt;
    }
  }

bool satisfy(Ctx& c, const ExprP& x, bool want);

/// make "x op k" true (want) or false: the value that has to be brought about is derived from the operator
bool setTo(Ctx& c, const ExprP& x, int32_t op, int32_t k, bool want) {
  const int32_t o = want ? op : opInvert(op);
  const int32_t v = desiredFor(o,k);

  if(x->kind==Expr::Var) {
    auto it = c.subst.find(x->sym);
    if(it!=c.subst.end() && it->second!=nullptr && it->second.get()!=x.get())
      return setTo(c,it->second,op,k,want);
    }
  if(x->kind==Expr::Var) {
    auto* s = symOf(c.env,x->sym);
    if(s==nullptr || s->is_const()) {
      c.fail(str(c.env,x)+" is a constant");
      return false;
      }
    c.did(str(c.env,x)+" = "+std::to_string(v));
    if(!c.dry)
      try { s->set_int(v,uint16_t(x->index)); } catch(...) { c.fail("cannot set "+str(c.env,x)); return false; }
    return true;
    }

  if(x->kind==Expr::Member) {
    int32_t tmp = 0;
    bool    isTmp = false;
    auto* p = npcMember(c,*x,tmp,isTmp);
    if(p!=nullptr && !isTmp) {
      c.did(str(c.env,x)+" = "+std::to_string(v));
      if(!c.dry)
        *p = v;
      return true;
      }
    c.fail("member "+str(c.env,x)+" cannot be set");
    return false;
    }

  if(x->kind!=Expr::Call) {
    c.fail("cannot set "+str(c.env,x));
    return false;
    }

  const std::string fn = upper(symName(c.env,x->sym));
  auto& a = x->args;
  auto  A = [&](size_t i) -> ExprP { return i<a.size() ? a[i] : nullptr; };
  const bool truth = (v!=0);

  if(fn=="NPC_KNOWSINFO") {
    Npc* npc = npcOf(c,A(0));
    int32_t info = 0;
    if(npc==nullptr || !constInt(A(1),info)) {
      c.fail(str(c.env,x)+": the character or the dialog is not known");
      return false;
      }
    c.did(std::string(truth ? "told " : "forgotten ")+symName(c.env,uint32_t(info))+" for "+std::string(npc->displayName()));
    if(!c.dry)
      c.env.sc.setInfoKnown(*npc,symName(c.env,uint32_t(info)),truth);
    return true;
    }

  if(fn=="LOG_GETSTATUS" || fn=="LOG_GETSTATUS_GLOBAL") {
    auto arg = A(0);
    if(arg==nullptr || arg->kind!=Expr::Var) {
      c.fail(str(c.env,x)+": the topic is not a variable");
      return false;
      }
    auto* s = symOf(c.env,arg->sym);
    if(s==nullptr)
      return false;
    c.did(symName(c.env,arg->sym)+" = "+std::to_string(v));
    if(!c.dry)
      try { s->set_int(v); } catch(...) { return false; }
    return true;
    }

  if(fn=="NPC_HASITEMS" || fn=="NPC_HASITEM") {
    Npc* npc = npcOf(c,A(0));
    int32_t item = 0;
    if(npc==nullptr || !constInt(A(1),item)) {
      c.fail(str(c.env,x)+": the character or the item is not known");
      return false;
      }
    const int32_t now = int32_t(npc->itemCount(size_t(item)));
    if(v>now) {
      c.did("added "+std::to_string(v-now)+" x "+symName(c.env,uint32_t(item))+" to "+std::string(npc->displayName()));
      if(!c.dry)
        npc->addItem(size_t(item),size_t(v-now));
      } else
    if(v<now) {
      c.did("removed "+std::to_string(now-v)+" x "+symName(c.env,uint32_t(item))+" from "+std::string(npc->displayName()));
      if(!c.dry)
        npc->delItem(size_t(item),uint32_t(now-v));
      }
    return true;
    }

  if(fn=="NPC_GETDISTTOWP") {
    Npc* npc = npcOf(c,A(0));
    const auto wpName = constStr(c,A(1));
    auto* wp = wpName.empty() ? nullptr : c.env.sc.world().findWayPoint(wpName);
    if(npc==nullptr || wp==nullptr) {
      c.fail(str(c.env,x)+": the character or the way point is not in the world");
      return false;
      }
    if(o==int32_t(DaedalusOpcode::GT) || o==int32_t(DaedalusOpcode::GTE) || o==int32_t(DaedalusOpcode::NEQ)) {
      c.fail(str(c.env,x)+": has to be far from the point, not done");
      return false;
      }
    c.did(std::string(npc->displayName())+" moved to the way point "+wpName);
    if(!c.dry) {
      auto p = wp->position();
      const float drop = p.y-wp->groundPos.y;
      npc->setPosition(drop>0.f && drop<200.f ? wp->groundPos : p);
      }
    return true;
    }

  if(fn=="NPC_GETDISTTONPC") {
    Npc* a0 = npcOf(c,A(0));
    Npc* b0 = npcOf(c,A(1));
    if(a0==nullptr || b0==nullptr) {
      c.fail(str(c.env,x)+": a character is not in the world");
      return false;
      }
    if(o==int32_t(DaedalusOpcode::GT) || o==int32_t(DaedalusOpcode::GTE) || o==int32_t(DaedalusOpcode::NEQ)) {
      c.fail(str(c.env,x)+": has to be far, not done");
      return false;
      }
    Npc* mover = (a0==c.hero) ? b0 : a0;
    Npc* stays = (mover==a0)  ? b0 : a0;
    c.did(std::string(mover->displayName())+" moved next to "+std::string(stays->displayName()));
    if(!c.dry)
      mover->setPosition(stays->position()+Tempest::Vec3(120,0,0));
    return true;
    }

  if(fn=="WLD_ISTIME") {
    int32_t h1=0,m1=0,h2=0,m2=0;
    if(!constInt(A(0),h1) || !constInt(A(1),m1) || !constInt(A(2),h2) || !constInt(A(3),m2)) {
      c.fail(str(c.env,x)+": the time is not a constant");
      return false;
      }
    if(!truth) {
      c.fail(str(c.env,x)+": has to be another time, not done");
      return false;
      }
    c.did("time set to "+std::to_string(h1)+":"+std::to_string(m1));
    if(!c.dry)
      c.env.sc.world().setDayTime(h1,m1);
    return true;
    }

  if(fn=="NPC_GETTALENTSKILL" || fn=="NPC_GETTALENTVALUE") {
    Npc* npc = npcOf(c,A(0));
    int32_t t = 0;
    if(npc==nullptr || !constInt(A(1),t) || t<0 || t>=int32_t(TALENT_MAX_G2)) {
      c.fail(str(c.env,x)+": the character or the talent is not known");
      return false;
      }
    c.did("talent "+std::to_string(t)+" of "+std::string(npc->displayName())+" = "+std::to_string(v));
    if(!c.dry) {
      if(fn=="NPC_GETTALENTSKILL")
        npc->setTalentSkill(Talent(t),v); else
        npc->setTalentValue(Talent(t),v);
      }
    return true;
    }

  if(fn=="MOB_HASITEMS") {
    const auto tag = constStr(c,A(0));
    int32_t item = 0;
    if(tag.empty() || !constInt(A(1),item)) {
      c.fail(str(c.env,x)+": the container or the item is not known");
      return false;
      }
    auto& w = c.env.sc.world();
    Interactive* mob = nullptr;
    for(uint32_t i=0; (mob = w.mobsiById(i))!=nullptr; ++i)
      if(mob->tag()==tag)
        break;
    if(mob==nullptr) {
      c.fail(str(c.env,x)+": there is no such container in the world");
      return false;
      }
    const int32_t now = int32_t(mob->inventory().itemCount(size_t(item)));
    if(v>now) {
      c.did("added "+std::to_string(v-now)+" x "+symName(c.env,uint32_t(item))+" to the container "+tag);
      if(!c.dry)
        mob->inventory().addItem(size_t(item),size_t(v-now),w);
      }
    return true;
    }

  if(fn=="HLP_ISVALIDNPC") {
    if(truth && npcOf(c,A(0))!=nullptr)
      return true;
    c.fail(str(c.env,x)+": the character cannot be made valid");
    return false;
    }

  if(fn=="NPC_REFUSETALK") {
    Npc* npc = npcOf(c,A(0));
    if(npc==nullptr) {
      c.fail(str(c.env,x)+": the character is not in the world");
      return false;
      }
    c.did(std::string("refuse talk of ")+std::string(npc->displayName())+(truth ? " on" : " off"));
    if(!c.dry)
      npc->setRefuseTalk(truth ? 600000 : 0);
    return true;
    }

  if(fn=="NPC_ISDEAD") {
    Npc* npc = npcOf(c,A(0));
    if(npc==nullptr || !truth) {
      c.fail(str(c.env,x)+(npc==nullptr ? ": the character is not in the world" : ": has to be alive again, not done"));
      return false;
      }
    c.did(std::string(npc->displayName())+" killed");
    if(!c.dry)
      npc->changeAttribute(ATR_HITPOINTS,-100000,false);
    return true;
    }

  if(fn=="NPC_ISINSTATE") {
    Npc* npc = npcOf(c,A(0));
    int32_t st = 0;
    if(npc!=nullptr && constInt(A(1),st) && truth && upper(symName(c.env,uint32_t(st)))=="ZS_UNCONSCIOUS") {
      c.did(std::string(npc->displayName())+" knocked out");
      if(!c.dry)
        npc->debugUnconscious(true);
      return true;
      }
    c.fail(str(c.env,x)+": cannot be arranged by the tool");
    return false;
    }

  if(fn=="HLP_STRCMP" && truth) {
    // the nearest way point of a character has to be a point: he is put there
    for(int side=0; side<2; ++side) {
      auto g = A(size_t(side));
      auto other = A(size_t(1-side));
      if(g!=nullptr && g->kind==Expr::Call && upper(symName(c.env,g->sym))=="NPC_GETNEARESTWP" && !g->args.empty()) {
        Npc* npc = npcOf(c,g->args[0]);
        const auto wpName = constStr(c,other);
        auto* wp = wpName.empty() ? nullptr : c.env.sc.world().findWayPoint(wpName);
        if(npc!=nullptr && wp!=nullptr) {
          c.did(std::string(npc->displayName())+" moved to the way point "+wpName);
          if(!c.dry) {
            const float drop = wp->position().y-wp->groundPos.y;
            npc->setPosition(drop>0.f && drop<200.f ? wp->groundPos : wp->position());
            }
          return true;
          }
        }
      }
    c.fail(str(c.env,x)+": the strings cannot be made equal");
    return false;
    }

  if(fn=="NPC_WASINSTATE" || fn=="HLP_GETINSTANCEID" || fn=="NPC_GETATTITUDE" ||
     fn=="NPC_ISINROUTINE" || fn=="NPC_ISPLAYER" || fn=="NPC_CANSEENPC" || fn=="HLP_STRCMP") {
    c.fail(str(c.env,x)+": cannot be arranged by the tool");
    return false;
    }

  // a function of the scripts: what its returns need
  if(symOf(c.env,x->sym)!=nullptr && !symOf(c.env,x->sym)->is_external() && c.depth<4) {
    auto* f = symOf(c.env,x->sym);
    auto paths = pathsOf(c.env,*f);
    // the parameters of the function are the arguments of the call
    auto savedSubst = c.subst;
    for(uint32_t i=0; i<f->count() && i<x->args.size(); ++i)
      c.subst[f->index()+1+i] = x->args[i];
    // the paths that return what is wanted
    for(auto& p:paths) {
      if(p.ret==nullptr)
        continue;
      auto rv = live(c,p.ret);
      if(rv && opHolds(o,*rv,k)) {
        bool ok = true;
        c.depth++;
        for(auto& cs:p.cons)
          ok = satisfy(c,cs.first,cs.second) && ok;
        c.depth--;
        if(ok) {
          c.subst = savedSubst;
          return true;
          }
        }
      }
    c.subst = savedSubst;
    c.fail(str(c.env,x)+": no way found through the function");
    return false;
    }

  c.fail(str(c.env,x)+": an engine function that the tool does not know");
  return false;
  }

bool satisfy(Ctx& c, const ExprP& x, bool want) {
  if(x==nullptr)
    return false;
  if(auto v = live(c,x)) {
    if((*v!=0)==want)
      return true;
    }
  switch(x->kind) {
    case Expr::Const:
      return (x->value!=0)==want;
    case Expr::Not:
      return satisfy(c,x->args[0],!want);
    case Expr::And:
      if(want) {
        bool a = satisfy(c,x->args[0],true);
        bool b = satisfy(c,x->args[1],true);
        return a && b;
        }
      return satisfy(c,x->args[0],false) || satisfy(c,x->args[1],false);
    case Expr::Or:
      if(!want) {
        bool a = satisfy(c,x->args[0],false);
        bool b = satisfy(c,x->args[1],false);
        return a && b;
        }
      return satisfy(c,x->args[0],true) || satisfy(c,x->args[1],true);
    case Expr::Cmp: {
      const int32_t op = want ? x->value : opInvert(x->value);
      auto& l = x->args[0];
      auto& r = x->args[1];
      auto  lv = live(c,l);
      auto  rv = live(c,r);
      if(rv && (l->kind==Expr::Var || l->kind==Expr::Member || l->kind==Expr::Call))
        return setTo(c,l,op,*rv,true);
      if(lv && (r->kind==Expr::Var || r->kind==Expr::Member || r->kind==Expr::Call))
        return setTo(c,r,opSwap(op),*lv,true);
      c.fail(str(c.env,x)+": neither side can be set");
      return false;
      }
    case Expr::Var:
    case Expr::Member:
    case Expr::Call:
      return setTo(c,x,int32_t(DaedalusOpcode::NEQ),0,want);
    default:
      break;
    }
  c.fail(str(c.env,x)+": not understood");
  return false;
  }

}

DialogTest::DialogTest(GameScript& sc) : impl(new Impl()), sc(sc) {
  }

DialogTest::~DialogTest() = default;

namespace {
const char* ProgressFile = "dialogtest_progress.txt";

std::string flat(std::string s) {
  for(auto& c:s)
    if(c=='\t' || c=='\n' || c=='\r' || c=='\x1f')
      c = ' ';
  return s;
  }

std::string joinList(const std::vector<std::string>& v) {
  std::string r;
  for(size_t i=0; i<v.size(); ++i) {
    if(i>0)
      r += '\x1f';
    r += flat(v[i]);
    }
  return r;
  }

std::vector<std::string> split(const std::string& s, char sep) {
  std::vector<std::string> out;
  size_t from = 0;
  for(;;) {
    const size_t at = s.find(sep,from);
    if(at==std::string::npos) {
      out.push_back(s.substr(from));
      break;
      }
    out.push_back(s.substr(from,at-from));
    from = at+1;
    }
  return out;
  }
}

void DialogTest::reset() {
  results.clear();
  statusOf.clear();
  impl->problems.clear();
  std::ofstream f(ProgressFile,std::ios::binary|std::ios::trunc);   // empty
  }

void DialogTest::appendProgress(const Result& r) const {
  std::ofstream f(ProgressFile,std::ios::binary|std::ios::app);
  if(!f)
    return;
  f << flat(r.name) << '\t' << flat(r.npc) << '\t' << flat(r.status) << '\t' << int(r.conditionBefore) << '\t' << int(r.conditionAfter) << '\t'
    << int(r.executed) << '\t' << r.outputs << '\t' << r.actions << '\t' << joinList(r.applied) << '\t' << joinList(r.unsolved) << '\t'
    << joinList(r.issues) << '\n';
  }

size_t DialogTest::loadProgress() {
  results.clear();
  statusOf.clear();
  impl->problems.clear();

  size_t n = 0;
  {
  std::ifstream f(ProgressFile,std::ios::binary);
  std::string line;
  while(f && std::getline(f,line)) {
    if(!line.empty() && line.back()=='\r')
      line.pop_back();
    auto fld = split(line,'\t');
    if(fld.size()<11 || fld[0].empty() || statusOf.find(fld[0])!=statusOf.end())
      continue;
    Result r;
    r.name            = fld[0];
    r.npc             = fld[1];
    r.status          = fld[2];
    r.conditionBefore = fld[3]=="1";
    r.conditionAfter  = fld[4]=="1";
    r.executed        = fld[5]=="1";
    try { r.outputs = std::stoi(fld[6]); r.actions = std::stoi(fld[7]); } catch(...) {}
    auto list = [](const std::string& s) { std::vector<std::string> v; if(!s.empty()) v = split(s,'\x1f'); return v; };
    r.applied  = list(fld[8]);
    r.unsolved = list(fld[9]);
    r.issues   = list(fld[10]);
    statusOf[r.name] = r.status;
    results.push_back(std::move(r));
    ++n;
    }
  }

  // the dialog that was running when the game died has no result: it is the one that crashed the game
  std::string last;
  {
  std::ifstream f("dialogtest_last.txt",std::ios::binary);
  std::getline(f,last);
  while(!last.empty() && (last.back()=='\r' || last.back()=='\n'))
    last.pop_back();
  }
  if(!last.empty() && statusOf.find(last)==statusOf.end()) {
    Result r;
    r.name   = last;
    r.status = "crashed";
    r.issues.push_back("the game died while this dialog ran (dialogtest_last.txt, crash.log)");
    statusOf[r.name] = r.status;
    appendProgress(r);
    results.push_back(std::move(r));
    ++n;
    }
  return n;
  }

std::string DialogTest::lastStatus(const std::string& info) const {
  auto it = statusOf.find(info);
  return it==statusOf.end() ? std::string() : it->second;
  }

std::vector<DialogTest::Row> DialogTest::list(std::string_view filter, size_t skip, size_t max, size_t* total) {
  auto& vm = sc.getVm();
  auto* w  = Gothic::inst().world();
  Npc*  hero = w!=nullptr ? w->player() : nullptr;

  std::vector<std::string> words;
  std::string statusFilter;
  {
  std::istringstream is{std::string(filter)};
  std::string t;
  while(is>>t) {
    if(t.compare(0,7,"status:")==0)
      statusFilter = t.substr(7); else
      words.push_back(t);
    }
  }

  std::vector<Row> all;
  for(auto& info:sc.infoList()) {
    auto* isym = vm.find_symbol_by_index(uint32_t(info->symbol_index()));
    auto* nsym = vm.find_symbol_by_index(uint32_t(info->npc));
    if(isym==nullptr)
      continue;
    // the placeholders of the scripts (RESERVED_INFO_nn of the character $INSTANCE_HELP) are not dialogs
    if(nsym!=nullptr && (nsym->name().empty() || nsym->name()[0]=='$'))
      continue;
    Row r;
    r.name        = isym->name();
    r.npc         = nsym!=nullptr ? nsym->name() : "?";
    r.description = u8(info->description);
    r.nr          = info->nr;
    r.important   = info->important!=0;
    r.permanent   = info->permanent!=0;
    r.trade       = info->trade!=0;
    if(w!=nullptr && nsym!=nullptr) {
      if(auto* n = w->findNpcByInstance(nsym->index())) {
        r.inWorld = true;
        r.npcName = u8(std::string(n->displayName()));
        }
      }
    if(hero!=nullptr)
      r.known = sc.npcKnowsInfo(*hero,isym->index());
    r.status = lastStatus(r.name);

    bool ok = true;
    for(auto& wd:words) {
      if(!containsNoCase(r.name,wd) && !containsNoCase(r.npc,wd) && !containsNoCase(r.npcName,wd) && !containsNoCase(r.description,wd)) {
        ok = false;
        break;
        }
      }
    if(ok && !statusFilter.empty() && upper(r.status)!=upper(statusFilter) && !(statusFilter=="none" && r.status.empty()))
      ok = false;
    if(ok)
      all.push_back(std::move(r));
    }
  std::stable_sort(all.begin(),all.end(),[](const Row& a, const Row& b){
    if(a.npc!=b.npc)
      return a.npc<b.npc;
    return a.nr<b.nr;
    });
  if(total!=nullptr)
    *total = all.size();
  std::vector<Row> out;
  for(size_t i=skip; i<all.size() && out.size()<max; ++i)
    out.push_back(std::move(all[i]));
  return out;
  }

namespace {
zenkit::IInfo* findInfo(GameScript& sc, std::string_view name, DaedalusSymbol*& sym) {
  sym = sc.getVm().find_symbol_by_name(name);
  if(sym==nullptr)
    return nullptr;
  for(auto& i:sc.infoList())
    if(i->symbol_index()==sym->index())
      return i.get();
  return nullptr;
  }
}

std::string DialogTest::analyze(std::string_view infoName) {
  Env env{sc,sc.getVm(),*impl};
  DaedalusSymbol* isym = nullptr;
  auto* info = findInfo(sc,infoName,isym);
  if(info==nullptr)
    return "No such dialog: "+std::string(infoName);

  std::string out;
  auto* w    = Gothic::inst().world();
  Npc*  hero = w!=nullptr ? w->player() : nullptr;
  Npc*  npc  = w!=nullptr ? w->findNpcByInstance(size_t(info->npc)) : nullptr;
  out += isym->name()+" ("+symName(env,uint32_t(info->npc))+(npc==nullptr ? ", not in this world" : "")+")\n";
  out += "  \""+info->description+"\"  nr "+std::to_string(info->nr)+(info->important ? ", important" : "")+(info->permanent ? ", permanent" : "")+
         (info->trade ? ", trade" : "")+"\n";
  if(info->condition==0) {
    out += "  no condition\n";
    return out;
    }
  auto* cond = symOf(env,uint32_t(info->condition));
  if(cond==nullptr)
    return out+"  the condition function is missing\n";

  out += "  condition "+cond->name()+"\n";
  auto wkOut = pathsOf(env,*cond);
  std::vector<Path> good;
  for(auto& p:wkOut) {
    if(p.ret==nullptr || (p.ret->kind==Expr::Const && p.ret->value==0))
      continue;
    good.push_back(p);
    }
  if(good.empty())
    return out+"  no way to return TRUE found (the condition may need something the tool does not read)\n";

  Ctx c{env,npc,hero,true,0,nullptr,nullptr};
  int n = 0;
  for(auto& p:good) {
    out += "  way "+std::to_string(++n)+":\n";
    Cons cons0 = p.cons;
    if(p.ret->kind!=Expr::Const)
      cons0.emplace_back(p.ret,true);
    // "a && b" that has to be true is two conditions
    Cons cons;
    std::function<void(const ExprP&,bool)> flat = [&](const ExprP& e, bool truth) {
      if(e->kind==Expr::And && truth) {
        flat(e->args[0],true);
        flat(e->args[1],true);
        } else
      if(e->kind==Expr::Or && !truth) {
        flat(e->args[0],false);
        flat(e->args[1],false);
        } else
      if(e->kind==Expr::Not) {
        flat(e->args[0],!truth);
        } else {
        cons.emplace_back(e,truth);
        }
      };
    for(auto& cs:cons0)
      flat(cs.first,cs.second);
    for(auto& cs:cons) {
      std::vector<std::string> applied,unsolved;
      c.applied  = &applied;
      c.unsolved = &unsolved;
      auto lv = live(c,cs.first);
      const bool holds = lv ? ((*lv!=0)==cs.second) : false;
      out += std::string("    ")+(holds ? "[ok]      " : lv ? "[to do]   " : "[unknown] ")+(cs.second ? "" : "not ")+str(env,cs.first)+"\n";
      if(!holds) {
        const bool ok = satisfy(c,cs.first,cs.second);
        for(auto& a:applied)
          out += "                would: "+a+"\n";
        for(auto& u:unsolved)
          out += "                CANNOT: "+u+"\n";
        if(!ok && unsolved.empty())
          out += "                CANNOT\n";
        }
      }
    if(n>=6)
      break;
    }
  if(hero==nullptr)
    out += "  (no player: the state of the game was not read)\n";
  return out;
  }

namespace {

void addUnique(std::vector<std::string>& v, const std::string& text) {
  const std::string s = u8(text);
  if(v.size()<12 && std::find(v.begin(),v.end(),s)==v.end())
    v.push_back(s);
  }

/// The lines of the log since the serial that tell what is missing in the game. A failure of the scripts is a block with the stack
/// and, after it, the message of the library (LeGo: MEM_Error): both make one problem.
void collectLog(DialogTest::Result& r, uint64_t from, std::map<std::string,std::pair<int,std::string>>& problems) {
  std::vector<GameLog::Line> lines;
  GameLog::since(from,6000,lines);

  auto add = [&](std::string key) {
    key = u8(key);
    if(key.size()>200)
      key.resize(200);
    addUnique(r.issues,key);
    auto& p = problems[key];
    if(p.first==0)
      p.second = r.name;
    };

  bool                     inBlock = false;
  bool                     pending = false;
  std::vector<std::string> chain;
  auto where = [&]() {
    std::string w;
    for(size_t i=0; i<chain.size() && i<3; ++i)
      w += (i==0 ? "" : " < ")+chain[i];
    return w;
    };

  for(auto& l:lines) {
    const std::string& t = l.text;
    if(t=="[start of stacktrace]") {
      inBlock = true;
      pending = false;
      chain.clear();
      continue;
      }
    if(inBlock) {
      if(t=="[end of stacktrace]") {
        inBlock = false;
        pending = true;
        } else
      if(t.compare(0,12,"[zenkit] in ")==0) {
        auto fn = t.substr(12,t.find(" at",12)-12);
        if(fn.compare(0,4,"MEM_")!=0 && fn.compare(0,7,"MEMINT_")!=0)
          chain.push_back(fn);
        }
      continue;
      }
    if(pending) {
      pending = false;
      if(t.compare(0,7,"[zpy]: ")==0) {
        add("script: "+t.substr(7)+"   ["+where()+"]");
        continue;
        }
      add("script error in "+where());
      }

    if(t.find("not implemented call")!=std::string::npos) {
      add("engine: "+t);
      } else
    if(t.find("Internal Exception")!=std::string::npos || t.find("illegal access")!=std::string::npos) {
      add("script: "+t.substr(t.find("]")==std::string::npos ? 0 : std::min(t.find("]")+2,t.size())));
      } else
    if(t.find("Unknown Symbol")!=std::string::npos || t.find("without an instance set")!=std::string::npos) {
      add("script: "+t);
      } else
    if(t.compare(0,7,"[zpy]: ")==0 && t.find("===")==std::string::npos && t.find("buffer")==std::string::npos &&
       t.find("objects ")==std::string::npos && t.find("ellapsed")==std::string::npos) {
      add("lego: "+t.substr(7));
      } else
    if(l.level==GameLog::Error) {
      add("error: "+t);
      }
    }
  // every problem counts once for a dialog
  for(auto& is:r.issues) {
    auto it = problems.find(is);
    if(it!=problems.end())
      it->second.first++;
    }
  }

void countOther(DialogTest::Result& r, std::map<std::string,std::pair<int,std::string>>& problems) {
  for(auto& is:r.issues) {
    if(is.compare(0,7,"script:")==0 || is.compare(0,7,"engine:")==0 || is.compare(0,6,"error:")==0 || is.compare(0,5,"lego:")==0 || is.compare(0,12,"script error")==0)
      continue;   // counted with the log
    auto& p = problems[is];
    if(p.first==0)
      p.second = r.name;
    p.first++;
    }
  for(auto& u:r.unsolved) {
    auto& p = problems["condition: "+u];
    if(p.first==0)
      p.second = r.name;
    p.first++;
    }
  }

void drainAll(GameScript& sc, DialogTest::Result& r) {
  auto* w = Gothic::inst().world();
  if(w==nullptr)
    return;
  // A scene of the story can queue commands for hundreds of characters, and the commands queue more: the run has a budget
  constexpr int Budget = 3000;
  Npc::DrainReport rep;
  for(int round=0; round<3 && rep.actions<Budget; ++round) {
    bool any = false;
    for(uint32_t i=0, n=w->npcCount(); i<n && rep.actions<Budget; ++i) {
      auto* npc = w->npcById(i);
      if(npc==nullptr || npc->isAiQueueEmpty())
        continue;
      any = true;
      npc->debugDrainQueue(rep,std::min(600,Budget-rep.actions));
      }
    if(!any)
      break;
    }
  if(rep.actions>=Budget)
    rep.issues.push_back("the commands of the scene were cut after "+std::to_string(Budget)+" (a very long scene or a loop of commands)");
  (void)sc;
  r.outputs += rep.outputs;
  r.actions += rep.actions;
  for(auto& i:rep.issues)
    addUnique(r.issues,i);
  }

}

namespace {
/// what runs now: if the game crashes in a dialog, the file says which one (dialogtest_last.txt, next to log.txt)
void markCurrent(std::string_view name) {
  std::ofstream f("dialogtest_last.txt",std::ios::binary|std::ios::trunc);
  f << name << "\n";
  }
}

DialogTest::Result DialogTest::run(std::string_view infoName, bool solve, bool execute) {
  Result r;
  try {
    r = runImpl(infoName,solve,execute);
    }
  catch(const std::exception& e) {
    // a failure in one dialog must not end the run; the result is kept like any other
    r.name   = std::string(infoName);
    r.status = "error";
    r.issues.push_back(std::string("exception: ")+e.what());
    results.push_back(r);
    statusOf[r.name] = r.status;
    }
  appendProgress(r);
  return r;
  }

DialogTest::Result DialogTest::runImpl(std::string_view infoName, bool solve, bool execute) {
  markCurrent(infoName);
  Result r;
  r.name = std::string(infoName);
  Env env{sc,sc.getVm(),*impl};
  DaedalusSymbol* isym = nullptr;
  auto* info = findInfo(sc,infoName,isym);
  auto* w    = Gothic::inst().world();
  Npc*  hero = w!=nullptr ? w->player() : nullptr;
  if(info==nullptr) {
    r.status = "unknown";
    results.push_back(r);
    statusOf[r.name] = r.status;
    return r;
    }
  r.npc = symName(env,uint32_t(info->npc));
  // the helpers of the story (STORYHELPER_*: jump to a chapter) change the whole world, they are no dialogs
  if(r.npc.empty() || r.npc[0]=='$' || containsNoCase(infoName,"STORYHELPER")) {
    r.status = "skipped";
    results.push_back(r);
    statusOf[r.name] = r.status;
    return r;
    }
  if(w==nullptr || hero==nullptr) {
    r.status = "no-world";
    results.push_back(r);
    statusOf[r.name] = r.status;
    return r;
    }
  Npc* npc = w->findNpcByInstance(size_t(info->npc));
  if(npc==nullptr && solve) {
    // a character of another world (or one that the scripts have not inserted yet): the dialog is tried with him next to the player
    auto* ns = symOf(env,uint32_t(info->npc));
    if(ns!=nullptr && ns->type()==DaedalusDataType::INSTANCE) {
      try {
        npc = w->addNpc(size_t(info->npc),hero->position()+Tempest::Vec3(300,0,0));
        }
      catch(const std::exception& e) {
        npc = nullptr;
        r.issues.push_back("the character "+r.npc+" cannot be inserted: "+e.what());
        }
      if(npc!=nullptr)
        r.applied.push_back("the character "+r.npc+" was not in the world: put next to the player");
      }
    }
  if(npc==nullptr) {
    r.status = "no-npc";
    r.issues.push_back("the character "+r.npc+" is not in the world "+std::string(w->name()));
    countOther(r,impl->problems);
    results.push_back(r);
    statusOf[r.name] = r.status;
    return r;
    }

  struct Active {
    explicit Active(Npc* n) { DialogTest::active = true; DialogTest::talkNpc = n; }
    ~Active() { DialogTest::active = false; DialogTest::talkNpc = nullptr; }
    } active(npc);
  // Everybody goes back where he was: the characters that stay next to the player (hundreds of them after a long run) would be
  // simulated, drawn and kept in the memory
  struct Home {
    Npc*          n;
    Npc*          h;
    Tempest::Vec3 np, hp;
    ~Home() {
      if(n!=nullptr) { n->clearGoTo(); n->setPosition(np); }
      if(h!=nullptr) h->setPosition(hp);
      }
    } home{npc,hero,npc->position(),hero->position()};
  const uint64_t logFrom = GameLog::serial();

  // a trialog of the run before that has not ended would stop this one (LeGo: the trialog is running already)
  if(auto* tr = env.vm.find_symbol_by_name("TRIA_RUNNING")) {
    try {
      if(tr->get_int()!=0) {
        tr->set_int(0);
        if(auto* f = env.vm.find_symbol_by_name("_TRIA_FINISH"))
          env.vm.call_function<void>(f);
        }
      }
    catch(const std::exception&) {}
    }

  auto condition = [&]() -> bool {
    if(info->condition==0)
      return true;
    auto* cs = symOf(env,uint32_t(info->condition));
    if(cs==nullptr)
      return true;
    ScopeInst<zenkit::INpc> self (*env.vm.global_self(),  npc->handlePtr());
    ScopeInst<zenkit::INpc> other(*env.vm.global_other(), hero->handlePtr());
    try { return env.vm.call_function<int>(cs)!=0; } catch(const std::exception&) { return false; }
    };

  r.conditionBefore = condition();
  r.conditionAfter  = r.conditionBefore;

  if(!r.conditionBefore && solve && info->condition!=0) {
    auto* cs = symOf(env,uint32_t(info->condition));
    auto wkOut = pathsOf(env,*cs);
    std::vector<Path> good;
    for(auto& p:wkOut)
      if(p.ret!=nullptr && !(p.ret->kind==Expr::Const && p.ret->value==0))
        good.push_back(p);
    std::stable_sort(good.begin(),good.end(),[](const Path& a, const Path& b){ return a.cons.size()<b.cons.size(); });
    int tries = 0;
    for(auto& p:good) {
      if(++tries>8)
        break;
      std::vector<std::string> applied,unsolved;
      Ctx c{env,npc,hero,false,0,&applied,&unsolved};
      Cons cons = p.cons;
      if(p.ret->kind!=Expr::Const)
        cons.emplace_back(p.ret,true);
      for(auto& cs2:cons)
        satisfy(c,cs2.first,cs2.second);
      r.applied  = applied;
      r.unsolved = unsolved;
      r.conditionAfter = condition();
      if(r.conditionAfter)
        break;
      }
    if(good.empty())
      r.unsolved.push_back("the condition has no way to return TRUE that the tool can read");
    else
    if(!r.conditionAfter && r.unsolved.empty()) {
      // the tool thinks that nothing is missing, the game says no: what the tool has read
      std::string t = "the tool read these conditions as true:";
      Ctx c{env,npc,hero,true,0,nullptr,nullptr};
      for(auto& cs2:good[0].cons) {
        t += "  "+std::string(cs2.second ? "" : "not ")+str(env,cs2.first);
        auto lv = live(c,cs2.first);
        t += lv ? " = "+std::to_string(*lv) : " = ?";
        }
      r.applied.push_back(t);
      }
    }

  if(!r.conditionAfter) {
    r.status = r.unsolved.empty() ? "cond-false" : "cond-unsolved";
    if(r.unsolved.empty() && solve)
      r.issues.push_back("the condition does not hold and the tool found nothing to set");
    collectLog(r,logFrom,impl->problems);
    countOther(r,impl->problems);
    results.push_back(r);
    statusOf[r.name] = r.status;
    return r;
    }

  r.status = "ok";
  if(execute) {
    r.executed = true;
    // a dialog is held next to the character (the libraries of the mods check the distances: LeGo does not invite a character that is
    // not near the player)
    if((hero->position()-npc->position()).length()>500.f) {
      // the character comes to the player (the player does not travel through the world: the game would load and keep the
      // characters of every place), and goes back after the dialog
      npc->clearGoTo();
      npc->setPosition(hero->position()+Tempest::Vec3(150,0,0));
      r.applied.push_back(std::string(npc->displayName())+" moved next to the player");
      }
    // The characters that the function of the dialog names (the invited of a trialog, a character that it sends somewhere) come to the
    // player as well, and go back after the dialog: LeGo invites only those who are near the player
    std::vector<Npc*> named;
    if(auto* fnSym = symOf(env,uint32_t(info->information))) {
      try {
        uint32_t pc = fnSym->address();
        for(int n=0; n<4000; ++n) {
          auto ins = env.vm.instruction_at(pc);
          if(ins.op==DaedalusOpcode::PUSHVI || ins.op==DaedalusOpcode::GMOVI) {
            auto* s = symOf(env,ins.symbol);
            if(s!=nullptr && s->type()==DaedalusDataType::INSTANCE) {
              const std::string nm = upper(s->name());
              if(nm!="SELF" && nm!="OTHER" && nm!="HERO" && nm!="VICTIM" && nm!="ITEM" && nm.find('.')==std::string::npos) {
                Npc* extra = w->findNpcByInstance(ins.symbol);
                if(extra!=nullptr && extra!=npc && extra!=hero && std::find(named.begin(),named.end(),extra)==named.end())
                  named.push_back(extra);
                }
              }
            }
          if(ins.op==DaedalusOpcode::RSR)
            break;
          pc += ins.size;
          }
        }
      catch(const std::exception&) {}
      }
    std::vector<std::pair<Npc*,Tempest::Vec3>> away;
    int k = 0;
    for(auto* extra:named) {
      if(k>=8)
        break;
      if((extra->position()-hero->position()).length()<600.f)
        continue;
      away.emplace_back(extra,extra->position());
      extra->clearGoTo();
      extra->setPosition(hero->position()+Tempest::Vec3(200.f+float(k)*60.f,0,150));
      r.applied.push_back(std::string(extra->displayName())+" named by the dialog: moved next to the player");
      ++k;
      }
    struct Back {
      std::vector<std::pair<Npc*,Tempest::Vec3>>& list;
      ~Back() { for(auto& a:list) { a.first->clearGoTo(); a.first->setPosition(a.second); } }
      } back{away};
    std::set<uint32_t> done;
    std::vector<uint32_t> todo;
    todo.push_back(uint32_t(info->information));
    int guard = 0;
    while(!todo.empty() && guard++<24) {
      const uint32_t fn = todo.front();
      todo.erase(todo.begin());
      if(fn==0 || !done.insert(fn).second)
        continue;
      GameScript::DlgChoice ch;
      ch.title    = info->description;
      ch.scriptFn = fn;
      ch.handle   = info;
      try {
        sc.exec(ch,*hero,*npc);
        }
      catch(const std::exception& e) {
        r.status = "error";
        addUnique(r.issues,std::string("exception: ")+e.what());
        }
      drainAll(sc,r);
      for(auto& c:info->choices)
        if(c.function!=0 && done.find(uint32_t(c.function))==done.end())
          todo.push_back(uint32_t(c.function));
      }
    }

  collectLog(r,logFrom,impl->problems);
  countOther(r,impl->problems);
  if(!r.issues.empty() && r.status=="ok")
    r.status = "issues";
  results.push_back(r);
  statusOf[r.name] = r.status;
  return r;
  }

DialogTest::Result DialogTest::runFunction(std::string_view function, std::string_view selfNpc) {
  markCurrent(function);
  Result r;
  r.name = std::string(function);
  Env env{sc,sc.getVm(),*impl};
  auto* w    = Gothic::inst().world();
  Npc*  hero = w!=nullptr ? w->player() : nullptr;
  auto* fn   = sc.getVm().find_symbol_by_name(function);
  if(fn==nullptr || fn->type()!=DaedalusDataType::FUNCTION) {
    r.status = "unknown";
    results.push_back(r);
    statusOf[r.name] = r.status;
    return r;
    }
  if(w==nullptr || hero==nullptr) {
    r.status = "no-world";
    results.push_back(r);
    statusOf[r.name] = r.status;
    return r;
    }
  Npc* self = hero;
  if(!selfNpc.empty()) {
    auto* s = sc.getVm().find_symbol_by_name(selfNpc);
    Npc*  n = s!=nullptr ? w->findNpcByInstance(s->index()) : nullptr;
    if(n==nullptr) {
      r.status = "no-npc";
      r.issues.push_back("the character "+std::string(selfNpc)+" is not in the world");
      countOther(r,impl->problems);
      results.push_back(r);
      statusOf[r.name] = r.status;
      return r;
      }
    self = n;
    }
  r.npc = std::string(self->displayName());

  struct Active { Active() { DialogTest::active = true; } ~Active() { DialogTest::active = false; } } active;
  const uint64_t logFrom = GameLog::serial();
  r.conditionBefore = r.conditionAfter = true;
  r.executed = true;
  r.status   = "ok";
  {
  ScopeInst<zenkit::INpc> s (*env.vm.global_self(),  self->handlePtr());
  ScopeInst<zenkit::INpc> o (*env.vm.global_other(), hero->handlePtr());
  try { env.vm.call_function<void>(fn); }
  catch(const std::exception& e) {
    r.status = "error";
    addUnique(r.issues,std::string("exception: ")+e.what());
    }
  }
  drainAll(sc,r);
  collectLog(r,logFrom,impl->problems);
  countOther(r,impl->problems);
  if(!r.issues.empty() && r.status=="ok")
    r.status = "issues";
  results.push_back(r);
  statusOf[r.name] = r.status;
  return r;
  }

std::string DialogTest::describe(const Result& r) const {
  std::string t = r.name+" ["+r.status+"]";
  if(!r.npc.empty())
    t += " "+r.npc;
  t += "\n";
  if(!r.conditionBefore && r.conditionAfter)
    t += "  condition was false, the tool made it true\n";
  for(auto& a:r.applied)
    t += "  set: "+a+"\n";
  for(auto& u:r.unsolved)
    t += "  not set: "+u+"\n";
  if(r.executed)
    t += "  lines: "+std::to_string(r.outputs)+", commands: "+std::to_string(r.actions)+"\n";
  for(auto& i:r.issues)
    t += "  ! "+i+"\n";
  return t;
  }

std::string DialogTest::report() const {
  std::map<std::string,int> byStatus;
  for(auto& r:results)
    byStatus[r.status]++;

  std::string t;
  t += "DIALOG TEST REPORT\n";
  t += "==================\n";
  t += "runs: "+std::to_string(results.size())+"\n";
  for(auto& s:byStatus)
    t += "  "+s.first+": "+std::to_string(s.second)+"\n";

  // what the game lacks, the most common first
  // (made from the results, so a run that has been read back from the file of the progress is counted too)
  std::map<std::string,std::pair<int,std::string>> counted;
  for(auto& r:results) {
    for(auto& is:r.issues) {
      auto& p = counted[is];
      if(p.first==0)
        p.second = r.name;
      p.first++;
      }
    for(auto& u:r.unsolved) {
      auto& p = counted["condition: "+u];
      if(p.first==0)
        p.second = r.name;
      p.first++;
      }
    }
  std::vector<std::pair<std::string,std::pair<int,std::string>>> probs(counted.begin(),counted.end());
  std::stable_sort(probs.begin(),probs.end(),[](auto& a, auto& b){ return a.second.first>b.second.first; });
  t += "\nWHAT IS STILL MISSING (by the number of dialogs that run into it)\n";
  t += "--------------------------------------------------------------\n";
  if(probs.empty())
    t += "nothing found\n";
  for(auto& p:probs)
    t += std::to_string(p.second.first)+"x  "+p.first+"   (e.g. "+p.second.second+")\n";

  t += "\nDIALOGS THAT DID NOT WORK\n";
  t += "-------------------------\n";
  int shown = 0;
  for(auto& r:results) {
    if(r.status=="ok")
      continue;
    t += describe(r);
    if(++shown>=600) {
      t += "... (the list is cut)\n";
      break;
      }
    }
  if(shown==0)
    t += "none\n";
  return t;
  }

bool DialogTest::saveReport(const std::string& path) const {
  std::ofstream f(path,std::ios::binary|std::ios::trunc);
  if(!f)
    return false;
  const auto t = report();
  f.write(t.data(),std::streamsize(t.size()));
  return bool(f);
  }
