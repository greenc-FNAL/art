#ifndef Framework_GeneratedInputSource_h
#define Framework_GeneratedInputSource_h

/*----------------------------------------------------------------------

----------------------------------------------------------------------*/


#include "art/Framework/Core/ConfigurableInputSource.h"

#include "fhiclcpp/ParameterSet.h"

#include <memory>


namespace edm {

  class GeneratedInputSource : public ConfigurableInputSource {
  public:
    explicit GeneratedInputSource( fhicl::ParameterSet const& pset
                                 , InputSourceDescription const& desc );
    virtual ~GeneratedInputSource();

  };

}  // namespace edm

#endif  // Framework_GeneratedInputSource_h
