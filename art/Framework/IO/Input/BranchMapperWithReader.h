#ifndef art_Framework_IO_Input_BranchMapperWithReader_h
#define art_Framework_IO_Input_BranchMapperWithReader_h

// ======================================================================
//
// BranchMapperWithReader
//
// ======================================================================

#include "TBranch.h"
#include "art/Framework/IO/Input/Inputfwd.h"
#include "art/Persistency/Provenance/BranchID.h"
#include "art/Persistency/Provenance/BranchMapper.h"
#include "art/Persistency/Provenance/EventEntryInfo.h"
#include "art/Persistency/Provenance/ProductID.h"
#include <vector>

// ----------------------------------------------------------------------

class TBranch;

namespace art {

  template <typename T>
  class BranchMapperWithReader : public BranchMapper {
  public:
    BranchMapperWithReader(TBranch * branch, input::EntryNumber entryNumber);

    virtual ~BranchMapperWithReader() {}

  private:
    virtual void readProvenance_() const;

    TBranch * branchPtr_;
    input::EntryNumber entryNumber_;
    std::vector<T> infoVector_;
    mutable std::vector<T> * pInfoVector_;
  };  // BranchMapperWithReader<>

  template <typename T>
  BranchMapperWithReader<T>::BranchMapperWithReader(TBranch * branch, input::EntryNumber entryNumber) :
         BranchMapper(true),
         branchPtr_(branch), entryNumber_(entryNumber),
         infoVector_(), pInfoVector_(&infoVector_)
  { }

  template <typename T>
  void
  BranchMapperWithReader<T>::readProvenance_() const {
    branchPtr_->SetAddress(&pInfoVector_);
    input::getEntry(branchPtr_, entryNumber_);
    BranchMapperWithReader<T> * me = const_cast<BranchMapperWithReader<T> *>(this);
    for (typename std::vector<T>::const_iterator it = infoVector_.begin(), itEnd = infoVector_.end();
      it != itEnd; ++it) {
      me->insert(it->makeProductProvenance());
    }
  }

  // Backward compatibility
  template <>
  class BranchMapperWithReader<EventEntryInfo> : public BranchMapper {
  public:
    BranchMapperWithReader(TBranch * branch, input::EntryNumber entryNumber) :
         BranchMapper(true),
         branchPtr_(branch), entryNumber_(entryNumber),
         infoVector_(), pInfoVector_(&infoVector_), oldProductIDToBranchIDMap_()
  { }

    virtual ~BranchMapperWithReader() {}

    void insertIntoMap(ProductID const& oldProductID, BranchID const& branchID);

  private:
    virtual void readProvenance_() const;
    virtual BranchID oldProductIDToBranchID_(ProductID const& oldProductID) const;

    TBranch * branchPtr_;
    input::EntryNumber entryNumber_;
    std::vector<EventEntryInfo> infoVector_;
    mutable std::vector<EventEntryInfo> * pInfoVector_;
    std::map<unsigned int, BranchID> oldProductIDToBranchIDMap_;
  };  // BranchMapperWithReader<EventEntryInfo>

}  // art

// ======================================================================

#endif /* art_Framework_IO_Input_BranchMapperWithReader_h */

// Local Variables:
// mode: c++
// End:
