#ifndef art_Framework_Services_Registry_ServiceToken_h
#define art_Framework_Services_Registry_ServiceToken_h

// ======================================================================
//
// ServiceToken - Token used to denote a 'service set'
//
// When you request a new 'service set' to be created from the
// ServiceRegistry, the ServiceRegistry will return a ServiceToken.  When
// you want this 'service set' to be used, create a
// ServiceRegistry::Operate by passing the ServiceToken via the constructor.
//
// ======================================================================

#include "art/Framework/Services/Registry/ServicesManager.h"
#include "art/Framework/Services/Registry/detail/ServiceHelper.h"

#include <memory>

namespace art {
  class ServiceRegistry;
  class ActivityRegistry;

  class ServiceToken;
}

// ----------------------------------------------------------------------

class art::ServiceToken
{
  friend class ServiceRegistry;
  friend class ServicesManager;

public:

  static auto createInvalid() { return ServiceToken{nullptr}; }

  template <typename T, typename = std::enable_if_t<detail::ServiceHelper<T>::scope_val != ServiceScope::PER_SCHEDULE>>
  void add(std::unique_ptr<T>&& serv)
  {
    manager_->put(std::move(serv));
  }

  template <typename T, typename = std::enable_if_t<detail::ServiceHelper<T>::scope_val == ServiceScope::PER_SCHEDULE>>
  void add(std::vector<std::unique_ptr<T>>&& services)
  {
    manager_->put(std::move(services));
  }

  void forceCreation()
  {
    manager_->forceCreation();
  }

  void getParameterSets(ServicesManager::ParameterSets& out) const
  {
    manager_->getParameterSets(out);
  }

private:

  explicit ServiceToken(std::shared_ptr<ServicesManager> iManager) :
    manager_{iManager}
  {}

  std::shared_ptr<ServicesManager> manager_;

};  // ServiceToken

// ======================================================================

#endif /* art_Framework_Services_Registry_ServiceToken_h */

// Local Variables:
// mode: c++
// End:
