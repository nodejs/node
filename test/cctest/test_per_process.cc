#include "node_native_module.h"

#include "gtest/gtest.h"
#include "node_test_fixture.h"

#include <string>


using node::native_module::NativeModuleLoader;
using node::native_module::NativeModuleRecordMap;

class PerProcessTest : public ::testing::Test {
 protected:
  static const NativeModuleRecordMap get_sources_for_test() {
    return NativeModuleLoader::instance_.source_;
  }
};

namespace {

TEST_F(PerProcessTest, EmbeddedSources) {
  const auto& sources = PerProcessTest::get_sources_for_test();
  ASSERT_TRUE(
    std::any_of(sources.cbegin(), sources.cend(),
                [](auto p){ return p.second.is_one_byte(); }))
      << "NativeModuleLoader::source_ should have some 8bit items";

  ASSERT_TRUE(
    std::any_of(sources.cbegin(), sources.cend(),
                [](auto p){ return !p.second.is_one_byte(); }))
      << "NativeModuleLoader::source_ should have some 16bit items";
}

}  // end namespace
