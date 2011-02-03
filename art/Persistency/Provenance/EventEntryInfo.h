#ifndef art_Persistency_Provenance_EventEntryInfo_h
#define art_Persistency_Provenance_EventEntryInfo_h

/*----------------------------------------------------------------------

EventEntryInfo: The event dependent portion of the description of a product
and how it came into existence, plus the product identifier and the status.

----------------------------------------------------------------------*/
#include <iosfwd>
#include <vector>

#include "boost/shared_ptr.hpp"

#include "art/Persistency/Provenance/BranchID.h"
#include "art/Persistency/Provenance/EntryDescriptionID.h"
#include "art/Persistency/Provenance/ModuleDescriptionID.h"
#include "art/Persistency/Provenance/ProductID.h"
#include "art/Persistency/Provenance/ProductProvenance.h"
#include "art/Persistency/Provenance/ProductStatus.h"
#include "art/Persistency/Provenance/ProvenanceFwd.h"
#include "art/Persistency/Provenance/Transient.h"

/*
  EventEntryInfo
*/

namespace art {
  class EventEntryDescription;
  class EventEntryInfo {
  public:
    typedef std::vector<EventEntryInfo> EntryInfoVector;
    EventEntryInfo();
    explicit EventEntryInfo(BranchID const& bid);
    EventEntryInfo(BranchID const& bid,
		    ProductStatus status,
		    ProductID const& pid);
    EventEntryInfo(BranchID const& bid,
		    ProductStatus status,
		    ProductID const& pid,
		    boost::shared_ptr<EventEntryDescription> edPtr);
    EventEntryInfo(BranchID const& bid,
		    ProductStatus status,
		    ProductID const& pid,
		    EntryDescriptionID const& edid);

    EventEntryInfo(BranchID const& bid,
		   ProductStatus status,
		   ModuleDescriptionID const& mdid,
		   ProductID const& pid,
		   std::vector<BranchID> const& parents = std::vector<BranchID>());

    EventEntryInfo(BranchID const& bid,
		   ProductStatus status,
		   ModuleDescriptionID const& mdid);

    ~EventEntryInfo() {}

    ProductProvenance makeProductProvenance() const;

    void write(std::ostream& os) const;

    BranchID const& branchID() const {return branchID_;}
    ProductID const& productID() const {return productID_;}
    ProductStatus const& productStatus() const {return productStatus_;}
    EntryDescriptionID const& entryDescriptionID() const {return entryDescriptionID_;}
    EventEntryDescription const& entryDescription() const;
    void setStatus(ProductStatus status) {productStatus_ = status;}
    void setPresent();
    void setNotPresent();
    void setModuleDescriptionID(ModuleDescriptionID const& mdid) {moduleDescriptionID() = mdid;}

    ModuleDescriptionID & moduleDescriptionID() const {return transients_.get().moduleDescriptionID_;}

    bool & noEntryDescription() const {return transients_.get().noEntryDescription_;}

    struct Transients {
      Transients();
      ModuleDescriptionID moduleDescriptionID_;
      boost::shared_ptr<EventEntryDescription> entryDescriptionPtr_;
      bool noEntryDescription_;
    };

  private:

    boost::shared_ptr<EventEntryDescription> & entryDescriptionPtr() const {return transients_.get().entryDescriptionPtr_;}

    BranchID branchID_;
    ProductID productID_;
    ProductStatus productStatus_;
    EntryDescriptionID entryDescriptionID_;
    mutable Transient<Transients> transients_;
  };

  inline
  bool
  operator < (EventEntryInfo const& a, EventEntryInfo const& b) {
    return a.branchID() < b.branchID();
  }

  inline
  std::ostream&
  operator<<(std::ostream& os, EventEntryInfo const& p) {
    p.write(os);
    return os;
  }

  // Only the 'salient attributes' are testing in equality comparison.
  bool operator==(EventEntryInfo const& a, EventEntryInfo const& b);
  inline bool operator!=(EventEntryInfo const& a, EventEntryInfo const& b) { return !(a==b); }
  typedef std::vector<EventEntryInfo> EventEntryInfoVector;
}
#endif /* art_Persistency_Provenance_EventEntryInfo_h */

// Local Variables:
// mode: c++
// End:
