/*----------------------------------------------------------------------

Test of the EventPrincipal class.

----------------------------------------------------------------------*/
#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>

#include "art/Utilities/EDMException.h"
#include "art/Utilities/GetPassID.h"
#include "art/Version/GetReleaseVersion.h"
#include "art/Utilities/GlobalIdentifier.h"
#include "art/Utilities/TypeID.h"
#include "art/Persistency/Provenance/EventAuxiliary.h"
#include "art/Persistency/Provenance/SubRunAuxiliary.h"
#include "art/Persistency/Provenance/ModuleDescription.h"
#include "art/Persistency/Provenance/RunAuxiliary.h"
#include "art/Persistency/Provenance/ProductRegistry.h"
#include "art/Persistency/Provenance/BranchDescription.h"
#include "art/Persistency/Provenance/BranchIDListHelper.h"
#include "art/Persistency/Provenance/Timestamp.h"
//#include "art/Framework/Core/Selector.h"
#include "test/TestObjects/ToyProducts.h"

#include "art/Framework/Core/EventPrincipal.h"
#include "art/Framework/Core/SubRunPrincipal.h"
#include "art/Framework/Core/RunPrincipal.h"

//have to do this evil in order to access commit_ member function
#define private public
#include "art/Framework/Core/Event.h"
#undef private

#include <cppunit/extensions/HelperMacros.h>

class testEventGetRefBeforePut: public CppUnit::TestFixture {
CPPUNIT_TEST_SUITE(testEventGetRefBeforePut);
CPPUNIT_TEST(failGetProductNotRegisteredTest);
CPPUNIT_TEST(getRefTest);
CPPUNIT_TEST_SUITE_END();
public:
  void setUp(){}
  void tearDown(){}
  void failGetProductNotRegisteredTest();
  void getRefTest();
};

///registration of the test so that the runner can find it
CPPUNIT_TEST_SUITE_REGISTRATION(testEventGetRefBeforePut);

void testEventGetRefBeforePut::failGetProductNotRegisteredTest() {

  edm::ProductRegistry *preg = new edm::ProductRegistry;
  preg->setFrozen();
  edm::BranchIDListHelper::updateRegistries(*preg);
  edm::EventID col(1L, 1L);
  std::string uuid = edm::createGlobalIdentifier();
  edm::Timestamp fakeTime;
  edm::ProcessConfiguration pc("PROD", edm::ParameterSetID(), edm::getReleaseVersion(), edm::getPassID());
  boost::shared_ptr<edm::ProductRegistry const> pregc(preg);
  edm::RunAuxiliary runAux(col.run(), fakeTime, fakeTime);
  boost::shared_ptr<edm::RunPrincipal> rp(new edm::RunPrincipal(runAux, pregc, pc));
  edm::SubRunAuxiliary subRunAux(rp->run(), 1, fakeTime, fakeTime);
  boost::shared_ptr<edm::SubRunPrincipal>lbp(new edm::SubRunPrincipal(subRunAux, pregc, pc));
  lbp->setRunPrincipal(rp);
  edm::EventAuxiliary eventAux(col, uuid, fakeTime, lbp->subRun(), true);
  edm::EventPrincipal ep(eventAux, pregc, pc);
  ep.setSubRunPrincipal(lbp);
  try {
     edm::ModuleDescription modDesc;
     modDesc.moduleName_ = "Blah";
     modDesc.moduleLabel_ = "blahs";
     edm::Event event(ep, modDesc);

     std::string label("this does not exist");
     edm::RefProd<edmtest::DummyProduct> ref = event.getRefBeforePut<edmtest::DummyProduct>(label);
     CPPUNIT_ASSERT("Failed to throw required exception" == 0);
  }
  catch (edm::Exception& x) {
    // nothing to do
  }
  catch (...) {
    CPPUNIT_ASSERT("Threw wrong kind of exception" == 0);
  }

}

void testEventGetRefBeforePut::getRefTest() {
  std::string processName = "PROD";

  std::string label("fred");
  std::string productInstanceName("Rick");

  edmtest::IntProduct dp;
  edm::TypeID dummytype(dp);
  std::string className = dummytype.friendlyClassName();

  edm::ModuleDescription modDesc;
  modDesc.moduleName_ = "Blah";
  modDesc.parameterSetID_ = edm::ParameterSet().id();

  edm::BranchDescription product(edm::InEvent,
				 label,
				 processName,
				 dummytype.userClassName(),
				 className,
				 productInstanceName,
				 modDesc
				);

  product.init();

  edm::ProductRegistry *preg = new edm::ProductRegistry;
  preg->addProduct(product);
  preg->setFrozen();
  edm::BranchIDListHelper::updateRegistries(*preg);
  edm::EventID col(1L, 1L);
  std::string uuid = edm::createGlobalIdentifier();
  edm::Timestamp fakeTime;
  edm::ProcessConfiguration pc(processName, edm::ParameterSetID(), edm::getReleaseVersion(), edm::getPassID());
  boost::shared_ptr<edm::ProductRegistry const> pregc(preg);
  edm::RunAuxiliary runAux(col.run(), fakeTime, fakeTime);
  boost::shared_ptr<edm::RunPrincipal> rp(new edm::RunPrincipal(runAux, pregc, pc));
  edm::SubRunAuxiliary subRunAux(rp->run(), 1, fakeTime, fakeTime);
  boost::shared_ptr<edm::SubRunPrincipal>lbp(new edm::SubRunPrincipal(subRunAux, pregc, pc));
  lbp->setRunPrincipal(rp);
  edm::EventAuxiliary eventAux(col, uuid, fakeTime, lbp->subRun(), true);
  edm::EventPrincipal ep(eventAux, pregc, pc);
  ep.setSubRunPrincipal(lbp);

  edm::RefProd<edmtest::IntProduct> refToProd;
  try {
    edm::ModuleDescription modDesc;
    modDesc.moduleName_="Blah";
    modDesc.moduleLabel_=label;
    modDesc.processConfiguration_ = pc;

    edm::Event event(ep, modDesc);
    std::auto_ptr<edmtest::IntProduct> pr(new edmtest::IntProduct);
    pr->value = 10;

    refToProd = event.getRefBeforePut<edmtest::IntProduct>(productInstanceName);
    event.put(pr,productInstanceName);
    event.commit_();
  }
  catch (cms::Exception& x) {
    std::cerr << x.explainSelf()<< std::endl;
    CPPUNIT_ASSERT("Threw exception unexpectedly" == 0);
  }
  catch(std::exception& x){
     std::cerr <<x.what()<<std::endl;
     CPPUNIT_ASSERT("threw std::exception"==0);
  }
  catch (...) {
    std::cerr << "Unknown exception type\n";
    CPPUNIT_ASSERT("Threw exception unexpectedly" == 0);
  }
  CPPUNIT_ASSERT(refToProd->value == 10);
}

