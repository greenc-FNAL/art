#ifndef art_Framework_Core_RunPrincipal_h
#define art_Framework_Core_RunPrincipal_h

/*----------------------------------------------------------------------

RunPrincipal: This is the class responsible for management of
per run EDProducts. It is not seen by reconstruction code;
such code sees the Run class, which is a proxy for RunPrincipal.

The major internal component of the RunPrincipal
is the DataBlock.

----------------------------------------------------------------------*/

#include "art/Framework/Core/Principal.h"
#include "art/Persistency/Provenance/BranchMapper.h"
#include "art/Persistency/Provenance/RunAuxiliary.h"
#include "boost/shared_ptr.hpp"
#include <vector>

namespace art {
  class UnscheduledHandler;
  class RunPrincipal : public Principal {
  public:
    typedef RunAuxiliary Auxiliary;
    typedef std::vector<ProductProvenance> EntryInfoVector;
    typedef Principal Base;

    RunPrincipal(RunAuxiliary const& aux,
	boost::shared_ptr<ProductRegistry const> reg,
	ProcessConfiguration const& pc,
	boost::shared_ptr<BranchMapper> mapper = boost::shared_ptr<BranchMapper>(new BranchMapper),
	boost::shared_ptr<DelayedReader> rtrv = boost::shared_ptr<DelayedReader>(new NoDelayedReader));
    ~RunPrincipal() {}

    RunAuxiliary const& aux() const {
      return aux_;
    }

    RunNumber_t run() const {
      return aux().run();
    }

    RunID const& id() const {
      return aux().id();
    }

    Timestamp const& beginTime() const {
      return aux().beginTime();
    }

    Timestamp const& endTime() const {
      return aux().endTime();
    }

    void setEndTime(Timestamp const& time) {
      aux_.setEndTime(time);
    }

    void setUnscheduledHandler(boost::shared_ptr<UnscheduledHandler>) {}

    void mergeRun(boost::shared_ptr<RunPrincipal> rp);

    void put(std::auto_ptr<EDProduct> edp,
	     ConstBranchDescription const& bd, std::auto_ptr<ProductProvenance> productProvenance);

    void addGroup(ConstBranchDescription const& bd);

    void addGroup(std::auto_ptr<EDProduct> prod, ConstBranchDescription const& bd, std::auto_ptr<ProductProvenance> productProvenance);

    void addGroup(ConstBranchDescription const& bd, std::auto_ptr<ProductProvenance> productProvenance);

  private:

    virtual void addOrReplaceGroup(std::auto_ptr<Group> g);

    virtual ProcessHistoryID const& processHistoryID() const {return aux().processHistoryID_;}

    virtual void setProcessHistoryID(ProcessHistoryID const& phid) const {return aux().setProcessHistoryID(phid);}

    virtual bool unscheduledFill(std::string const&) const {return false;}

    RunAuxiliary aux_;
  };

}  // art

#endif /* art_Framework_Core_RunPrincipal_h */

// Local Variables:
// mode: c++
// End:
