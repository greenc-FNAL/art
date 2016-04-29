#ifndef test_Integration_dump_file_info_InputFile_h
#define test_Integration_dump_file_info_InputFile_h

#include "TBranch.h"
#include "TFile.h"
#include "TTree.h"
#include "art/Framework/IO/Root/Inputfwd.h"
#include "art/Framework/IO/Root/detail/resolveRangeSet.h"
#include "canvas/Persistency/Provenance/FileIndex.h"
#include "canvas/Persistency/Provenance/RangeSet.h"
#include "canvas/Persistency/Provenance/RunAuxiliary.h"
#include <memory>
#include <ostream>
#include <string>

namespace art {
  namespace detail {

    class InputFile {
    public:

      using EntryNumber = input::EntryNumber;
      using EntryNumbers = input::EntryNumbers;

      InputFile(std::string const& filename);
      void print_range_sets(std::ostream&) const;
      void print_file_index(std::ostream&) const;
      TFile* tfile() const { return file_.get(); }

    private:
      RunAuxiliary getAuxiliary(TTree* tree, EntryNumber const entry) const;

      RangeSet getRangeSet(TTree* tree,
                           EntryNumbers const& entries,
                           sqlite3* db,
                           std::string const& filename) const;

      std::unique_ptr<TFile> file_;
      FileIndex fileIndex_;
    };

  }
}

#endif

// Local variables:
// mode: c++
// End:
