#ifndef art_Persistency_Provenance_BranchListIndex_h
#define art_Persistency_Provenance_BranchListIndex_h

/*----------------------------------------------------------------------

ProcessInfo:
ProcessInfoVector:
	table keyed by ProductID::processIndex_.
	processIndex_ is monotonically increasing with time.
        one table stored per Occurrence(Event. SubRun, Run)

----------------------------------------------------------------------*/

#include <vector>

namespace art {
  typedef unsigned short BranchListIndex;
  typedef std::vector<BranchListIndex> BranchListIndexes;
}
#endif /* art_Persistency_Provenance_BranchListIndex_h */

// Local Variables:
// mode: c++
// End:
