
#define HWY_HEADER_ONLY

#include <string>
#include "hwy/base.h"
#include "hwy/tests/hwy_gtest.h"
#include "hwy/tests/test_util-inl.h"  // HWY_ASSERT_EQ

namespace hwy {
namespace {

#ifdef GTEST_HAS_DEATH_TEST

TEST(AbortDeathTest, AbortDefault) {
  std::string expected = std::string("Abort at ") + __FILE__ + ":" +
                         std::to_string(__LINE__ + 1) + ": Test Abort";
  ASSERT_DEATH(HWY_ABORT("Test %s", "Abort"), expected);
}
#endif  // GTEST_HAS_DEATH_TEST

}  // namespace
}  // namespace hwy
