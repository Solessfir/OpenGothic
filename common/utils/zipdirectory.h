#pragma once

#include <miniz.h>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace ZipDirectory {

inline uint32_t size(mz_zip_archive& archive, std::string_view directory) {
  uint32_t count = 0;
  std::vector<char> buffer;
  for(mz_uint i=0; i<mz_zip_reader_get_num_files(&archive); ++i) {
    // Full file stats also decode timestamps and other metadata we do not need.
    const auto length = mz_zip_reader_get_filename(&archive, i, nullptr, 0);
    if(length==0)
      throw std::runtime_error("unable to locate entry in game archive");
    buffer.resize(length);
    if(mz_zip_reader_get_filename(&archive, i, buffer.data(), length)!=length)
      throw std::runtime_error("unable to read entry name in game archive");
    const std::string_view name(buffer.data(), length-1);
    if(name.size()<=directory.size() || !name.starts_with(directory))
      continue;
    const auto child = name.substr(directory.size());
    const auto separator = child.find('/');
    if(separator==std::string_view::npos || separator+1==child.size())
      ++count;
    }
  return count;
  }

}
