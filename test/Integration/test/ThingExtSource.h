#ifndef Integration_ThingExtSource_h
#define Integration_ThingExtSource_h

/** \class ThingExtSource
 *
 * \version   1st Version Dec. 27, 2005

 *
 ************************************************************/

#include "art/Framework/Core/Frameworkfwd.h"
#include "art/Framework/IO/Sources/ExternalInputSource.h"
#include "FWCore/Integration/test/ThingAlgorithm.h"

namespace edmtest {
  class ThingExtSource : public edm::ExternalInputSource {
  public:

    // The following is not yet used, but will be the primary
    // constructor when the parameter set system is available.
    //
    explicit ThingExtSource(edm::ParameterSet const& pset, edm::InputSourceDescription const& desc);

    virtual ~ThingExtSource();

    virtual bool produce(edm::Event& e);

    virtual void beginRun(edm::Run& r);

    virtual void endRun(edm::Run& r);

    virtual void beginSubRun(edm::SubRun& lb);

    virtual void endSubRun(edm::SubRun& lb);

  private:
    ThingAlgorithm alg_;
  };
}
#endif
