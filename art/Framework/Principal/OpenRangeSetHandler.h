#ifndef art_Framework_Principal_OpenRangeSetHandler_h
#define art_Framework_Principal_OpenRangeSetHandler_h

// FIXME: Expand on specific use case for this class.
//
// OpenRangeSetHandler is used by the SubRunPrincipal to:
//
//   - Accept a vector of EventRanges from an input file (if present).
//   - Combine mergeable ranges from the input file.
//   - Create sliding output ranges

#include "art/Framework/Principal/RangeSetHandler.h"
#include "canvas/Persistency/Provenance/EventID.h"
#include "canvas/Persistency/Provenance/RangeSet.h"
#include "canvas/Persistency/Provenance/SubRunID.h"

#include <string>
#include <vector>

namespace art {

  class OpenRangeSetHandler : public RangeSetHandler {
  public:

    explicit OpenRangeSetHandler(RunNumber_t r);

    // This class contains an iterator as a member.
    // It should not be copied!
    OpenRangeSetHandler(OpenRangeSetHandler const&) = delete;
    OpenRangeSetHandler& operator=(OpenRangeSetHandler const&) = delete;

    OpenRangeSetHandler(OpenRangeSetHandler&&) = default;
    OpenRangeSetHandler& operator=(OpenRangeSetHandler&&) = default;

  private:

    auto begin() const { return ranges_.begin(); }
    auto end() const { return ranges_.end(); }

    RangeSet do_getSeenRanges() const override;

    void do_updateFromEvent(EventID const&, bool lastInSubRun) override;
    void do_updateFromSubRun(SubRunID const&) override;
    void do_flushRanges() override {}
    void do_maybeSplitRange() override {}
    void do_rebase() override;

    RangeSet ranges_ {RangeSet::invalid()};
    RangeSet::const_iterator rsIter_ {ranges_.begin()};
    bool lastInSubRun_ {true};
  };

}
#endif /* art_Utilities_OpenRangeSetHandler_h */

// Local Variables:
// mode: c++
// End:
