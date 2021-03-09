#include "art/Utilities/make_tool.h"
#include "art/test/Utilities/tools/NestedFunctionTool.h"

namespace arttest {
  int
  callThroughToAddOne(fhicl::ParameterSet const& pset, int const i)
  {
    auto addOne = art::make_tool<int(int)>(pset, "addOne");
    return addOne(i);
  }
} // namespace arttest

