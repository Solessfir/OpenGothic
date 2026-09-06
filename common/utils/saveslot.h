#pragma once

#include <filesystem>
#include <string>
#include <system_error>

namespace SaveSlot {

inline std::filesystem::path path(const std::filesystem::path& directory, size_t slot) {
  return directory / ("save_slot_"+std::to_string(slot)+".sav");
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
