#pragma once

#include "atomicsave.h"
#include <miniz.h>
#include <fstream>
#include <string>
#include <string_view>
#include <cstdint>
#include <vector>

class SaveSnapshot {
  public:
    class TooLarge : public std::runtime_error {
      public:
        TooLarge() : std::runtime_error("save snapshot exceeds memory budget") {}
      };

    explicit SaveSnapshot(size_t budget = 128*1024*1024) : budget(budget) {}
    SaveSnapshot(SaveSnapshot&&) = default;
    SaveSnapshot& operator=(SaveSnapshot&&) = default;

    void add(std::string_view name, const void* bytes, size_t size) {
      if(size>budget-data.size() || entries.size()>=262144)
        throw TooLarge();
      // Reserve once to avoid temporarily keeping two large buffers while growing.
      if(size>0 && data.capacity()==0)
        data.reserve(budget);
      const auto offset = data.size();
      if(size>0) {
        auto src = static_cast<const uint8_t*>(bytes);
        data.insert(data.end(), src, src+size);
        }
      entries.push_back({std::string(name), offset, size});
      }

    size_t byteSize() const { return data.size(); }

    void write(const std::filesystem::path& destination) const {
      AtomicSave::replace(destination, [&](const std::filesystem::path& temporary) {
        struct Output {
          std::ofstream file;
          uint64_t offset = 0;
          } output{std::ofstream(temporary, std::ios::binary | std::ios::trunc)};
        auto& file = output.file;
        if(!file)
          throw std::runtime_error("unable to open temporary save");
        mz_zip_archive archive = {};
        archive.m_pIO_opaque = &output;
        archive.m_pWrite = [](void* opaque, mz_uint64 offset, const void* bytes, size_t size) -> size_t {
          auto& output = *static_cast<Output*>(opaque);
          if(output.offset!=offset)
            return 0;
          output.file.write(static_cast<const char*>(bytes), std::streamsize(size));
          if(!output.file)
            return 0;
          output.offset += size;
          return size;
          };
        if(!mz_zip_writer_init_v2(&archive, 0, 0))
          throw std::runtime_error("unable to create save archive");
        try {
          for(const auto& entry:entries) {
            const auto bytes = entry.size==0 ? nullptr : data.data()+entry.offset;
            const auto level = entry.size>256 ? MZ_BEST_SPEED : MZ_NO_COMPRESSION;
            if(!mz_zip_writer_add_mem(&archive, entry.name.c_str(), bytes, entry.size, mz_uint(level)))
              throw std::runtime_error("unable to write entry in save archive");
            }
          if(!mz_zip_writer_finalize_archive(&archive))
            throw std::runtime_error("unable to finalize save archive");
          }
        catch(...) {
          mz_zip_writer_end(&archive);
          throw;
          }
        mz_zip_writer_end(&archive);
        file.flush();
        if(!file)
          throw std::runtime_error("unable to flush save archive");
        file.close();
        if(!file)
          throw std::runtime_error("unable to close save archive");
        });
      }

  private:
    struct Entry {
      std::string name;
      size_t offset;
      size_t size;
      };
    size_t budget;
    std::vector<uint8_t> data;
    std::vector<Entry> entries;
  };
