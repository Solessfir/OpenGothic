#include "utils/zipdirectory.h"
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
  mz_zip_reader_end(&archive);
  mz_free(data);
  std::cout << "Save archive tests passed\n";
  }
