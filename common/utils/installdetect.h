#pragma once

#include <Tempest/Platform>
#include <string>

#ifdef __ANDROID__
#include <filesystem>
#include <vector>
#endif

class InstallDetect final {
  public:
    InstallDetect();

    std::u16string detectG2();
#if defined(__OSX__) || defined(__IOS__) || defined(__ANDROID__)
    static std::u16string applicationSupportDirectory();
#endif
#if defined(__IOS__) || defined(__ANDROID__)
    // Working directory for saves, settings and logs
    static std::u16string userDirectory();
#endif
#if defined(__ANDROID__)
    static std::filesystem::path androidInternalDataPath();
    // Game folder chosen by the launcher in game.txt; empty for the single-game layout
    static std::u16string androidGame();
    // Command-line options the launcher passes on the second line of game.txt
    static std::vector<std::string> androidArgs();
#endif

  private:
    std::u16string detectG2(std::u16string pfiles);

#ifdef __WINDOWS__
    static std::u16string programFiles(bool x86);
    std::u16string pfiles, pfilesX86;
#endif

#if defined(__OSX__) || defined(__IOS__) || defined(__ANDROID__)
    std::u16string appDir;
#endif
  };
