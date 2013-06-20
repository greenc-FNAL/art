#ifndef art_Framework_Core_PathManager_h
#define art_Framework_Core_PathManager_h
////////////////////////////////////////////////////////////////////////
// PathManager.
//
// Class to handle the processing of the configuration of modules in
// art, including the creation of paths and construction of modules as
// appropriate.
//
// Intended to be constructed early, prior to services, since
// TriggerNamesService will need some of the information herein at
// construction time.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/Path.h"
#include "art/Framework/Core/detail/ModuleFactory.h"
#include "art/Framework/Core/detail/ModuleConfigInfo.h"
#include "art/Framework/Principal/Actions.h"
#include "art/Framework/Services/Registry/ActivityRegistry.h"
#include "art/Persistency/Common/HLTGlobalStatus.h"
#include "art/Persistency/Provenance/MasterProductRegistry.h"
#include "art/Utilities/ScheduleID.h"
#include "fhiclcpp/ParameterSet.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

namespace art {
  class PathManager;
}

class art::PathManager {
public:
  typedef std::vector<std::unique_ptr<Path> > Paths;
  typedef std::vector<Worker *> Workers;
  typedef std::vector<std::string> vstring;

  PathManager(fhicl::ParameterSet const & procPS,
              MasterProductRegistry & preg,
              ActionTable & exceptActions,
              std::shared_ptr<ActivityRegistry> const & areg);

  vstring const & triggerPathNames() const;

  // These methods will trigger module construction.
  Path & endPath();
  Paths const & triggerPaths(ScheduleID sID);

  void resetAll(); // Reset trigger results ready for next event.

private:
  typedef std::map<std::string, std::shared_ptr<Worker>> WorkerMap;
  typedef std::vector<detail::ModuleConfigInfo const *> ModInfos;

  detail::ModuleConfigInfoMap fillAllModules_();
  vstring processPathConfigs_();
  // Returns true if path is a trigger path.
  bool processOnePathConfig_(std::string const & path_name,
                             vstring const & path_seq);
  void
  makeWorker_(detail::ModuleConfigInfo const * mci,
              WorkerMap & workers,
              std::vector<WorkerInPath> & pathWorkers);
  std::unique_ptr<Path> fillWorkers_(int bitpos,
                                     std::string const & pathName,
                                     ModInfos const & modInfos,
                                     HLTGlobalStatus & pathResults,
                                     WorkerMap & workers);

  fhicl::ParameterSet procPS_;
  MasterProductRegistry & preg_;
  ActionTable & exceptActions_;
  std::shared_ptr<ActivityRegistry> const & areg_;

  detail::ModuleFactory fact_;
  detail::ModuleConfigInfoMap allModules_;
  std::map<std::string, ModInfos> protoTrigPathMap_;
  ModInfos protoEndPathInfo_;
  vstring triggerPathNames_;
  WorkerMap endPathWorkers_;
  std::unique_ptr<Path> endPath_;
  std::map<ScheduleID, WorkerMap> triggerPathWorkers_; // Per-schedule.
  std::map<ScheduleID, Paths> triggerPaths_; // Per-schedule.
  std::map<ScheduleID, HLTGlobalStatus> triggerResults_; // Per-schedule.
  HLTGlobalStatus endPathResults_;
};

#endif /* art_Framework_Core_PathManager_h */

// Local Variables:
// mode: c++
// End:
