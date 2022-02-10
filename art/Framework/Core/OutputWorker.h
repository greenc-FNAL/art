#ifndef art_Framework_Core_OutputWorker_h
#define art_Framework_Core_OutputWorker_h
// vim: set sw=2 expandtab :

//  The OutputModule as the schedule sees it.  The job of this object
//  is to call the output module.
//
//  According to our current definition, a single output module can
//  only appear in one worker.

#include "art/Framework/Core/OutputFileGranularity.h"
#include "art/Framework/Core/OutputFileStatus.h"
#include "art/Framework/Core/WorkerT.h"
#include "art/Framework/Core/fwd.h"
#include "art/Framework/Principal/fwd.h"
#include "art/Framework/Services/FileServiceInterfaces/CatalogInterface.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "canvas/Persistency/Provenance/fwd.h"

#include <memory>

namespace art {
  struct OutputModuleDescription;
  class OutputWorker : public Worker {
  public:
    virtual ~OutputWorker();
    // This is called directly by the make_worker function created
    // by the DEFINE_ART_MODULE macro.
    OutputWorker(std::shared_ptr<OutputModule> mod,
                 ModuleDescription const&,
                 WorkerParams const&);

    std::string const& lastClosedFileName() const;
    bool closeFile();
    bool fileIsOpen() const;
    void incrementInputFileNumber();
    bool requestsToCloseFile() const;
    bool openFile(FileBlock const& fb);
    void writeRun(RunPrincipal& rp);
    void writeSubRun(SubRunPrincipal& srp);
    void writeEvent(EventPrincipal& ep, ModuleContext const& mc);
    void setRunAuxiliaryRangeSetID(RangeSet const&);
    void setSubRunAuxiliaryRangeSetID(RangeSet const&);
    bool limitReached() const;
    void setFileStatus(OutputFileStatus);
    void configure(OutputModuleDescription const& desc);
    Granularity fileGranularity() const;
    void selectProducts(ProductTables const&);

  private:
    hep::concurrency::SerialTaskQueueChain* implSerialTaskQueueChain()
      const override;
    void implBeginJob(detail::SharedResources const&) override;
    void implEndJob() override;
    void implRespondToOpenInputFile(FileBlock const&) override;
    void implRespondToCloseInputFile(FileBlock const&) override;
    void implRespondToOpenOutputFiles(FileBlock const&) override;
    void implRespondToCloseOutputFiles(FileBlock const&) override;
    bool implDoBegin(RunPrincipal&, ModuleContext const&) override;
    bool implDoEnd(RunPrincipal&, ModuleContext const&) override;
    bool implDoBegin(SubRunPrincipal&, ModuleContext const&) override;
    bool implDoEnd(SubRunPrincipal&, ModuleContext const&) override;
    bool implDoProcess(EventPrincipal&, ModuleContext const&) override;

    // A module is co-owned by one worker per schedule.  Only
    // replicated modules have a one-to-one correspondence with their
    // worker.
    std::shared_ptr<OutputModule> module_;
    ServiceHandle<CatalogInterface> ci_{};
    Granularity fileGranularity_{Granularity::Unset};
  };
} // namespace art

#endif /* art_Framework_Core_OutputWorker_h */

// Local Variables:
// mode: c++
// End:
