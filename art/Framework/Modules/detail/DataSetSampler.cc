#include "art/Framework/Modules/detail/DataSetSampler.h"
#include "art/Utilities/bold_fontify.h"
#include "canvas/Utilities/Exception.h"
#include "cetlib/container_algorithms.h"
#include "fhiclcpp/ParameterSet.h"
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/Table.h"

#include <iterator>

using namespace fhicl;

namespace {
  struct DataSetConfig {
    Atom<std::string> fileName{Name{"fileName"}};
    Atom<double> weight{Name{"weight"}};
  };
}

art::detail::DataSetSampler::DataSetSampler(ParameterSet const& pset) noexcept(
  false)
{
  auto const dataset_names = pset.get_pset_names();
  if (dataset_names.empty()) {
    throw Exception{
      errors::Configuration,
      "An error occurred while processing dataset configurations.\n"}
      << "No datasets were configured for the SamplingInput source.\n"
         "At least one must be specified.\n";
  }
  auto const ndatasets = dataset_names.size();
  datasetNames_.reserve(ndatasets);
  weights_.reserve(ndatasets);
  fileNames_.reserve(ndatasets);
  for (auto const& dataset : dataset_names) {
    auto const& dataset_pset = pset.get<ParameterSet>(dataset);
    try {
      Table<DataSetConfig> table{dataset_pset};
      datasetNames_.push_back(dataset);
      weights_.push_back(table().weight());
      fileNames_.push_back(table().fileName());
    }
    catch (fhicl::detail::validationException const& e) {
      throw Exception{errors::Configuration}
        << "\nModule label: " << art::detail::bold_fontify("source")
        << "\nmodule_type : " << art::detail::bold_fontify("SamplingInput")
        << "\ndataset     : " << art::detail::bold_fontify(dataset) << "\n\n"
        << e.what();
    }
  }
  dist_ = decltype(dist_){cbegin(weights_), cend(weights_)};
}

std::size_t
art::detail::DataSetSampler::index_for(std::string const& dataset) const
{
  auto const it = cet::find_in_all(datasetNames_, dataset);
  if (it == cend(datasetNames_)) {
    throw Exception{errors::LogicError}
      << "An index has been requested for dataset '" << dataset
      << "', which has\n"
      << "not been configured.  Please contact artists@fnal.gov for "
         "guidance.\n";
  }
  return static_cast<std::size_t>(std::distance(cbegin(datasetNames_), it));
}