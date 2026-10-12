#include "installdetect.h"

#include <Tempest/Platform>
#include <Tempest/SystemApi>
#include <Tempest/TextCodec>

#ifdef __ANDROID__
#include <Tempest/AndroidApi>
#endif

#ifdef __WINDOWS__
#include "windows.h"
#include "shlobj.h"
#include "shlwapi.h"
#endif

#include <cctype>
#include <cstring>
#include <fstream>
#include "utils/fileutil.h"

InstallDetect::InstallDetect() {
#ifdef __WINDOWS__
  pfiles    = programFiles(false);
  pfilesX86 = programFiles(true);
#endif
#if defined(__OSX__) || defined(__IOS__) || defined(__ANDROID__)
  appDir    = applicationSupportDirectory();
#endif
  }

std::u16string InstallDetect::detectG2() {
#if defined(__WINDOWS__)
  auto ret = detectG2(pfiles);
  if(ret.empty())
    ret = detectG2(pfilesX86);
  return ret;
#elif defined(__ANDROID__)
  auto name = androidGame();
  auto game = appDir;
  if(!game.empty() && game.back()!=u'/')
    game.push_back(u'/');
  game += name.empty() ? u"Gothic2" : name;
  if(FileUtil::exists(game))
    return game;
  return u"";
#elif defined(__OSX__) || defined(__IOS__)
  if(FileUtil::exists(appDir))
    return appDir;
  return u"";
#else
  return u"";
#endif

  }

#ifdef __ANDROID__
std::filesystem::path InstallDetect::androidInternalDataPath() {
  auto path = Tempest::AndroidApi::internalDataPath();
  std::filesystem::create_directories(path);
  return path;
  }
#endif

std::u16string InstallDetect::detectG2(std::u16string pfiles) {
  if(pfiles.empty())
    return u"";
  auto steam = pfiles+u"/Steam/steamapps/common/Gothic II/";
  if(FileUtil::exists(steam))
    return steam;
  auto akela = pfiles+u"/Akella Games/Gothic II/";
  if(FileUtil::exists(akela))
    return akela;
  return u"";
  }

#ifdef __WINDOWS__
std::u16string InstallDetect::programFiles(bool x86) {
  WCHAR path[MAX_PATH]={};
  if(FAILED(SHGetFolderPathW(NULL, (x86 ? CSIDL_PROGRAM_FILESX86 : CSIDL_PROGRAM_FILES), NULL, 0, path)))
    return u"";
  std::u16string ret;
  size_t len=0;
  for(;path[len];++len);

  ret.resize(len);
  std::memcpy(&ret[0],path,len*sizeof(char16_t));
  return ret;
  }
#endif

#ifdef __ANDROID__
std::u16string InstallDetect::applicationSupportDirectory() {
  auto path = Tempest::AndroidApi::externalDataPath();
  if(path.empty())
    path = Tempest::AndroidApi::internalDataPath();
  return path.u16string();
  }

std::u16string InstallDetect::androidGame() {
  std::ifstream file(std::filesystem::path(applicationSupportDirectory())/"game.txt");
  std::string   name;
  std::getline(file,name);
  while(!name.empty() && std::isspace(uint8_t(name.back())))
    name.pop_back();
  // A plain folder name, never a path
  if(name.empty() || name.size()>32 || name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos)
    return u"";
  return std::u16string(name.begin(),name.end());
  }
#endif

#if defined(__IOS__) || defined(__ANDROID__)
std::u16string InstallDetect::userDirectory() {
  auto dir = applicationSupportDirectory();
#if defined(__ANDROID__)
  // Launcher games keep their saves and settings outside of the game data, so reinstalling a game keeps them
  auto game = androidGame();
  if(!game.empty() && !dir.empty()) {
    auto user = std::filesystem::path(dir)/"User"/std::filesystem::path(game);
    std::error_code ec;
    std::filesystem::create_directories(user,ec);
    if(!ec)
      return user.u16string();
    }
#endif
  return dir;
  }
#endif
