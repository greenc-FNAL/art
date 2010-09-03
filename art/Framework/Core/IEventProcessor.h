#ifndef FWCore_Framework_IEventProcessor_h
#define FWCore_Framework_IEventProcessor_h

/*


Abstract base class for Event Processors

Original Authors: W. David Dagenhart, Marc Paterno
*/

#include <string>

namespace statemachine {
  class Restart;
}

namespace edm
{
  class IEventProcessor {
  public:

    // Status codes:
    //   0     successful completion
    //   1     exception of unknown type caught
    //   2     everything else
    //   3     signal received
    //   4     input complete
    //   5     call timed out
    //   6     input count complete
    enum Status { epSuccess=0, epException=1, epOther=2, epSignal=3,
                  epInputComplete=4, epTimedOut=5, epCountComplete=6 };

    // Eventually, we might replace StatusCode with a class. This
    // class should have an automatic conversion to 'int'.
    typedef Status StatusCode ;

    virtual ~IEventProcessor();

    virtual StatusCode runToCompletion(bool onlineStateTransitions) = 0;
    virtual StatusCode runEventCount(int numberOfEventsToProcess) = 0;

    virtual void readFile() = 0;
    virtual void closeInputFile() = 0;
    virtual void openOutputFiles() = 0;
    virtual void closeOutputFiles() = 0;

    virtual void respondToOpenInputFile() = 0;
    virtual void respondToCloseInputFile() = 0;
    virtual void respondToOpenOutputFiles() = 0;
    virtual void respondToCloseOutputFiles() = 0;

    virtual void startingNewLoop() = 0;
    virtual bool endOfLoop() = 0;
    virtual void rewindInput() = 0;
    virtual void prepareForNextLoop() = 0;
    virtual void writeSubRunCache() = 0;
    virtual void writeRunCache() = 0;
    virtual bool shouldWeCloseOutput() const = 0;

    virtual void doErrorStuff() = 0;

    virtual void beginRun(int run) = 0;
    virtual void endRun(int run) = 0;

    virtual void beginSubRun(int run, int subRun) = 0;
    virtual void endSubRun(int run, int subRun) = 0;

    virtual int readAndCacheRun() = 0;
    virtual int readAndCacheSubRun() = 0;
    virtual void writeRun(int run) = 0;
    virtual void deleteRunFromCache(int run) = 0;
    virtual void writeSubRun(int run, int subRun) = 0;
    virtual void deleteSubRunFromCache(int run, int subRun) = 0;

    virtual void readEvent() = 0;
    virtual void processEvent() = 0;
    virtual bool shouldWeStop() const = 0;

    virtual void setExceptionMessageFiles(std::string& message) = 0;
    virtual void setExceptionMessageRuns(std::string& message) = 0;
    virtual void setExceptionMessageSubRuns(std::string& message) = 0;

    virtual bool alreadyHandlingException() const = 0;
  };
}

#endif
