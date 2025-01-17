#define BOOST_TEST_MODULE (artapp basic source options)
#include "boost/test/unit_test.hpp"

#include "art/Framework/Art/detail/fillSourceList.h"

#include <sstream>

BOOST_AUTO_TEST_SUITE(artappBasicSourceOptionsTest)

BOOST_AUTO_TEST_CASE(SourceListWithComments)
{
  std::string const files{"f1.root\n"
                          "# This is a random comment\n"
                          "#\n"
                          "# f2.root\n"
                          "f3.root\n"
                          "f4#.root#something\n"
                          "f5.root# this file is sometimes problematic\n"
                          "f6.root\n"};

  std::istringstream is(files);
  std::vector<std::string> slist;
  art::detail::fillSourceList(is, slist);

  auto const ref = {"f1.root", "f3.root", "f4", "f5.root", "f6.root"};
  BOOST_TEST(slist == ref, boost::test_tools::per_element{});
}

BOOST_AUTO_TEST_SUITE_END()
