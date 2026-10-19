#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace Compatibility {

/// A script that fails once fails again in every frame. The first few times of a kind are logged, the rest are not.
/// Use as: if(logBudget("deref")) Log::e(...)
inline bool logBudget(std::string_view kind, int limit = 8) {
  static std::unordered_map<std::string,int> used;
  return ++used[std::string(kind)]<=limit;
  }

}
