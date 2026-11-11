#pragma once

#include <zenkit/addon/daedalus.hh>

#include <Tempest/SoundDevice>
#include <Tempest/SoundEffect>
#include <optional>

class GameMusic final {
  public:
    GameMusic();
    GameMusic(const GameMusic&)=delete;
    ~GameMusic();

    static GameMusic& inst();

    enum Music : uint8_t {
      SysLoading
      };

    enum Tags : uint8_t {
      Day = 0,
      Ngt = 1<<0,

      Std = 0,
      Fgt = 1<<1,
      Thr = 1<<2
      };

    static Tags mkTags(Tags daytime,Tags mode);

    void      setEnabled(bool e);
    bool      isEnabled() const;
    void      setMusic(Music m);
    void      setMusic(const zenkit::IMusicTheme &theme, Tags t);
    void      clearZoneMusic();
    void      stopMusic();
    void      overrideTrack(uint32_t instance);
    void      clearTrackOverride();

  private:
    struct MusicProvider;
    struct OpenGothicMusicProvider;
    struct GothicKitMusicProvider;
    struct RecordedMusic;

    void      setupSettings();
    void      playCurrent();

    static GameMusic* instance;

    struct {
      zenkit::IMusicTheme theme = {};
      Tags                tags  = Tags::Std;
      } currentMusic;

    int                  provider = -1;
    Tempest::SoundDevice device;
    Tempest::SoundEffect sound;
    MusicProvider*       impl = nullptr;
    bool                 enabled = true;
    bool                 zoneMusicActive = true;
    Tempest::SoundEffect recordedSound, outgoingSound;
    RecordedMusic*       recordedImpl = nullptr;
    std::string          recordedFile;
    int32_t              recordedFadeOut = 0;
    std::optional<zenkit::IMusicTheme> overrideTheme;
  };
