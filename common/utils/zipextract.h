#pragma once

#include <miniz.h>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <vector>

namespace ZipExtract {

inline void entry(mz_zip_archive& archive, mz_uint index, std::vector<uint8_t>& bytes) {
  bytes.clear();
  struct Output {
    std::vector<uint8_t>& bytes;
    std::exception_ptr error;
    } output{bytes, {}};
  // Extraction already reads the entry metadata.
  // Stream into the reusable buffer instead of decoding that metadata a second time just to get its size.
  const auto ok = mz_zip_reader_extract_to_callback(&archive, index,
      [](void* opaque, mz_uint64 offset, const void* data, size_t size) -> size_t {
        auto& output = *static_cast<Output*>(opaque);
        if(offset!=output.bytes.size() || size>output.bytes.max_size()-output.bytes.size())
          return 0;
        try {
          if(size>0) {
            const auto begin = static_cast<const uint8_t*>(data);
            output.bytes.insert(output.bytes.end(), begin, begin+size);
            }
          return size;
          }
        catch(...) {
          // Let miniz release its buffers before propagating an allocation error.
          output.error = std::current_exception();
          return 0;
          }
        }, &output, 0);
  if(!ok) {
    bytes.clear();
    if(output.error)
      std::rethrow_exception(output.error);
    throw std::runtime_error("unable to extract entry in game archive");
    }
  }

}
