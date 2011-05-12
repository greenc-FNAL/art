#ifndef art_Framework_Core_OutputModule_h
#define art_Framework_Core_OutputModule_h

// ======================================================================
//
// OutputModule - The base class of all "modules" that write Events to an
//                output stream.
//
// ======================================================================

#include "art/Framework/Core/CachedProducts.h"
#include "art/Framework/Core/EventObserver.h"
#include "art/Framework/Core/FCPfwd.h"
#include "art/Framework/Core/Frameworkfwd.h"
#include "art/Framework/Core/GroupSelector.h"
#include "art/Framework/Core/GroupSelectorRules.h"
#include "art/Framework/Core/OutputModuleDescription.h"
#include "art/Framework/Core/OutputWorker.h"
#include "art/Persistency/Provenance/BranchChildren.h"
#include "art/Persistency/Provenance/BranchID.h"
#include "art/Persistency/Provenance/BranchType.h"
#include "art/Persistency/Provenance/ModuleDescription.h"
#include "art/Persistency/Provenance/ParentageID.h"
#include "art/Persistency/Provenance/Selections.h"
#include "boost/noncopyable.hpp"
#include "cpp0x/array"
#include "cpp0x/memory"
#include <string>
#include <vector>

// ----------------------------------------------------------------------

namespace art {

  typedef art::detail::CachedProducts::handle_t Trig;

  std::vector<std::string> const& getAllTriggerNames();

  class OutputModule : public EventObserver,
     private boost::noncopyable {
  public:
    template <typename T> friend class WorkerT;
    friend class OutputWorker;
    typedef OutputModule ModuleType;
    typedef OutputWorker WorkerType;

    explicit OutputModule(fhicl::ParameterSet const& pset);
    virtual ~OutputModule();
    void reconfigure(fhicl::ParameterSet const&);
    // Accessor for maximum number of events to be written.
    // -1 is used for unlimited.
    int maxEvents() const {return maxEvents_;}

    // Accessor for remaining number of events to be written.
    // -1 is used for unlimited.
    int remainingEvents() const {return remainingEvents_;}

    bool selected(BranchDescription const& desc) const;

    void selectProducts();
    std::string const& processName() const {return process_name_;}
    SelectionsArray const& keptProducts() const {return keptProducts_;}
    std::array<bool, NumBranchTypes> const& hasNewlyDroppedBranch() const {return hasNewlyDroppedBranch_;}

    BranchChildren const& branchChildren() const {return branchChildren_;}

  protected:
    //const Trig& getTriggerResults(Event const& ep) const;
    Trig getTriggerResults(Event const& ep) const;

    // The returned pointer will be null unless the this is currently
    // executing its event loop function ('write').
    CurrentProcessingContext const* currentContext() const;

    ModuleDescription const& description() const;

    bool wantAllEvents() const {return wantAllEvents_;}

    fhicl::ParameterSetID selectorConfig() const { return selector_config_id_; }

  private:

    int maxEvents_;
    int remainingEvents_;

    // TODO: Give OutputModule
    // an interface (protected?) that supplies client code with the
    // needed functionality *without* giving away implementation
    // details ... don't just return a reference to keptProducts_, because
    // we are looking to have the flexibility to change the
    // implementation of keptProducts_ without modifying clients. When this
    // change is made, we'll have a one-time-only task of modifying
    // clients (classes derived from OutputModule) to use the
    // newly-introduced interface.
    // TODO: Consider using shared pointers here?

    // keptProducts_ are pointers to the BranchDescription objects describing
    // the branches we are to write.
    //
    // We do not own the BranchDescriptions to which we point.
    SelectionsArray keptProducts_;

    std::array<bool, NumBranchTypes> hasNewlyDroppedBranch_;

    std::string process_name_;
    GroupSelectorRules groupSelectorRules_;
    GroupSelector groupSelector_;
    ModuleDescription moduleDescription_;

    // We do not own the pointed-to CurrentProcessingContext.
    CurrentProcessingContext const* current_context_;

    //This will store TriggerResults objects for the current event.
    // mutable std::vector<Trig> prods_;
    mutable bool prodsValid_;

    bool wantAllEvents_;
    mutable detail::CachedProducts selectors_;
    // ID of the ParameterSet that configured the event selector
    // subsystem.
    fhicl::ParameterSetID selector_config_id_;

    typedef std::map<BranchID, std::set<ParentageID> > BranchParents;
    BranchParents branchParents_;

    BranchChildren branchChildren_;

    //------------------------------------------------------------------
    // private member functions
    //------------------------------------------------------------------
    void configure(OutputModuleDescription const& desc);
    void doBeginJob();
    void doEndJob();
    bool doEvent(EventPrincipal const& ep,
                    CurrentProcessingContext const* cpc);
    bool doBeginRun(RunPrincipal const& rp,
                    CurrentProcessingContext const* cpc);
    bool doEndRun(RunPrincipal const& rp,
                    CurrentProcessingContext const* cpc);
    bool doBeginSubRun(SubRunPrincipal const& srp,
                    CurrentProcessingContext const* cpc);
    bool doEndSubRun(SubRunPrincipal const& srp,
                    CurrentProcessingContext const* cpc);
    void doWriteRun(RunPrincipal const& rp);
    void doWriteSubRun(SubRunPrincipal const& srp);
    void doOpenFile(FileBlock const& fb);
    void doRespondToOpenInputFile(FileBlock const& fb);
    void doRespondToCloseInputFile(FileBlock const& fb);
    void doRespondToOpenOutputFiles(FileBlock const& fb);
    void doRespondToCloseOutputFiles(FileBlock const& fb);

    std::string workerType() const {return "OutputWorker";}

    // Tell the OutputModule that is must end the current file.
    void doCloseFile();

    // Tell the OutputModule to open an output file, if one is not
    // already open.
    void maybeOpenFile();


    // Do the end-of-file tasks; this is only called internally, after
    // the appropriate tests have been done.
    void reallyCloseFile();

    void registerAnyProducts(std::shared_ptr<OutputModule>const&, ProductRegistry const*) {}

    // Ask the OutputModule if we should end the current file.
    virtual bool shouldWeCloseFile() const {return false;}

    virtual void write(EventPrincipal const& e) = 0;
    virtual void beginJob(){}
    virtual void endJob(){}
    virtual void beginRun(RunPrincipal const& r){}
    virtual void endRun(RunPrincipal const& r){}
    virtual void writeRun(RunPrincipal const& r) = 0;
    virtual void beginSubRun(SubRunPrincipal const& sr){}
    virtual void endSubRun(SubRunPrincipal const& sr){}
    virtual void writeSubRun(SubRunPrincipal const& sr) = 0;
    virtual void openFile(FileBlock const& fb) {}
    virtual void respondToOpenInputFile(FileBlock const& fb) {}
    virtual void respondToCloseInputFile(FileBlock const& fb) {}
    virtual void respondToOpenOutputFiles(FileBlock const& fb) {}
    virtual void respondToCloseOutputFiles(FileBlock const& fb) {}

    virtual bool isFileOpen() const { return true; }

    virtual void doOpenFile() { }

    void setModuleDescription(ModuleDescription const& md) {
      moduleDescription_ = md;
    }

    void updateBranchParents(EventPrincipal const& ep);
    void fillDependencyGraph();

    bool limitReached() const {return remainingEvents_ == 0;}

    // The following member functions are part of the Template Method
    // pattern, used for implementing doCloseFile() and maybeEndFile().

    virtual void startEndFile() {}
    virtual void writeFileFormatVersion() {}
    virtual void writeFileIdentifier() {}
    virtual void writeFileIndex() {}
    virtual void writeEventHistory() {}
    virtual void writeProcessConfigurationRegistry() {}
    virtual void writeProcessHistoryRegistry() {}
    virtual void writeParameterSetRegistry() {}
    virtual void writeBranchIDListRegistry() {}
    virtual void writeParentageRegistry() {}
    virtual void writeProductDescriptionRegistry() {}
    virtual void writeProductDependencies() {}
    virtual void writeBranchMapper() {}
    virtual void finishEndFile() {}
  };  // OutputModule

}  // art

// ======================================================================

#endif /* art_Framework_Core_OutputModule_h */

// Local Variables:
// mode: c++
// End:
