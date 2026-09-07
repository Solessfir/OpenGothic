#include "../../common/utils/saveslot.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

void check(bool ok, const char* message) {
  if(!ok) throw std::runtime_error(message);
  }

int main() {
  namespace fs=std::filesystem;
  const auto id=std::chrono::steady_clock::now().time_since_epoch().count();
  const auto directory=fs::temp_directory_path()/("opengothic-save-delete-test-"+std::to_string(id));
  try {
    check(fs::create_directory(directory),"Create isolated test directory");
    check(!SaveSlot::hasAny(directory),"Fresh installation has no saves");
    check(!SaveSlot::hasAny(directory/"missing"),"Missing save directory is harmless");
    std::ofstream(directory/"save_slot_1.sav.bak") << "backup";
    std::ofstream(directory/"save_slot_.sav") << "not a slot";
    std::ofstream(directory/"save_slot_test.sav") << "not a numbered slot";
    fs::create_directory(SaveSlot::path(directory,3));
    check(!SaveSlot::hasAny(directory),"Backups, malformed names and directories are not saves");
    std::ofstream(SaveSlot::path(directory,0)) << "quicksave";
    check(SaveSlot::hasAny(directory),"A quicksave alone enables the load preference");
    fs::remove(SaveSlot::path(directory,0));
    std::ofstream(SaveSlot::path(directory,1)) << "synthetic test save";
    std::ofstream(SaveSlot::path(directory,2)) << "keep this save";
    check(SaveSlot::hasAny(directory),"Manual saves enable the load preference");
    std::error_code error;
    check(SaveSlot::remove(directory,1,error) && !error,"Remove selected save file");
    check(!fs::exists(SaveSlot::path(directory,1)),"Deleted slot is absent");
    check(fs::exists(SaveSlot::path(directory,2)),"Other save remains intact");
    check(!SaveSlot::remove(directory,1,error) && error,"Missing slot reports failure");
    check(!SaveSlot::remove(directory,3,error) && error,"Directories cannot be deleted as saves");
    check(fs::is_directory(SaveSlot::path(directory,3)),"Rejected directory remains intact");
    fs::remove(SaveSlot::path(directory,2));
    fs::remove(SaveSlot::path(directory,3));
    check(!SaveSlot::hasAny(directory),"Removing the last save clears the load preference");
    fs::remove(directory/"save_slot_1.sav.bak");
    fs::remove(directory/"save_slot_.sav");
    fs::remove(directory/"save_slot_test.sav");
    fs::remove(directory);
    std::cout << "Save deletion tests passed\n";
    }
  catch(const std::exception& error) {
    std::cerr << error.what() << "\nTest files retained at " << directory << '\n';
    return 1;
    }
  }
