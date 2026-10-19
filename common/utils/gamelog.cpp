#include "gamelog.h"

#include <deque>
#include <mutex>

namespace {

constexpr size_t Capacity = 6000;

struct Buffer {
  std::mutex                  sync;
  std::deque<GameLog::Line>   lines;
  uint64_t                    next = 1;
  };

Buffer& buffer() {
  static Buffer b;
  return b;
  }

}

void GameLog::push(Level level, const char* text) {
  auto& b = buffer();
  std::lock_guard<std::mutex> g(b.sync);
  Line l;
  l.serial = b.next++;
  l.level  = level;
  l.text   = text==nullptr ? "" : text;
  b.lines.push_back(std::move(l));
  while(b.lines.size()>Capacity)
    b.lines.pop_front();
  }

uint64_t GameLog::since(uint64_t after, size_t max, std::vector<Line>& out) {
  auto& b = buffer();
  std::lock_guard<std::mutex> g(b.sync);
  out.clear();
  for(auto& l:b.lines) {
    if(l.serial<=after)
      continue;
    out.push_back(l);
    if(out.size()>=max)
      break;
    }
  return b.next-1;
  }

uint64_t GameLog::serial() {
  auto& b = buffer();
  std::lock_guard<std::mutex> g(b.sync);
  return b.next-1;
  }
