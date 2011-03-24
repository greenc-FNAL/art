// ======================================================================
//
// DecrepitRelicInputSourceImplementation
//
// ======================================================================

#include "art/Framework/Core/DecrepitRelicInputSourceImplementation.h"

#include "art/Framework/Core/Event.h"
#include "art/Framework/Core/EventPrincipal.h"
#include "art/Framework/Core/FileBlock.h"
#include "art/Framework/Core/InputSourceDescription.h"
#include "art/Framework/Core/Run.h"
#include "art/Framework/Core/RunPrincipal.h"
#include "art/Framework/Core/SubRun.h"
#include "art/Framework/Core/SubRunPrincipal.h"
#include "art/Framework/Services/Registry/ActivityRegistry.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Persistency/Provenance/ProductRegistry.h"
#ifdef RNGS
//#include "art/Framework/Services/Optional/RandomNumberGenerator.h"
#endif
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "fhiclcpp/ParameterSet.h"
#include <cassert>
#include <ctime>

using fhicl::ParameterSet;

// ----------------------------------------------------------------------

namespace art {

  namespace {
    std::string const& suffix(int count) {
      static std::string const st("st");
      static std::string const nd("nd");
      static std::string const rd("rd");
      static std::string const th("th");
      // *0, *4 - *9 use "th".
      int lastDigit = count % 10;
      if (lastDigit >= 4 || lastDigit == 0) return th;
      // *11, *12, or *13 use "th".
      if (count % 100 - lastDigit == 10) return th;
      return (lastDigit == 1 ? st : (lastDigit == 2 ? nd : rd));
    }

    struct do_nothing_deleter {
      void  operator () (void const*) const { }
    };

    template <typename T>
    boost::shared_ptr<T> createSharedPtrToStatic(T * ptr) {
      return  boost::shared_ptr<T>(ptr, do_nothing_deleter());
    }
  }  // namespace

// ----------------------------------------------------------------------

  DecrepitRelicInputSourceImplementation::DecrepitRelicInputSourceImplementation( ParameterSet           const & pset
                          , InputSourceDescription const & desc )
  : ProductRegistryHelper( )
  , boost::noncopyable   ( )
  , actReg_              ( desc.actReg_ )
  , maxEvents_           ( pset.get<int>("maxEvents", -1) )
  , remainingEvents_     ( maxEvents_ )
  , maxSubRuns_            ( pset.get<int>("maxSubRuns", -1) )
  , remainingSubRuns_      ( maxSubRuns_ )
  , readCount_           ( 0 )
  , processingMode_      ( RunsSubRunsAndEvents )
  , moduleDescription_   ( desc.moduleDescription_ )
  , productRegistry_     ( desc.productRegistry_ )
  , primary_             ( pset.get<std::string>("module_label") == std::string("source") )
  , time_                ( )
  , doneReadAhead_       ( false )
  , state_               ( input::IsInvalid )
  , runPrincipal_        ( )
  , subRunPrincipal_       ( )
  {
    // Secondary input sources currently do not have a product registry.
    if (primary_) {
      assert(desc.productRegistry_ != 0);
    }
    std::string const defaultMode("RunsSubRunsAndEvents");
    std::string const runMode("Runs");
    std::string const runSubRunMode("RunsAndSubRuns");
    std::string processingMode
       = pset.get<std::string>("processingMode", defaultMode);
    if (processingMode == runMode) {
      processingMode_ = Runs;
    }
    else if (processingMode == runSubRunMode) {
      processingMode_ = RunsAndSubRuns;
    }
    else if (processingMode != defaultMode) {
      throw art::Exception(art::errors::Configuration)
        << "DecrepitRelicInputSourceImplementation::DecrepitRelicInputSourceImplementation()\n"
        << "The 'processingMode' parameter for sources has an illegal value '"
          << processingMode << "'\n"
        << "Legal values are '" << defaultMode
          << "', '" << runSubRunMode
          << "', or '" << runMode << "'.\n";
    }

    // This must come LAST in the constructor.
    registerProducts_();
  }


  void
  DecrepitRelicInputSourceImplementation::commitEvent(art::Event &e)
  {
    e.commit_();
  }


  void
  DecrepitRelicInputSourceImplementation::commitRun(art::Run &r)
  {
    r.commit_();
  }


  void
  DecrepitRelicInputSourceImplementation::commitSubRun(art::SubRun &sr)
  {
    sr.commit_();
  }


  DecrepitRelicInputSourceImplementation::~DecrepitRelicInputSourceImplementation() { }

  // This next function is to guarantee that "runs only" mode does not
  // return events or subRuns, and that "runs and subRuns only" mode
  // does not return events.  For input sources that are not random
  // access (e.g. you need to read through the events to get to the
  // subRuns and runs), this is all that is involved to implement
  // these modes.  For input sources where events or subRuns can be
  // skipped, getNextItemType() should implement the skipping
  // internally, so that the performance gain is realized.  If this is
  // done for a source, the 'if' blocks in this function will never be
  // entered for that source.
  input::ItemType
  DecrepitRelicInputSourceImplementation::nextItemType_() {
    input::ItemType itemType = getNextItemType();
    if (itemType == input::IsEvent && processingMode() != RunsSubRunsAndEvents) {
      readEvent_();
      return nextItemType_();
    }
    if (itemType == input::IsSubRun && processingMode() == Runs) {
      readSubRun_();
      return nextItemType_();
    }
    return itemType;
  }

  input::ItemType
  DecrepitRelicInputSourceImplementation::nextItemType() {
    if (doneReadAhead_) {
      return state_;
    }
    doneReadAhead_ = true;
    input::ItemType oldState = state_;
    if (eventLimitReached()) {
      // If the maximum event limit has been reached, stop.
      state_ = input::IsStop;
    }
    else if (subRunLimitReached()) {
      // If the maximum subRun limit has been reached, stop
      // when reaching a new file, run, or subRun.
      if (oldState == input::IsInvalid ||
	  oldState == input::IsFile ||
	  oldState == input::IsRun ||
	  processingMode() != RunsSubRunsAndEvents) {
        state_ = input::IsStop;
      }
      else {
	input::ItemType newState = nextItemType_();
        if (newState == input::IsEvent) {
          assert(processingMode() == RunsSubRunsAndEvents);
          state_ = input::IsEvent;
        }
        else {
          state_ = input::IsStop;
        }
      }
    }
    else {
      input::ItemType newState = nextItemType_();
      if (newState == input::IsStop) {
        state_ = input::IsStop;
      }
      else if (newState == input::IsFile || oldState == input::IsInvalid) {
        state_ = input::IsFile;
      }
      else if (newState == input::IsRun || oldState == input::IsFile) {
        setRunPrincipal(readRun_());
        state_ = input::IsRun;
      }
      else if (newState == input::IsSubRun || oldState == input::IsRun) {
        assert(processingMode() != Runs);
        setSubRunPrincipal(readSubRun_());
        state_ = input::IsSubRun;
      }
      else {
        assert(processingMode() == RunsSubRunsAndEvents);
        state_ = input::IsEvent;
      }
    }
    if (state_ == input::IsStop) {
      subRunPrincipal_.reset();
      runPrincipal_.reset();
    }
    return state_;
  }

  void
  DecrepitRelicInputSourceImplementation::doBeginJob() {
    beginJob();
  }

  void
  DecrepitRelicInputSourceImplementation::doEndJob() {
    endJob();
  }

  void
  DecrepitRelicInputSourceImplementation::registerProducts_()
  {
    addToRegistry(typeLabelList().begin(),
		  typeLabelList().end(),
		  moduleDescription(),
		  productRegistryUpdate());
  }

  // Return a dummy file block.
  boost::shared_ptr<FileBlock>
  DecrepitRelicInputSourceImplementation::readFile()
  {
    assert(doneReadAhead_);
    assert(state_ == input::IsFile);
    assert(!limitReached());
    doneReadAhead_ = false;
    boost::shared_ptr<FileBlock> fb = readFile_();
    return fb;
  }

  void
  DecrepitRelicInputSourceImplementation::closeFile() {
    return closeFile_();
  }

  // Return a dummy file block.
  // This function must be overridden for any input source that reads a file
  // containing Products.
  boost::shared_ptr<FileBlock>
  DecrepitRelicInputSourceImplementation::readFile_() {
    return boost::shared_ptr<FileBlock>(new FileBlock);
  }

  boost::shared_ptr<RunPrincipal>
  DecrepitRelicInputSourceImplementation::readRun() {
    // Note: For the moment, we do not support saving and restoring the state of the
    // random number generator if random numbers are generated during processing of runs
    // (e.g. beginRun(), endRun())
    assert(doneReadAhead_);
    assert(state_ == input::IsRun);
    assert(!limitReached());
    doneReadAhead_ = false;
    return runPrincipal_;
  }

  boost::shared_ptr<SubRunPrincipal>
  DecrepitRelicInputSourceImplementation::readSubRun(boost::shared_ptr<RunPrincipal> rp) {
    // Note: For the moment, we do not support saving and restoring the state of the
    // random number generator if random numbers are generated during processing of subRuns
    // (e.g. beginSubRun(), endSubRun())
    assert(doneReadAhead_);
    assert(state_ == input::IsSubRun);
    assert(!limitReached());
    doneReadAhead_ = false;
    --remainingSubRuns_;
    assert(subRunPrincipal_->run() == rp->run());
    subRunPrincipal_->setRunPrincipal(rp);
    return subRunPrincipal_;
  }

  std::auto_ptr<EventPrincipal>
  DecrepitRelicInputSourceImplementation::readEvent(boost::shared_ptr<SubRunPrincipal> srp) {
    assert(doneReadAhead_);
    assert(state_ == input::IsEvent);
    assert(!eventLimitReached());
    doneReadAhead_ = false;

    preRead();
    std::auto_ptr<EventPrincipal> result = readEvent_();
    assert(srp->run() == result->run());
    assert(srp->subRun() == result->subRun());
    result->setSubRunPrincipal(srp);
    if (result.get() != 0) {
      Event event(*result, moduleDescription());
      postRead(event);
      if (remainingEvents_ > 0) --remainingEvents_;
      ++readCount_;
      setTimestamp(result->time());
      issueReports(result->id(), result->subRun());
    }
    return result;
  }

  std::auto_ptr<EventPrincipal>
  DecrepitRelicInputSourceImplementation::readEvent(EventID const& eventID) {
    std::auto_ptr<EventPrincipal> result(0);

    if (!limitReached()) {
      preRead();
      result = readIt(eventID);
      if (result.get() != 0) {
        Event event(*result, moduleDescription());
        postRead(event);
        if (remainingEvents_ > 0) --remainingEvents_;
        ++readCount_;
        issueReports(result->id(), result->subRun());
      }
    }
    return result;
  }

  void
  DecrepitRelicInputSourceImplementation::skipEvents(int offset) {
    this->skip(offset);
  }

  void
  DecrepitRelicInputSourceImplementation::issueReports(EventID const& eventID, SubRunNumber_t const& subRun) {
    time_t t = time(0);
    char ts[] = "dd-Mon-yyyy hh:mm:ss TZN     ";
    strftime( ts, strlen(ts)+1, "%d-%b-%Y %H:%M:%S %Z", localtime(&t) );
    mf::LogVerbatim("ArtReport")
      << "Begin processing the " << readCount_
      << suffix(readCount_) << " record. Run " << eventID.run()
      << ", SubRun " << subRun
      << ", Event " << eventID.event()
      << " at " << ts;
    // At some point we may want to initiate checkpointing here
  }

  std::auto_ptr<EventPrincipal>
  DecrepitRelicInputSourceImplementation::readIt(EventID const&) {
    throw art::Exception(art::errors::LogicError)
      << "DecrepitRelicInputSourceImplementation::readIt()\n"
      << "Random access is not implemented for this type of Input Source\n"
      << "Contact a Framework Developer\n";
  }

  void
  DecrepitRelicInputSourceImplementation::skip(int) {
    throw art::Exception(art::errors::LogicError)
      << "DecrepitRelicInputSourceImplementation::skip()\n"
      << "Random access is not implemented for this type of Input Source\n"
      << "Contact a Framework Developer\n";
  }

  void
  DecrepitRelicInputSourceImplementation::rewind_() {
    throw art::Exception(art::errors::LogicError)
      << "DecrepitRelicInputSourceImplementation::rewind()\n"
      << "Rewind is not implemented for this type of Input Source\n"
      << "Contact a Framework Developer\n";
  }

  void
  DecrepitRelicInputSourceImplementation::preRead() {  // roughly corresponds to "end of the prev event"
#ifdef RNGS
    if (primary()) {
      ServiceHandle<RandomNumberGenerator> rng;
      if (rng.isAvailable()) {
        rng->takeSnapshot_();
      }
    }
#endif
  }

  void
  DecrepitRelicInputSourceImplementation::postRead(Event& event) {
#ifdef RNGS
    if (primary()) {
      ServiceHandle<RandomNumberGenerator> rng;
      if (rng.isAvailable()) {
        rng->restoreSnapshot_(event);
      }
    }
#endif
  }

  void
  DecrepitRelicInputSourceImplementation::doEndRun(RunPrincipal& rp) {
    rp.setEndTime(time_);
    Run run(rp, moduleDescription());
    endRun(run);
    run.commit_();
  }

  void
  DecrepitRelicInputSourceImplementation::doEndSubRun(SubRunPrincipal & srp) {
    srp.setEndTime(time_);
    SubRun sr(srp, moduleDescription());
    endSubRun(sr);
    sr.commit_();
  }

//   void
//   DecrepitRelicInputSourceImplementation::wakeUp_() { }

  void
  DecrepitRelicInputSourceImplementation::endSubRun(SubRun &) { }

  void
  DecrepitRelicInputSourceImplementation::endRun(Run &) { }

  void
  DecrepitRelicInputSourceImplementation::beginJob() { }

  void
  DecrepitRelicInputSourceImplementation::endJob() { }

  RunNumber_t
  DecrepitRelicInputSourceImplementation::run() const {
    assert(runPrincipal());
    return runPrincipal()->run();
  }

  SubRunNumber_t
  DecrepitRelicInputSourceImplementation::subRun() const {
    assert(subRunPrincipal());
    return subRunPrincipal()->subRun();
  }

}  // art

// ======================================================================
