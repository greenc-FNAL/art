#include "art/Framework/Core/EDProducer.h"
// vim: set sw=2 expandtab :

#include "art/Framework/Core/SharedResourcesRegistry.h"
#include "art/Utilities/CPCSentry.h"
#include "canvas/Utilities/Exception.h"
#include "hep_concurrency/SerialTaskQueueChain.h"

#include <string>
#include <utility>
#include <vector>

using namespace hep::concurrency;
using namespace std;

namespace art {

  string
  EDProducer::workerType() const
  {
    return "WorkerT<EDProducer>";
  }

  string
  SharedProducer::workerType() const
  {
    return "WorkerT<SharedProducer>";
  }

  string
  ReplicatedProducer::workerType() const
  {
    return "WorkerT<ReplicatedProducer>";
  }

  void
  EDProducer::doBeginJob()
  {
    serialize(SharedResourcesRegistry::kLegacy);
    vector<string> const names(cbegin(resourceNames_), cend(resourceNames_));
    auto queues = SharedResourcesRegistry::instance()->createQueues(names);
    chain_ = new SerialTaskQueueChain{queues};
    failureToPutProducts(md_);
    beginJob();
  }

  void
  SharedProducer::doBeginJob()
  {
    if (!resourceNames_.empty()) {
      if (asyncDeclared_) {
        throw art::Exception{
          art::errors::LogicError,
          "An error occurred while processing scheduling options for a module."}
          << "async<InEvent>() cannot be called in combination with any "
             "serialize<InEvent>(...) calls.\n";
      }
      vector<string> const names(cbegin(resourceNames_), cend(resourceNames_));
      auto queues = SharedResourcesRegistry::instance()->createQueues(names);
      chain_ = new SerialTaskQueueChain{queues};
    }
    failureToPutProducts(md_);
    beginJob();
  }

  void
  ReplicatedProducer::doBeginJob()
  {
    failureToPutProducts(md_);
    beginJob();
  }

} // namespace art
