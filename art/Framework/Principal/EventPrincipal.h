#ifndef art_Framework_Principal_EventPrincipal_h
#define art_Framework_Principal_EventPrincipal_h

/*----------------------------------------------------------------------

EventPrincipal: This is the class responsible for management of
per event EDProducts. It is not seen by reconstruction code;
such code sees the Event class, which is a proxy for EventPrincipal.

The major internal component of the EventPrincipal
is the DataBlock.

----------------------------------------------------------------------*/

#include "art/Framework/Principal/NoDelayedReader.h"
#include "art/Framework/Principal/Principal.h"
#include "art/Framework/Principal/fwd.h"
#include "art/Persistency/Common/GroupQueryResult.h"
#include "art/Persistency/Provenance/BranchMapper.h"
#include "art/Persistency/Provenance/BranchType.h"
#include "art/Persistency/Provenance/EventAuxiliary.h"
#include "art/Persistency/Provenance/History.h"
#include "cetlib/exempt_ptr.h"
#include "cpp0x/memory"

#include <vector>

namespace art {
  class EventID;
  bool
  isSameEvent(EventPrincipal const& a, EventPrincipal const& b);
}

class art::EventPrincipal : public Principal {
public:
  typedef EventAuxiliary Auxiliary;
  typedef Principal::SharedConstGroupPtr SharedConstGroupPtr;

  EventPrincipal(EventAuxiliary const& aux,
                 ProcessConfiguration const& pc,
                 std::shared_ptr<History> history = std::shared_ptr<History>(new History),
                 std::auto_ptr<BranchMapper> mapper = std::auto_ptr<BranchMapper>(new BranchMapper),
                 std::auto_ptr<DelayedReader> rtrv = std::auto_ptr<DelayedReader>(new NoDelayedReader));

  // use compiler-generated copy c'tor, copy assignment, and d'tor

  SubRunPrincipal const& subRunPrincipal() const;

  SubRunPrincipal & subRunPrincipal();

  std::shared_ptr<SubRunPrincipal> subRunPrincipalSharedPtr() { return subRunPrincipal_; }

  void setSubRunPrincipal(std::shared_ptr<SubRunPrincipal> srp) { subRunPrincipal_ = srp; }

  EventID const& id() const { return aux().id(); }

  Timestamp const& time() const { return aux().time(); }

  bool isReal() const { return aux().isRealData(); }

  EventAuxiliary::ExperimentType ExperimentType() const { return aux().experimentType(); }

  EventAuxiliary const& aux() const { return aux_; }

  SubRunNumber_t subRun() const { return aux().subRun(); }

  RunNumber_t run() const { return id().run(); }

  RunPrincipal const& runPrincipal() const;

  RunPrincipal & runPrincipal();

  void addOnDemandGroup(BranchDescription const& desc,
                        cet::exempt_ptr<Worker> worker);

  EventSelectionIDVector const& eventSelectionIDs() const;

  History const& history() const {return *history_;}

  History& history() {return *history_;}

  using Principal::getGroup;
  GroupQueryResult getGroup(ProductID const &pid) const;

  GroupQueryResult getByProductID(ProductID const& pid) const;

  void put(std::auto_ptr<EDProduct> edp, BranchDescription const& bd,
           std::auto_ptr<ProductProvenance const> productProvenance);

  void addGroup(BranchDescription const& bd);

  void addGroup(std::auto_ptr<EDProduct> prod,
                BranchDescription const& bd);

  ////    virtual EDProduct const* getIt(ProductID const& pid) const;

  ProductID branchIDToProductID(BranchID const& bid) const;

  BranchType branchType() const { return InEvent; }

private:

  BranchID productIDToBranchID(ProductID const& pid) const;

  virtual void addOrReplaceGroup(std::auto_ptr<Group> g);

  virtual ProcessHistoryID const& processHistoryID() const {return history().processHistoryID();}

  virtual void setProcessHistoryID(ProcessHistoryID const& phid) const {return history().setProcessHistoryID(phid);}

  EventAuxiliary aux_;
  std::shared_ptr<SubRunPrincipal> subRunPrincipal_;

  std::shared_ptr<History> history_;

  std::map<BranchListIndex, ProcessIndex> branchToProductIDHelper_;

};

inline
bool
art::isSameEvent(EventPrincipal const& a, EventPrincipal const& b) {
  return a.aux() == b.aux();
}

#endif /* art_Framework_Principal_EventPrincipal_h */

// Local Variables:
// mode: c++
// End:
