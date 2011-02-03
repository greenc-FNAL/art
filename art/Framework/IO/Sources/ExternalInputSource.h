#ifndef art_Framework_IO_Sources_ExternalInputSource_h
#define art_Framework_IO_Sources_ExternalInputSource_h

// ======================================================================
//
// ExternalInputSource
//
// ======================================================================

#include "art/Framework/Core/ConfigurableInputSource.h"
#include "art/Framework/IO/Catalog/InputFileCatalog.h"
#include "fhiclcpp/ParameterSet.h"
#include <string>
#include <vector>


namespace art {

  class ExternalInputSource
    : public ConfigurableInputSource
  {
  public:
    ExternalInputSource(fhicl::ParameterSet const& pset,
                        InputSourceDescription const& desc,
                        bool realData = true);
    virtual ~ExternalInputSource();

  std::vector<std::string> const&
    logicalFileNames() const {return catalog_.logicalFileNames();}
  std::vector<std::string> const&
    fileNames() const {return catalog_.fileNames();}
  InputFileCatalog&
    catalog() {return catalog_;}

  private:
    PoolCatalog poolCatalog_;
    InputFileCatalog catalog_;
  };

}  // art

// ======================================================================

#endif /* art_Framework_IO_Sources_ExternalInputSource_h */

// Local Variables:
// mode: c++
// End:
