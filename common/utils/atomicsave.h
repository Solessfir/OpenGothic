#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace AtomicSave {

template<class Write>
void replace(const std::filesystem::path& destination, Write write) {
  static std::atomic_uint64_t serial{0};
  // Drop folders left behind when the app was killed during an earlier save of this slot
  std::error_code ignore;
  const auto prefix = destination.filename().string()+".pending-";
  for(auto& entry:std::filesystem::directory_iterator(destination.parent_path(), ignore))
    if(entry.path().filename().string().starts_with(prefix))
      std::filesystem::remove_all(entry.path(), ignore);
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  std::filesystem::path directory;
  for(unsigned attempt=0;; ++attempt) {
    if(attempt==100)
      throw std::runtime_error("unable to allocate temporary save directory");
    directory = destination;
    directory += ".pending-"+std::to_string(stamp)+"-"+std::to_string(serial++);
    if(std::filesystem::create_directory(directory))
      break;
    }
  const auto temporary = directory / "archive.tmp";
  try {
    write(temporary);
    // Both paths are on the same filesystem. Never delete the previous save first.
    std::filesystem::rename(temporary, destination);
    }
  catch(...) {
    std::error_code error;
    std::filesystem::remove(temporary, error);
    std::filesystem::remove(directory, error);
    throw;
    }
  std::error_code error;
  std::filesystem::remove(directory, error);
  }

}
