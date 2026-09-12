#include "utils/zipdirectory.h"
#include "utils/zipextract.h"
#include <cstdlib>
#include <iostream>
#include <string>

static void check(bool value, const char* message) {
  if(!value) {
    std::cerr << message << '\n';
    std::exit(1);
    }
  }

int main() {
  mz_zip_archive archive = {};
  check(mz_zip_writer_init_heap(&archive, 0, 0)!=0, "create archive");
  const char* names[] = {"world/", "world/npc/", "world/npc/0/", "world/npc/0/data",
                         "world/npc/1/", "world/npc/1/data", "world/npc_invalid/0/", "root"};
  for(auto name:names)
    check(mz_zip_writer_add_mem(&archive, name, nullptr, 0, 0)!=0, "add entry");
  std::vector<uint8_t> large(256*1024);
  for(size_t i=0; i<large.size(); ++i)
    large[i] = uint8_t(i%251);
  check(mz_zip_writer_add_mem(&archive, "compressed", large.data(), large.size(), MZ_BEST_SPEED)!=0, "add compressed entry");
  check(mz_zip_writer_add_mem(&archive, "stored", large.data(), large.size(), 0)!=0, "add stored entry");
  const auto longDirectory = std::string(600, 'a')+"/";
  check(mz_zip_writer_add_mem(&archive, (longDirectory+"0/").c_str(), nullptr, 0, 0)!=0, "add long entry");
  void* data = nullptr;
  size_t bytes = 0;
  check(mz_zip_writer_finalize_heap_archive(&archive, &data, &bytes)!=0, "finalize archive");
  mz_zip_writer_end(&archive);
  archive = {};
  check(mz_zip_reader_init_mem(&archive, data, bytes, 0)!=0, "open archive");
  check(ZipDirectory::size(archive, "world/npc/")==2, "count only direct children");
  check(ZipDirectory::size(archive, "world/npc/0/")==1, "count files");
  check(ZipDirectory::size(archive, "world/npc_invalid/")==1, "separate prefix");
  check(ZipDirectory::size(archive, "missing/")==0, "missing directory");
  check(ZipDirectory::size(archive, longDirectory)==1, "untruncated filenames");
  std::vector<uint8_t> extracted;
  for(const auto name:{"compressed", "stored"}) {
    const auto index = mz_zip_reader_locate_file(&archive, name, nullptr, 0);
    check(index>=0, "find extraction sample");
    ZipExtract::entry(archive, mz_uint(index), extracted);
    check(extracted==large, "extract multiple output chunks unchanged");
    }
  ZipExtract::entry(archive, 0, extracted);
  check(extracted.empty(), "empty entries clear old contents");
  try {
    ZipExtract::entry(archive, mz_zip_reader_get_num_files(&archive), extracted);
    check(false, "invalid archive entry must fail");
    }
  catch(const std::runtime_error&) {}
  const auto storedIndex = mz_uint(mz_zip_reader_locate_file(&archive, "stored", nullptr, 0));
  mz_zip_archive_file_stat stat = {};
  check(mz_zip_reader_file_stat(&archive, storedIndex, &stat)!=0, "locate stored payload");
  auto raw = static_cast<uint8_t*>(data);
  raw[size_t(stat.m_local_header_ofs)+30+std::string("stored").size()] ^= 1;
  try {
    ZipExtract::entry(archive, storedIndex, extracted);
    check(false, "CRC mismatch must fail");
    }
  catch(const std::runtime_error&) {}
  check(extracted.empty(), "failed extraction discards incomplete contents");
  mz_zip_reader_end(&archive);
  mz_free(data);

  std::cout << "Save archive tests passed\n";
  }
