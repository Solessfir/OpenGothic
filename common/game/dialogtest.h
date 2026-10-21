#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class GameScript;
class Npc;

namespace zenkit {
  class DaedalusSymbol;
  struct IInfo;
  }

/// The developer tool for the dialogs and the scenes of a mod (the tab Dialogs of mods/devtools, comcheck dlg):
/// reads the condition of a dialog from the bytecode of the scripts, sets what it needs (the variables, the dialogs that were told,
/// the status of the quest, the items, the place, the time), runs the dialog and lets its queue of commands go on without the waiting,
/// and collects what is missing in the game for the mod into a report.
class DialogTest final {
  public:
    explicit DialogTest(GameScript& sc);
    ~DialogTest();

    struct Row final {
      std::string name;          // the instance of the dialog
      std::string npc;           // the instance of the character
      std::string npcName;       // the name of the character, empty if he is not in the world
      std::string description;
      int         nr        = 0;
      bool        important = false;
      bool        permanent = false;
      bool        trade     = false;
      bool        known     = false;
      bool        inWorld   = false;
      std::string status;        // of the last run: "", ok, error, no-npc, cond-false, ...
      };

    /// The words of the filter must all be in the name of the dialog, of the character or in the description; "status:error" filters by
    /// the last run. The result is sorted by the character.
    std::vector<Row> list(std::string_view filter, size_t skip, size_t max, size_t* total = nullptr);

    /// What the condition of the dialog needs, in words, with what the tool can do about it.
    std::string analyze(std::string_view info);

    struct Result final {
      std::string              name;
      std::string              npc;
      std::string              status;      // ok, cond-false, cond-unsolved, no-npc, no-world, unknown, error
      bool                     conditionBefore = false;
      bool                     conditionAfter  = false;
      bool                     executed        = false;
      int                      outputs         = 0;   // the lines that were said
      int                      actions         = 0;   // the commands of the queues that went through
      std::vector<std::string> applied;               // what the tool has set for the condition
      std::vector<std::string> unsolved;              // what it could not set
      std::vector<std::string> issues;                // errors of the scripts, the engine functions that are missing, missing animations...
      };

    /// solve: set what the condition needs. execute: run the dialog (and what it offers as choices) when the condition holds.
    Result run(std::string_view info, bool solve, bool execute);
    /// A function of the scripts that is a scene (an event, a cutscene): called with self = the player, the commands that it queues are
    /// let go on without the waiting.
    Result runFunction(std::string_view function, std::string_view selfNpc);

    /// Forget the results of the last runs (and the file of the progress)
    void   reset();
    /// Every run is written to dialogtest_progress.txt at once. After a crash of the game the results are read back and the run goes on
    /// with the dialogs that have no result; the dialog that was running when the game died is marked "crashed". Returns the number of
    /// results that were read.
    size_t loadProgress();
    size_t resultCount() const { return results.size(); }
    /// The report of all runs: the summary, what the game lacks (by how many dialogs it matters), the dialogs that did not work
    std::string report() const;
    bool   saveReport(const std::string& path) const;
    std::string lastStatus(const std::string& info) const;
    std::string describe(const Result& r) const;

    struct Impl;
    /// true while the tool runs a dialog: the scripts must not end the game or play a video
    static inline bool active = false;
    /// true during the run of many dialogs: the scripts that the dialogs have prepared (the event managers of the quests run every frame)
    /// must not end the game or play a video between two dialogs either
    static inline bool guardExit = false;
    /// the character of the dialog that runs: the scripts ask whether he is in the state of the talk
    static inline Npc*  talkNpc = nullptr;

  private:
    Result runImpl(std::string_view info, bool solve, bool execute);
    void   appendProgress(const Result& r) const;

    std::unique_ptr<Impl> impl;
    GameScript&           sc;
    std::vector<Result>   results;
    std::map<std::string,std::string> statusOf;
  };
