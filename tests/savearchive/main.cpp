#include "utils/zipdirectory.h"
#include "utils/savesnapshot.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <future>

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

  auto directory = std::filesystem::temp_directory_path() /
      ("opengothic-save-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  check(std::filesystem::create_directory(directory), "create test directory");
  auto target = directory / "save.sav";
  {
    SaveSnapshot snapshot(4096);
    std::string source(1024, 'A');
    snapshot.add("world/", nullptr, 0);
    snapshot.add("world/state", source.data(), source.size());
    source.assign(1024, 'B');
    auto task = std::async(std::launch::async, [target,snapshot=std::move(snapshot)] { snapshot.write(target); });
    task.get();
  }
  auto verify = [&](char expected = 'A') {
    mz_zip_archive reader = {};
    check(mz_zip_reader_init_file(&reader, target.string().c_str(), 0)!=0, "read saved snapshot");
    check(mz_zip_validate_archive(&reader, 0)!=0, "validate ZIP contents and CRCs");
    std::string contents(1024, '\0');
    check(mz_zip_reader_extract_file_to_mem(&reader, "world/state", contents.data(), contents.size(), 0)!=0, "extract snapshot");
    check(contents==std::string(1024, expected), "snapshot owns immutable data");
    check(ZipDirectory::size(reader, "world/")==1, "snapshot preserves directory layout");
    mz_zip_reader_end(&reader);
    };
  verify();
  try {
    AtomicSave::replace(target, [](const auto& temporary) {
      std::ofstream file(temporary, std::ios::binary);
      file << "incomplete replacement";
      throw std::runtime_error("simulated disk failure");
      });
    check(false, "injected error must propagate");
    }
  catch(const std::runtime_error&) {}
  verify();
  check(std::distance(std::filesystem::directory_iterator(directory), std::filesystem::directory_iterator())==1,
        "failed writes remove only their temporary files");
  try {
    SaveSnapshot tooLarge(8);
    tooLarge.add("large", "123456789", 9);
    check(false, "snapshot budget must be enforced");
    }
  catch(const SaveSnapshot::TooLarge&) {}
  verify();
  SaveSnapshot replacement(4096);
  const std::string contents(1024, 'C');
  replacement.add("world/", nullptr, 0);
  replacement.add("world/state", contents.data(), contents.size());
  replacement.write(target);
  verify('C');
  std::filesystem::remove(target);
  std::filesystem::remove(directory);
  std::cout << "Save archive tests passed\n";
  }
