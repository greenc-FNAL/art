#ifndef FWCore_Services_InitRootHandlers_h
#define FWCore_Services_InitRootHandlers_h


#include "art/Utilities/RootHandlers.h"
#include "fhiclcpp/ParameterSet.h"


namespace edm {
  class ActivityRegistry;
}

namespace edm {
  namespace service {

    class InitRootHandlers : public RootHandlers
    {
    public:
      InitRootHandlers( fhicl::ParameterSet const & pset
                      , edm::ActivityRegistry & activity );
      virtual ~InitRootHandlers ();

    private:
      virtual void disableErrorHandler_();
      virtual void enableErrorHandler_();
      bool unloadSigHandler_;
      bool resetErrHandler_;
      bool autoLibraryLoader_;
    };

  }  // namespace service
}  // namespace edm

#endif  // InitRootHandlers_H
