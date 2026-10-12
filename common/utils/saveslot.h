#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <algorithm>
#include <vector>

namespace SaveSlot {

constexpr size_t QuickFirst=1000;
constexpr size_t QuickCount=20;

inline std::filesystem::path path(const std::filesystem::path& directory, size_t slot) {
  if(slot>=QuickFirst && slot<QuickFirst+QuickCount)
    return directory / ("save_quick_"+std::to_string(slot-QuickFirst+1)+".sav");
  return directory / ("save_slot_"+std::to_string(slot)+".sav");
  }

inline std::vector<size_t> quickSlots(const std::filesystem::path& directory) {
  std::vector<std::pair<std::filesystem::file_time_type,size_t>> files;
  for(size_t i=0;i<=QuickCount;++i) {
    const size_t slot=i==0 ? 0 : QuickFirst+i-1;
    std::error_code error;
    const auto file=path(directory,slot);
    if(!std::filesystem::is_regular_file(std::filesystem::symlink_status(file,error))) continue;
    const auto time=std::filesystem::last_write_time(file,error);
    if(!error) files.emplace_back(time,slot);
    }
  std::sort(files.rbegin(),files.rend());
  std::vector<size_t> slots;
  for(const auto& file:files) slots.push_back(file.second);
  return slots;
  }

inline size_t nextQuickSlot(const std::filesystem::path& directory, int count) {
  count=std::clamp(count,0,int(QuickCount));
  if(count==0) return 0;
  size_t oldest=size_t(-1);
  auto oldestTime=std::filesystem::file_time_type::max();
  for(int i=0;i<count;++i) {
    const auto slot=QuickFirst+size_t(i);
    const auto file=path(directory,slot);
    std::error_code error;
    const auto status=std::filesystem::symlink_status(file,error);
    if(status.type()==std::filesystem::file_type::not_found) return slot;
    if(error || !std::filesystem::is_regular_file(status)) continue;
    const auto time=std::filesystem::last_write_time(file,error);
    if(!error && time<oldestTime) { oldest=slot; oldestTime=time; }
    }
  return oldest;
  }

inline bool hasAny(const std::filesystem::path& directory) {
  std::error_code error;
  std::filesystem::directory_iterator it(directory,error), end;
  for(; !error && it!=end; it.increment(error)) {
    const auto name=it->path().filename().u8string();
    std::u8string_view slot=name;
    if((!slot.starts_with(u8"save_slot_") && !slot.starts_with(u8"save_quick_")) || !slot.ends_with(u8".sav"))
      continue;
    slot.remove_prefix(slot.starts_with(u8"save_quick_") ? 11 : 10);
    slot.remove_suffix(4);
    if(slot.empty() || slot.find_first_not_of(u8"0123456789")!=slot.npos)
      continue;
    if(std::filesystem::is_regular_file(it->symlink_status(error)))
      return true;
    }
  return false;
  }

// Remove only a regular save file, never a directory or a symbolic link.
inline bool remove(const std::filesystem::path& directory, size_t slot, std::error_code& error) {
  error.clear();
  const auto file = path(directory,slot);
  const auto status = std::filesystem::symlink_status(file,error);
  if(error)
    return false;
  if(!std::filesystem::is_regular_file(status)) {
    error = std::make_error_code(std::errc::invalid_argument);
    return false;
    }
  const bool removed = std::filesystem::remove(file,error);
  if(!removed && !error)
    error = std::make_error_code(std::errc::no_such_file_or_directory);
  return removed && !error;
  }

}
