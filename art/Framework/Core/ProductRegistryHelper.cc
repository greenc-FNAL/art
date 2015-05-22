#include "art/Framework/Core/ProductRegistryHelper.h"
// vim: set sw=2:

#include "art/Persistency/Provenance/BranchDescription.h"
#include "art/Persistency/Provenance/BranchIDList.h"
#include "art/Persistency/Provenance/BranchIDListHelper.h"
#include "art/Persistency/Provenance/MasterProductRegistry.h"
#include "art/Persistency/Provenance/ModuleDescription.h"
#include "art/Persistency/Provenance/TypeLabel.h"
#include "art/Utilities/DebugMacros.h"
#include "art/Utilities/Exception.h"
#include "cetlib/container_algorithms.h"

#include <string>

void
art::ProductRegistryHelper::
registerProducts(MasterProductRegistry& mpr,
                 ModuleDescription const& md)
{
  auto pl = std::move(productList_);
  if (pl) {
    FileBlock fb({}, "ProductRegistryHelper");
    mpr.initFromFirstPrimaryFile(*productList_.get(), fb);
    BranchIDList bil;
    for (auto const& val : productList_) {
        bil.push_back(val.second.branchID().id());
    }
    BranchIDListHelper::updateFromInput({bil}, fb.fileName());
  }
  for (auto const& val : typeLabelList_) {
    mpr.addProduct(make_unique<art::BranchDescription>(val, md));
  }
}

