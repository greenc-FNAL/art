#include "art/Framework/Core/detail/ModuleGraph.h"
#include "art/Framework/Core/detail/ModuleGraphInfoMap.h"
#include "art/Framework/Core/detail/graph_algorithms.h"
#include "art/test/Framework/Core/data-dependencies/Configs.h"
#include "boost/graph/graph_utility.hpp"
#include "canvas/Utilities/Exception.h"
#include "fhiclcpp/ParameterSet.h"
#include "fhiclcpp/make_ParameterSet.h"
#include "fhiclcpp/types/OptionalAtom.h"
#include "fhiclcpp/types/OptionalDelegatedParameter.h"
#include "fhiclcpp/types/OptionalSequence.h"
#include "fhiclcpp/types/OptionalTable.h"
#include "fhiclcpp/types/Table.h"
#include "fhiclcpp/types/TupleAs.h"

#include <cassert>
#include <fstream>
#include <iostream>

using namespace art::detail;
using namespace fhicl;
using namespace std::string_literals;
using std::string;

namespace {
  paths_to_modules_t
  get_paths_to_modules(ParameterSet const& physics)
  {
    paths_to_modules_t result;
    for (auto const& name : physics.get_names()) {
      if (!physics.is_key_to_sequence(name))
        continue;
      auto const tmp = physics.get<std::vector<string>>(name);
      configs_t configs;
      cet::transform_all(tmp, back_inserter(configs), [](auto const& str) {
        return art::WorkerInPath::ConfigInfo{str, art::WorkerInPath::Normal};
      });
      result[name] = configs;
    }
    return result;
  }

  names_t const tables_with_modifiers{"physics.producers", "physics.filters"};
  names_t const tables_with_observers{"physics.analyzers", "outputs"};

  inline bool
  module_found_in_table(string const& module_name,
                        ParameterSet const& pset,
                        string const& table_name)
  {
    if (!pset.has_key(table_name)) {
      return false;
    }
    auto const& table = pset.get<ParameterSet>(table_name);
    return table.has_key(module_name);
  }

  inline art::ModuleType
  module_found_with_type(string const& module_name, ParameterSet const& pset)
  {
    if (module_found_in_table(module_name, pset, "physics.producers"))
      return art::ModuleType::producer;
    if (module_found_in_table(module_name, pset, "physics.filters"))
      return art::ModuleType::filter;
    if (module_found_in_table(module_name, pset, "physics.analyzers"))
      return art::ModuleType::analyzer;
    if (module_found_in_table(module_name, pset, "outputs"))
      return art::ModuleType::output_module;
    return art::ModuleType::non_art;
  }

  inline string
  table_for_module_type(art::ModuleType const module_type)
  {
    if (module_type == art::ModuleType::producer)
      return "physics.producers";
    if (module_type == art::ModuleType::filter)
      return "physics.filters";
    if (module_type == art::ModuleType::analyzer)
      return "physics.analyzers";
    if (module_type == art::ModuleType::output_module)
      return "outputs";
    return {};
  }

  bool
  module_found_in_tables(string const& module_name,
                         ParameterSet const& pset,
                         names_t const& table_names)
  {
    for (auto const& table_name : table_names) {
      if (module_found_in_table(module_name, pset, table_name)) {
        return true;
      }
    }
    return false;
  }

  paths_to_modules_t
  select_paths(ParameterSet const& pset,
               names_t const& tables,
               paths_to_modules_t& paths_to_modules)
  {
    paths_to_modules_t result;
    std::vector<string> paths_to_erase;
    for (auto const& pr : paths_to_modules) {
      auto const& path_name = pr.first;
      auto const& modules = pr.second;

      bool first_module{true};
      bool present{true};
      for (auto const& module : modules) {
        if (first_module) {
          first_module = false;
          present = module_found_in_tables(module.label, pset, tables);
        } else if (present !=
                   module_found_in_tables(module.label, pset, tables)) {
          // The presence of the first module determines what the
          // remaining modules should be.
          throw art::Exception{art::errors::LogicError}
            << "There is an inconsistency in path " << path_name << ".\n"
            << "Module " << module.label
            << " is a modifier/observer whereas the other modules\n"
            << "on the path are the opposite.";
        }
      }
      if (present) {
        result.insert(cend(result), pr);
        paths_to_erase.push_back(path_name);
      }
    }
    for (auto const& path : paths_to_erase) {
      paths_to_modules.erase(path);
    }
    return result;
  }

  configs_t
  merge_end_paths(paths_to_modules_t const& paths_to_modules)
  {
    configs_t result;
    for (auto const& pr : paths_to_modules) {
      result.insert(cend(result), cbegin(pr.second), cend(pr.second));
    }
    return result;
  }

  name_set_t
  path_names(paths_to_modules_t const& paths_to_modules)
  {
    name_set_t result;
    for (auto const& pr : paths_to_modules) {
      result.insert(pr.first);
    }
    return result;
  }

  std::set<art::ProductInfo>
  produced_products(
    std::vector<art::test::TypeAndInstance> const& productsToProduce,
    string const& module_name,
    string const& current_process_name)
  {
    std::set<art::ProductInfo> result;
    for (auto const& prod : productsToProduce) {
      result.emplace(
        art::ProductInfo::ConsumableType::Product,
        prod.friendlyClassName,
        module_name,
        prod.productInstanceName,
        art::ProcessTag{current_process_name, current_process_name});
    }
    return result;
  }

  template <typename T>
  std::set<art::ProductInfo>
  sorted_consumed_products(Table<T> const& module,
                           string const& module_name,
                           string const& current_process_name,
                           collection_map_t const& modules)
  {
    std::set<art::ProductInfo> sorted_deps;
    std::vector<art::test::TypeAndTag> deps;
    if (module().consumes(deps)) {
      for (auto const& dep : deps) {
        art::ProcessTag const processTag{dep.inputTag.process(),
                                         current_process_name};
        // In cases where a user has not specified the current process
        // name (or the literal "current_process"), we set the label
        // of the module this worker depends upon to "input_source",
        // solely for data-dependency checking.  This permits users to
        // specify only a module label in the input tag, and even
        // though this might collide with a module label in the
        // current process, it is not necessarily an error.
        //
        // In the future, we may wish to constrain the behavior so
        // that if there is an ambiguity in module labels between
        // processes, a user will be required to specify
        // "current_process" or "input_source".
        std::string const label = (processTag.name() != current_process_name) ?
                                    "input_source" :
                                    dep.inputTag.label();
        if (label != "input_source") {
          auto const& mod_info = modules.at(dep.inputTag.label());
          // Current process
          auto product_match = [&dep](auto const& pi) {
            return dep.friendlyClassName == pi.friendlyClassName &&
                   dep.inputTag.instance() == pi.instance;
          };

          if (!std::any_of(cbegin(mod_info.produced_products),
                           cend(mod_info.produced_products),
                           product_match)) {
            throw art::Exception{art::errors::Configuration}
              << "Module " << module_name
              << " expects to consume a product from module "
              << dep.inputTag.label() << " with the signature:\n"
              << "  Friendly class name: " << dep.friendlyClassName << '\n'
              << "  Instance name: " << dep.inputTag.instance() << '\n'
              << "  Process name: " << dep.inputTag.process() << '\n'
              << "However, no product of that signature is provided by "
                 "module "
              << dep.inputTag.label() << ".\n";
          }
        }
        sorted_deps.emplace(art::ProductInfo::ConsumableType::Product,
                            dep.friendlyClassName,
                            label,
                            dep.inputTag.instance(),
                            processTag);
      }
    }
    return sorted_deps;
  }

  template <typename T>
  std::set<art::ProductInfo>
  consumes_many(Table<T> const& module,
                configs_t::const_iterator mit,
                configs_t::const_iterator const end,
                collection_map_t const& modules)
  {
    std::set<art::ProductInfo> result;
    std::vector<std::string> many;
    if (module().consumesMany(many)) {
      for (auto const& class_name : many) {
        // Loop through modules on this path, introducing
        // product-lookup dependencies if the type of the product
        // created by the module matches the type requested in the
        // consumesMany call.
        for (; mit != end; ++mit) {
          auto const& preceding_module_name = mit->label;
          // Bail out early if no products produced for this module.
          auto found = modules.find(preceding_module_name);
          if (found == cend(modules)) {
            continue;
          }
          cet::copy_if_all(found->second.produced_products,
                           inserter(result, begin(result)),
                           [&class_name](auto const& pi) {
                             return class_name == pi.friendlyClassName;
                           });
        }
      }
    }
    return result;
  }

  art::ProductDescriptions
  fillProducesInfo(ParameterSet const& pset,
                   string const& process_name,
                   string const& path_name,
                   configs_t const& module_configs,
                   collection_map_t& modules)
  {
    art::ProductDescriptions producedProducts;
    auto const begin = cbegin(module_configs);
    for (auto it = begin, end = cend(module_configs); it != end; ++it) {
      auto const& config = *it;
      auto const& module_name = config.label;
      auto& info = modules[module_name];
      info.paths.insert(path_name);
      info.module_type = module_found_with_type(module_name, pset);
      auto const table_name = table_for_module_type(info.module_type);
      auto const& table =
        pset.get<ParameterSet>(table_name + "." + module_name);
      if (!is_modifier(info.module_type))
        continue;

      Table<art::test::ModifierModuleConfig> const mod{table};
      std::vector<art::test::TypeAndInstance> prods;
      if (mod().produces(prods)) {
        info.produced_products =
          produced_products(prods, module_name, process_name);
      }
    }
    return producedProducts;
  }

  void
  fillModifierInfo(ParameterSet const& pset,
                   string const& process_name,
                   string const& path_name,
                   configs_t const& module_configs,
                   collection_map_t& modules)
  {
    auto const begin = cbegin(module_configs);
    for (auto it = begin, end = cend(module_configs); it != end; ++it) {
      auto const& config = *it;
      auto const& module_name = config.label;
      auto& info = modules[module_name];
      info.paths.insert(path_name);
      info.module_type = module_found_with_type(module_name, pset);
      auto const table_name = table_for_module_type(info.module_type);
      auto const& table =
        pset.get<ParameterSet>(table_name + "." + module_name);
      if (!is_modifier(info.module_type))
        continue;

      Table<art::test::ModifierModuleConfig> const mod{table};
      info.consumed_products =
        sorted_consumed_products(mod, module_name, process_name, modules);
      auto const& many = consumes_many(mod, begin, it, modules);
      info.consumed_products.insert(cbegin(many), cend(many));
    }
  }

  void
  fillObserverInfo(ParameterSet const& pset,
                   string const& process_name,
                   string const& path_name,
                   configs_t const& module_configs,
                   collection_map_t& modules)
  {
    auto const begin = cbegin(module_configs);
    for (auto it = begin, end = cend(module_configs); it != end; ++it) {
      auto const& config = *it;
      auto const& module_name = config.label;
      auto& info = modules[module_name];
      info.paths.insert(path_name);
      info.module_type = module_found_with_type(module_name, pset);
      auto const table_name = table_for_module_type(info.module_type);
      auto const& table =
        pset.get<ParameterSet>(table_name + "." + module_name);
      if (!is_observer(info.module_type))
        continue;

      Table<art::test::ObserverModuleConfig> const mod{table};
      info.consumed_products =
        sorted_consumed_products(mod, module_name, process_name, modules);
      auto const& many = consumes_many(mod, begin, it, modules);
      info.consumed_products.insert(cbegin(many), cend(many));
      std::vector<string> sel;
      if (mod().select_events(sel)) {
        info.select_events = std::set<string>(cbegin(sel), cend(sel));
      }
    }
  }
}

int
main(int argc, char** argv) try {
  if (argc != 2)
    return 1;

  string const filename{argv[1]};

  ParameterSet pset;
  cet::filepath_maker maker{};
  make_ParameterSet(filename, maker, pset);
  Table<art::test::TopLevelTable> table{pset};
  auto const& process_name = table().process_name();
  auto const& test_properties = table().test_properties();

  ParameterSet physics;
  if (!table().physics.get_if_present(physics)) {
    return 0;
  }

  // Form the paths
  auto paths_to_modules = get_paths_to_modules(physics);
  auto const trigger_paths =
    select_paths(pset, tables_with_modifiers, paths_to_modules);
  auto end_paths = paths_to_modules;

  auto const end_path = merge_end_paths(end_paths);

  // Get modules
  art::detail::collection_map_t modules{};

  auto& source_info = modules["input_source"];
  if (!trigger_paths.empty()) {
    source_info.paths = path_names(trigger_paths);
  } else if (!end_path.empty()) {
    source_info.paths = {"end_path"};
  }

  // Assemble all the information for products to be produced.
  for (auto const& path : trigger_paths) {
    fillProducesInfo(pset, process_name, path.first, path.second, modules);
  }

  // Now go through an assemble the rest of the graph info objects,
  // based on the consumes clauses.  The reason this is separate from
  // the filling of the produces information is that we want to allow
  // users to specify consumes dependencies at this stage, checking
  // for correct types, etc. *before* checking if the workflow is
  // well-formed (i.e. no interpath dependencies, or intrapath
  // circularities).  This pattern mimics what is done in
  // art::PathManager, where all produces information is filled first,
  // and then the graph is assembled afterward.
  std::string err_msg;
  bool graph_failure{false};
  try {
    for (auto const& path : trigger_paths) {
      fillModifierInfo(pset, process_name, path.first, path.second, modules);
    }

    if (!trigger_paths.empty()) {
      modules["TriggerResults"] = ModuleGraphInfo{art::ModuleType::producer};
    }

    fillObserverInfo(pset, process_name, "end_path", end_path, modules);
  }
  catch (cet::exception const& e) {
    err_msg += e.what();
    graph_failure = true;
  }

  // Build the graph only if there was no error in constructing the
  // information it needs.
  if (err_msg.empty()) {
    ModuleGraphInfoMap const modInfos{modules};
    auto const module_graph =
      art::detail::make_module_graph(modInfos, trigger_paths, end_path);
    auto const& err = module_graph.second;
    if (!err.empty()) {
      err_msg += err;
      graph_failure = true;
    }

    auto const pos = filename.find(".fcl");
    string const basename =
      (pos != string::npos) ? filename.substr(0, pos) : filename;
    std::ofstream ofs{basename + ".dot"};
    print_module_graph(ofs, modInfos, module_graph.first);
  }

  // Check if test properties have been satisfied
  int rc{};
  bool const graph_failure_expected = test_properties.graph_failure_expected();
  if (graph_failure && !graph_failure_expected) {
    std::cerr << "Unexpected graph-construction failure.\n"
              << "Error message:\n"
              << err_msg << '\n';
    rc = 1;
  } else if (!graph_failure && graph_failure_expected) {
    std::cerr << "Unexpected graph-construction success.\n";
    rc = 1;
  }
  string expected_msg;
  if (test_properties.error_message(expected_msg)) {
    std::regex const re{expected_msg};
    if (!std::regex_search(err_msg, re)) {
      std::cerr << " The error message does not match what was expected:\n"
                << "   Actual: [" << err_msg << "]\n"
                << "   Expected: [" << expected_msg << "]\n";
      rc = 3;
    }
  }
  return rc;
}
catch (detail::validationException const& v) {
  std::cerr << v.what();
  return 1;
}
catch (std::exception const& e) {
  std::cerr << e.what() << '\n';
  return 1;
}
catch (...) {
  std::cerr << "Job failed.\n";
  return 1;
}