#ifndef art_Persistency_Provenance_BranchChildren_h
#define art_Persistency_Provenance_BranchChildren_h

/*----------------------------------------------------------------------

BranchChildren: Dependency information between branches.

----------------------------------------------------------------------*/

#include "art/Persistency/Provenance/BranchID.h"

#include <map>
#include <set>

namespace art {

  class BranchChildren {
  private:
    using BranchIDSet = std::set<BranchID>;
  public:

    // Clear all information.
    void clear();

    // Insert a parent with no children.
    void insertEmpty(BranchID parent);

    // Insert a new child for the given parent.
    void insertChild(BranchID parent, BranchID child);

    // Look up all the descendants of the given parent, and insert them
    // into descendants. N.B.: this does not clear out descendants first;
    // it only appends *new* elements to the collection.
    void appendToDescendants(BranchID parent, BranchIDSet& descendants) const;

  private:
    using map_t = std::map<BranchID, BranchIDSet>;
    map_t childLookup_;

    void append_(map_t const& lookup, BranchID item, BranchIDSet& itemSet) const;
  };

}
#endif /* art_Persistency_Provenance_BranchChildren_h */

// Local Variables:
// mode: c++
// End:
