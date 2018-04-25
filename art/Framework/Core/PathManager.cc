#include "art/Framework/Core/PathManager.h"
// vim: set sw=2 expandtab :

#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleBase.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Core/ModuleType.h"
#include "art/Framework/Core/Path.h"
#include "art/Framework/Core/PathsInfo.h"
#include "art/Framework/Core/UpdateOutputCallbacks.h"
#include "art/Framework/Core/WorkerInPath.h"
#include "art/Framework/Core/WorkerT.h"
#include "art/Framework/Core/detail/ModuleGraphInfoMap.h"
#include "art/Framework/Core/detail/graph_algorithms.h"
#include "art/Framework/Principal/Actions.h"
#include "art/Framework/Principal/Worker.h"
#include "art/Framework/Principal/WorkerParams.h"
#include "art/Framework/Services/Registry/ActivityRegistry.h"
#include "art/Utilities/CPCSentry.h"
#include "art/Utilities/Globals.h"
#include "art/Utilities/PerScheduleContainer.h"
#include "art/Utilities/PluginSuffixes.h"
#include "art/Utilities/ScheduleID.h"
#include "art/Utilities/bold_fontify.h"
#include "art/Version/GetReleaseVersion.h"
#include "canvas/Persistency/Common/HLTGlobalStatus.h"
#include "canvas/Persistency/Provenance/ModuleDescription.h"
#include "canvas/Utilities/DebugMacros.h"
#include "canvas/Utilities/Exception.h"
#include "cetlib/HorizontalRule.h"
#include "cetlib/LibraryManager.h"
#include "cetlib/container_algorithms.h"
#include "cetlib/detail/wrapLibraryManagerException.h"
#include "cetlib/ostream_handle.h"
#include "fhiclcpp/ParameterSet.h"
#include "fhiclcpp/types/detail/validationException.h"
#include "hep_concurrency/tsan.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace std;
using namespace std::string_literals;

using fhicl::ParameterSet;

namespace art {

  PathManager::~PathManager()
  {
    for (auto& label_and_worker : workerSet_) {
      delete label_and_worker.second;
      label_and_worker.second = nullptr;
    }
    for (auto& label_and_module : moduleSet_) {
      delete label_and_module.second;
      label_and_module.second = nullptr;
    }
    for (auto& tri : triggerResultsInserter_) {
      if (tri) {
        delete &tri->module();
      }
    }
  }

  PathManager::PathManager(ParameterSet const& procPS,
                           UpdateOutputCallbacks& outputCallbacks,
                           ProductDescriptions& productsToProduce,
                           ActionTable const& exceptActions,
                           ActivityRegistry const& actReg)
    : outputCallbacks_{outputCallbacks}
    , exceptActions_{exceptActions}
    , actReg_{actReg}
    , lm_{Suffixes::module()}
    , procPS_{procPS}
    , triggerPathNames_()
    , moduleSet_{}
    , workerSet_{}
    , triggerPathsInfo_{static_cast<ScheduleID::size_type>(
        Globals::instance()->nschedules())}
    , endPathInfo_{}
    , triggerResultsInserter_{}
    , productsToProduce_{productsToProduce}
    , processName_{procPS.get<string>("process_name"s, ""s)}
    , allModules_{}
    , trigger_paths_config_{}
    , end_paths_config_{}
    , protoTrigPathLabelMap_{}
    , protoEndPathLabels_()
  {
    triggerResultsInserter_.expand_to_num_schedules();
    //
    //  Collect trigger_paths and end_paths.
    {
      vector<string> tmp;
      if (procPS_.get_if_present("physics.trigger_paths", tmp)) {
        trigger_paths_config_.reset(new set<string>);
        trigger_paths_config_->insert(tmp.cbegin(), tmp.cend());
      }
      tmp.clear();
      if (procPS_.get_if_present("physics.end_paths", tmp)) {
        end_paths_config_.reset(new set<string>);
        end_paths_config_->insert(tmp.cbegin(), tmp.cend());
      }
    }
    //
    //  Collect all the module information.
    //
    {
      ostringstream es;
      int idx = 0;
      for (auto const& path_table_name : vector<string>{"physics.producers"s,
                                                        "physics.filters"s,
                                                        "physics.analyzers"s,
                                                        "outputs"s}) {
        ++idx;
        auto module_type = static_cast<ModuleType>(idx);
        for (auto const& module_label :
             procPS_.get<ParameterSet>(path_table_name, {}).get_names()) {
          try {
            auto const& module_pset = procPS_.get<fhicl::ParameterSet>(
              path_table_name + '.' + module_label);
            auto const lib_spec = module_pset.get<string>("module_type");
            auto const actualModType = loadModuleType_(lib_spec);
            if (actualModType != module_type) {
              es << "  ERROR: Module with label " << module_label << " of type "
                 << lib_spec << " is configured as a "
                 << ModuleType_to_string(module_type)
                 << " but defined in code as a "
                 << ModuleType_to_string(actualModType) << ".\n";
            }
            ModuleConfigInfo mci{path_table_name,
                                 module_type,
                                 loadModuleThreadingType_(lib_spec),
                                 module_pset,
                                 lib_spec};
            auto result = allModules_.emplace(module_label, move(mci));
            if (!result.second) {
              es << "  ERROR: Module label " << module_label
                 << " has been used in "
                 << result.first->second.configTableName_ << " and "
                 << path_table_name << ".\n";
            }
          }
          catch (exception const& e) {
            es << "  ERROR: Configuration of module with label " << module_label
               << " encountered the following error:\n"
               << e.what() << "\n";
          }
        }
      }
      if (!es.str().empty()) {
        throw Exception(errors::Configuration)
          << "The following were encountered while processing the module "
             "configurations:\n"
          << es.str();
      }
    }
    //
    //  Collect all the path information.
    //
    {
      ostringstream es;
      //
      //  Get the physics table.
      //
      auto const physics = procPS_.get<ParameterSet>("physics", {});
      //
      //  Get the non-special entries, should be user-specified paths
      //  (labeled fhicl sequences of module labels).
      //
      // Note: The ParameterSet::get_names() routine returns
      // vector<string> by value, so we must make sure to iterate over
      // the same returned object.
      auto const physics_names = physics.get_names();
      set<string> const special_parms = {
        "producers"s, "filters"s, "analyzers"s, "trigger_paths"s, "end_paths"s};
      vector<string> path_names;
      set_difference(physics_names.cbegin(),
                     physics_names.cend(),
                     special_parms.cbegin(),
                     special_parms.cend(),
                     back_inserter(path_names));
      //
      //  Check that each path in trigger_paths and end_paths actually exists.
      //
      if (trigger_paths_config_) {
        vector<string> unknown_paths;
        set_difference(trigger_paths_config_->cbegin(),
                       trigger_paths_config_->cend(),
                       path_names.cbegin(),
                       path_names.cend(),
                       back_inserter(unknown_paths));
        for (auto const& path : unknown_paths) {
          es << "ERROR: Unknown path " << path
             << " specified by user in trigger_paths.\n";
        }
      }
      if (end_paths_config_) {
        vector<string> missing_paths;
        set_difference(end_paths_config_->cbegin(),
                       end_paths_config_->cend(),
                       path_names.cbegin(),
                       path_names.cend(),
                       back_inserter(missing_paths));
        for (auto const& path : missing_paths) {
          es << "ERROR: Unknown path " << path
             << " specified by user in end_paths.\n";
        }
      }
      //
      //  Make sure the path names are keys to fhicl sequences.
      //
      {
        map<string, string> bad_names;
        for (auto const& name : path_names) {
          if (physics.is_key_to_sequence(name)) {
            continue;
          }
          string const type = physics.is_key_to_table(name) ? "table" : "atom";
          bad_names.emplace(name, type);
        }
        if (!bad_names.empty()) {
          string msg =
            "\nYou have specified the following unsupported parameters in the\n"
            "\"physics\" block of your configuration:\n\n";
          cet::for_all(bad_names, [&msg](auto const& name) {
            msg.append("   \"physics." + name.first + "\"   (" + name.second +
                       ")\n");
          });
          msg.append("\n");
          msg.append("Supported parameters include the following tables:\n");
          msg.append("   \"physics.producers\"\n");
          msg.append("   \"physics.filters\"\n");
          msg.append("   \"physics.analyzers\"\n");
          msg.append("and sequences. Atomic configuration parameters are not "
                     "allowed.\n\n");
          throw Exception(errors::Configuration) << msg;
        }
      }
      //
      //  Process each path.
      //
      auto remove_filter_action = [](auto const& spec) {
        auto pos = spec.find_first_not_of("!-");
        if (pos > 1) {
          throw Exception(errors::Configuration)
            << "Module label " << spec << " is illegal.\n";
        }
        return spec.substr(pos);
      };
      set<string> specified_modules;
      {
        enum class mod_cat_t { UNSET, OBSERVER, MODIFIER };
        size_t num_end_paths = 0;
        for (auto const& path_name : path_names) {
          mod_cat_t cat = mod_cat_t::UNSET;
          auto const path = physics.get<vector<string>>(path_name);
          for (auto const& modname_filterAction : path) {
            auto const label = remove_filter_action(modname_filterAction);
            specified_modules.insert(label);
            auto iter = allModules_.find(label);
            if (iter == allModules_.end()) {
              es << "  ERROR: Entry " << modname_filterAction << " in path "
                 << path_name << " refers to a module label " << label
                 << " which is not configured.\n";
              continue;
            }
            // map<string, ModuleConfigInfo>
            auto& mci = iter->second;
            auto mtype = is_observer(mci.moduleType_) ? mod_cat_t::OBSERVER :
                                                        mod_cat_t::MODIFIER;
            if ((cat != mod_cat_t::UNSET) && (cat != mtype)) {
              // Warn on mixed module types.
              es << "  ERROR: Entry " << modname_filterAction << " in path "
                 << path_name << " is a"
                 << (cat == mod_cat_t::OBSERVER ? " modifier" : "n observer")
                 << " while previous entries in the same path are all "
                 << (cat == mod_cat_t::OBSERVER ? "observers" : "modifiers")
                 << ".\n";
            }
            if (cat == mod_cat_t::UNSET) {
              // We now know path is not empty, categorize it.
              cat = mtype;
              // If optional triggers_paths or end_paths used, and this path is
              // not on them, ignore it.
              if (cat == mod_cat_t::MODIFIER) {
                if (trigger_paths_config_ &&
                    (trigger_paths_config_->find(path_name) ==
                     trigger_paths_config_->cend())) {
                  mf::LogInfo("DeactivatedPath")
                    << "Detected trigger path \"" << path_name
                    << "\" which was not found in\n"
                    << "parameter \"physics.trigger_paths\". Path will be "
                       "ignored.";
                  for (auto const& mod : path) {
                    specified_modules.erase(remove_filter_action(mod));
                  }
                  break;
                }
                if (end_paths_config_ && (end_paths_config_->find(path_name) !=
                                          end_paths_config_->cend())) {
                  es << "  ERROR: Path '" << path_name
                     << "' is configured as an end path but is actually a "
                        "trigger path.";
                }
                triggerPathNames_.push_back(path_name);
              } else {
                if (end_paths_config_ && (end_paths_config_->find(path_name) ==
                                          end_paths_config_->cend())) {
                  mf::LogInfo("DeactivatedPath")
                    << "Detected end path \"" << path_name
                    << "\" which was not found in\n"
                    << "parameter \"physics.end_paths\". "
                    << "Path will be ignored.";
                  for (auto const& mod : path) {
                    specified_modules.erase(remove_filter_action(mod));
                  }
                  break;
                }
                if (trigger_paths_config_ &&
                    (trigger_paths_config_->find(path_name) !=
                     trigger_paths_config_->cend())) {
                  es << "  ERROR: Path '" << path_name
                     << "' is configured as a trigger path but is actually an "
                        "end path.";
                }
              }
            }
            auto filteract = WorkerInPath::FilterAction::Normal;
            if (modname_filterAction[0] == '!') {
              filteract = WorkerInPath::FilterAction::Veto;
            } else if (modname_filterAction[0] == '-') {
              filteract = WorkerInPath::FilterAction::Ignore;
            }
            if (mci.moduleType_ != ModuleType::filter &&
                filteract != WorkerInPath::Normal) {
              es << "  ERROR: Module " << label << " in path " << path_name
                 << " is" << (cat == mod_cat_t::OBSERVER ? " an " : " a ")
                 << ModuleType_to_string(mci.moduleType_)
                 << " and cannot have a '!' or '-' prefix.\n";
            }
            if (cat == mod_cat_t::MODIFIER) {
              // Trigger path.
              protoTrigPathLabelMap_[path_name].emplace_back(label, filteract);
            } else {
              protoEndPathLabels_.emplace_back(label, filteract);
            }
          }
          if (cat == mod_cat_t::OBSERVER) {
            ++num_end_paths;
          }
        }
        if (num_end_paths > 1) {
          mf::LogInfo("PathConfiguration")
            << "Multiple end paths have been combined into one end path,\n"
            << "\"end_path\" since order is irrelevant.\n";
        }
      }
      //
      //  Complain about unused modules.
      //
      {
        set<string> all_module_labels;
        for (auto const& val : allModules_) {
          all_module_labels.insert(val.first);
        }
        vector<string> unused_modules;
        set_difference(all_module_labels.cbegin(),
                       all_module_labels.cend(),
                       specified_modules.cbegin(),
                       specified_modules.cend(),
                       back_inserter(unused_modules));
        if (!unused_modules.empty()) {
          ostringstream us;
          us << "The following module label"
             << ((unused_modules.size() == 1) ? " is" : "s are")
             << " either not assigned to any path,\n"
             << "or "
             << ((unused_modules.size() == 1ull) ? "it has" : "they have")
             << " been assigned to ignored path(s):\n"
             << "'" << unused_modules.front() << "'";
          for (auto i = unused_modules.cbegin() + 1, e = unused_modules.cend();
               i != e;
               ++i) {
            us << ", '" << *i << "'";
          }
          mf::LogInfo("path") << us.str();
          // Remove configuration info for unused modules
          for (auto const& unused_module : unused_modules) {
            allModules_.erase(unused_module);
          }
        }
      }
      //
      // Check for fatal errors.
      //
      if (!es.str().empty()) {
        throw Exception(errors::Configuration, "Path configuration: ")
          << "The following were encountered while processing path "
             "configurations:\n"
          << es.str();
      }
    }
  }

  vector<string> const&
  PathManager::triggerPathNames() const
  {
    return triggerPathNames_;
  }

  void
  PathManager::createModulesAndWorkers()
  {
    //  For each configured schedule, create the trigger paths and the workers
    //  on each path.
    //
    //  Note: Only schedule module workers are unique to each schedule,
    //        all other module workers are singletons.
    {
      auto const nschedules =
        static_cast<ScheduleID::size_type>(Globals::instance()->nschedules());
      for (auto si = ScheduleID::first(); si < ScheduleID(nschedules);
           si = si.next()) {
        auto& pinfo = triggerPathsInfo_[si];
        pinfo.pathResults() = HLTGlobalStatus(triggerPathNames_.size());
        int bitPos = 0;
        for (auto const& val : protoTrigPathLabelMap_) {
          auto const& path_name = val.first;
          auto const& worker_config_infos = val.second;
          vector<WorkerInPath> wips;
          fillWorkers_(si, bitPos, worker_config_infos, wips, pinfo.workers());
          pinfo.paths().push_back(new Path{exceptActions_,
                                           actReg_,
                                           si,
                                           bitPos,
                                           false,
                                           path_name,
                                           move(wips),
                                           &pinfo.pathResults()});
          {
            ostringstream msg;
            msg << "Made path 0x" << hex
                << ((unsigned long)pinfo.paths().back()) << dec
                << " bitPos: " << bitPos << " name: " << val.first;
            TDEBUG_FUNC_SI_MSG(
              5, "PathManager::createModulesAndWorkers", si, msg.str());
          }
          ++bitPos;
        }
      }
    }
    if (!protoEndPathLabels_.empty()) {
      //  Create the end path and the workers on it.
      vector<WorkerInPath> wips;
      fillWorkers_(ScheduleID::first(),
                   0,
                   protoEndPathLabels_,
                   wips,
                   endPathInfo_.workers());
      endPathInfo_.paths().push_back(new Path{exceptActions_,
                                              actReg_,
                                              ScheduleID::first(),
                                              0,
                                              true,
                                              "end_path",
                                              move(wips),
                                              nullptr});
      {
        ostringstream msg;
        msg << "Made end path 0x" << hex
            << ((unsigned long)endPathInfo_.paths().back()) << dec;
        TDEBUG_FUNC_SI_MSG(
          5, "PathManager::createModulesAndWorkers", 0, msg.str());
      }
    }
    using namespace detail;
    auto const graph_info_collection = getModuleGraphInfoCollection_();
    allModules_.clear();
    ModuleGraphInfoMap const modInfos{graph_info_collection};
    auto const module_graph =
      make_module_graph(modInfos, protoTrigPathLabelMap_, protoEndPathLabels_);
    auto const graph_filename =
      procPS_.get<string>("services.scheduler.dataDependencyGraph", {});
    if (!graph_filename.empty()) {
      cet::ostream_handle osh{graph_filename};
      print_module_graph(osh, modInfos, module_graph.first);
      cerr << "Generated data-dependency graph file: " << graph_filename
           << '\n';
    }
    auto const& err = module_graph.second;
    if (!err.empty()) {
      throw Exception{errors::Configuration} << err << '\n';
    }
  }

  PathsInfo&
  PathManager::triggerPathsInfo(ScheduleID const si)
  {
    return triggerPathsInfo_.at(si);
  }

  PerScheduleContainer<PathsInfo>&
  PathManager::triggerPathsInfo()
  {
    return triggerPathsInfo_;
  }

  PathsInfo&
  PathManager::endPathInfo()
  {
    return endPathInfo_;
  }

  Worker*
  PathManager::triggerResultsInserter(ScheduleID const si) const
  {
    return triggerResultsInserter_.at(si).get();
  }

  void
  PathManager::setTriggerResultsInserter(ScheduleID const si,
                                         unique_ptr<WorkerT<EDProducer>>&& w)
  {
    triggerResultsInserter_.at(si) = move(w);
  }

  void
  PathManager::fillWorkers_(ScheduleID const si,
                            int const pi,
                            vector<WorkerInPath::ConfigInfo> const& wci_list,
                            vector<WorkerInPath>& wips,
                            map<string, Worker*>& workers)
  {
    vector<string> configErrMsgs;
    for (auto const& wci : wci_list) {
      auto const& module_label = wci.label;
      auto const& filterAction = wci.filterAction;
      auto const& mci = allModules_.at(module_label);
      auto const& modPS = mci.modPS_;
      auto const& module_type = mci.libSpec_;
      auto const& module_threading_type = mci.moduleThreadingType_;
      ModuleBase* module = nullptr;
      // All modules are singletons except for replicated modules,
      // enforce that.
      if (module_threading_type != ModuleThreadingType::replicated) {
        auto iter = moduleSet_.find(module_label);
        if (iter != moduleSet_.end()) {
          // We have already constructed this module, reuse it.
          {
            ostringstream msg;
            msg << "Reusing module 0x" << hex << ((unsigned long)iter->second)
                << dec << " path: " << pi << " type: " << module_type
                << " label: " << module_label;
            TDEBUG_FUNC_SI_MSG(5, "PathManager::fillWorkers_", si, msg.str());
          }
          module = iter->second;
        }
      }
      Worker* worker = nullptr;
      // Workers which are present on multiple paths should be shared so
      // that their work is only done once per schedule.
      {
        auto iter = workers.find(module_label);
        if (iter != workers.end()) {
          {
            ostringstream msg;
            msg << "Reusing worker 0x" << hex << ((unsigned long)iter->second)
                << dec << " path: " << pi << " type: " << module_type
                << " label: " << module_label;
            TDEBUG_FUNC_SI_MSG(5, "PathManager::fillWorkers_", si, msg.str());
          }
          worker = iter->second;
        }
      }
      if (worker == nullptr) {
        try {
          ModuleDescription const md{modPS.id(),
                                     module_type,
                                     module_label,
                                     static_cast<int>(module_threading_type),
                                     ProcessConfiguration{processName_,
                                                          procPS_.id(),
                                                          getReleaseVersion()}};
          WorkerParams const wp{procPS_,
                                modPS,
                                outputCallbacks_,
                                productsToProduce_,
                                actReg_,
                                exceptActions_,
                                processName_,
                                module_threading_type,
                                si};
          if (module == nullptr) {
            detail::ModuleMaker_t* module_factory_func = nullptr;
            try {
              lm_.getSymbolByLibspec(
                module_type, "make_module", module_factory_func);
            }
            catch (Exception& e) {
              cet::detail::wrapLibraryManagerException(
                e, "Module", module_type, getReleaseVersion());
            }
            if (module_factory_func == nullptr) {
              throw Exception(errors::Configuration, "BadPluginLibrary: ")
                << "Module " << module_type << " with version "
                << getReleaseVersion()
                << " has internal symbol definition problems: consult an "
                   "expert.";
            }
            string pathName{"ctor"};
            CurrentProcessingContext cpc{
              ScheduleID::first(), &pathName, 0, false};
            cpc.activate(0, &md);
            detail::CPCSentry cpc_sentry{cpc};
            actReg_.sPreModuleConstruction.invoke(md);
            module = module_factory_func(md, wp);
            moduleSet_.emplace(module_label, module);
            {
              ostringstream msg;
              msg << "Made module 0x" << hex << ((unsigned long)module) << dec
                  << " path: " << pi << " type: " << module_type
                  << " label: " << module_label;
              TDEBUG_FUNC_SI_MSG(5, "PathManager::fillWorkers_", si, msg.str());
            }
            actReg_.sPostModuleConstruction.invoke(md);
            module->sortConsumables();
            ConsumesInfo::instance()->collectConsumes(module_label,
                                                      module->getConsumables());
          }
          detail::WorkerFromModuleMaker_t* worker_from_module_factory_func =
            nullptr;
          try {
            lm_.getSymbolByLibspec(module_type,
                                   "make_worker_from_module",
                                   worker_from_module_factory_func);
          }
          catch (Exception& e) {
            cet::detail::wrapLibraryManagerException(
              e, "Module", module_type, getReleaseVersion());
          }
          if (worker_from_module_factory_func == nullptr) {
            throw Exception(errors::Configuration, "BadPluginLibrary: ")
              << "Module " << module_type << " with version "
              << getReleaseVersion()
              << " has internal symbol definition problems: consult an expert.";
          }
          worker = worker_from_module_factory_func(module, md, wp);
          workerSet_.emplace(module_label, worker);
          TDEBUG(5) << "Made worker 0x" << hex << ((unsigned long)worker) << dec
                    << " (" << si << ") path: " << pi
                    << " type: " << module_type << " label: " << module_label
                    << "\n";
        }
        catch (fhicl::detail::validationException const& e) {
          ostringstream es;
          es << "\n\nModule label: " << detail::bold_fontify(module_label)
             << "\nmodule_type : " << detail::bold_fontify(module_type)
             << "\n\n"
             << e.what();
          configErrMsgs.push_back(es.str());
        }
      }
      workers.emplace(module_label, worker);
      wips.emplace_back(worker, filterAction);
    }
    if (!configErrMsgs.empty()) {
      constexpr cet::HorizontalRule rule{100};
      ostringstream msg;
      msg << "\n"
          << rule('=') << "\n\n"
          << "!! The following modules have been misconfigured: !!"
          << "\n";
      for (auto const& err : configErrMsgs) {
        msg << "\n" << rule('-') << "\n" << err;
      }
      msg << "\n" << rule('=') << "\n\n";
      throw Exception(errors::Configuration) << msg.str();
    }
  }

  ModuleType
  PathManager::loadModuleType_(string const& lib_spec)
  {
    detail::ModuleTypeFunc_t* mod_type_func = nullptr;
    try {
      lm_.getSymbolByLibspec(lib_spec, "moduleType", mod_type_func);
    }
    catch (Exception& e) {
      cet::detail::wrapLibraryManagerException(
        e, "Module", lib_spec, getReleaseVersion());
    }
    if (mod_type_func == nullptr) {
      throw Exception(errors::Configuration, "BadPluginLibrary")
        << "Module " << lib_spec << " with version " << getReleaseVersion()
        << " has internal symbol definition problems: consult an expert.";
    }
    return mod_type_func();
  }

  ModuleThreadingType
  PathManager::loadModuleThreadingType_(string const& lib_spec)
  {
    detail::ModuleThreadingTypeFunc_t* mod_threading_type_func = nullptr;
    try {
      lm_.getSymbolByLibspec(
        lib_spec, "moduleThreadingType", mod_threading_type_func);
    }
    catch (Exception& e) {
      cet::detail::wrapLibraryManagerException(
        e, "Module", lib_spec, getReleaseVersion());
    }
    if (mod_threading_type_func == nullptr) {
      throw Exception(errors::Configuration, "BadPluginLibrary")
        << "Module " << lib_spec << " with version " << getReleaseVersion()
        << " has internal symbol definition problems: consult an expert.";
    }
    return mod_threading_type_func();
  }

  detail::collection_map_t
  PathManager::getModuleGraphInfoCollection_()
  {
    using namespace detail;
    collection_map_t result{};
    auto& source_info = result["*source*"];
    if (!protoTrigPathLabelMap_.empty()) {
      set<string> const path_names{cbegin(triggerPathNames_),
                                   cend(triggerPathNames_)};
      source_info.paths = path_names;
      result["TriggerResults"] = ModuleGraphInfo{ModuleType::producer};
    } else if (!protoEndPathLabels_.empty()) {
      source_info.paths = {"end_path"};
    }
    auto fill_module_info = [this](string const& path_name,
                                   configs_t const& worker_configs,
                                   collection_map_t& info_collection) {
      for (auto const& worker_config : worker_configs) {
        auto const& module_name = worker_config.label;
        auto const& mci = allModules_.at(module_name);
        auto& graph_info = info_collection[module_name];
        graph_info.paths.insert(path_name);
        graph_info.module_type = mci.moduleType_;
        auto const& consumables =
          ConsumesInfo::instance()->consumables(module_name);
        for (auto const& per_branch_type : consumables) {
          for (auto const& prod_info : per_branch_type) {
            if ((prod_info.process_ != processName_) &&
                (prod_info.process_ != "*current_process*")) {
              graph_info.product_dependencies.insert("*source*");
            } else {
              graph_info.product_dependencies.insert(prod_info.label_);
            }
          }
        }
      }
    };
    static string const allowed_path_spec{R"([\*a-zA-Z_][\*\?\w]*)"};
    static regex const regex{"(\\w+:)?(!|exception@)?(" + allowed_path_spec +
                             ")(&noexception)?"};
    auto fill_select_events_deps = [this](configs_t const& worker_configs,
                                          collection_map_t& info_collection) {
      for (auto const& worker_config : worker_configs) {
        auto const& module_name = worker_config.label;
        auto const& ps = allModules_.at(module_name).modPS_;
        auto& graph_info = info_collection[module_name];
        assert(is_observer(graph_info.module_type));
        auto const path_specs = ps.get<vector<string>>("SelectEvents", {});
        for (auto const& path_spec : path_specs) {
          smatch matches;
          regex_match(path_spec, matches, regex);
          // By the time we have gotten here, all modules have been
          // constructed, and it is guaranteed that the specified paths
          // are in accord with the above regex.
          //   0: Full match
          //   1: Optional process name
          //   2: Optional '!' or 'exception@'
          //   3: Required path specification
          //   4: Optional '&noexception'
          assert(matches.size() == 5);
          graph_info.select_events.insert(matches[3]);
        }
      }
    };
    for (auto const& path : protoTrigPathLabelMap_) {
      fill_module_info(path.first, path.second, result);
    }
    fill_module_info("end_path", protoEndPathLabels_, result);
    fill_select_events_deps(protoEndPathLabels_, result);
    return result;
  }

} // namespace art
