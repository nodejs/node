#include "node_builtins.h"
#include "node_threadsafe_cow-inl.h"

#include "gtest/gtest.h"
#include "node_test_fixture.h"

#include <string>

using node::ThreadsafeCopyOnWrite;
using node::builtins::BuiltinLoader;
using node::builtins::BuiltinSourceMap;

class PerProcessTest : public ::testing::Test {
 protected:
  static const BuiltinSourceMap get_sources_for_test() {
    return *BuiltinLoader().source_.read();
  }
};

namespace {

struct CopyConstructOnly {
  explicit CopyConstructOnly(int value) : value(value) {}
  CopyConstructOnly() = delete;
  CopyConstructOnly(const CopyConstructOnly&) = default;
  CopyConstructOnly& operator=(const CopyConstructOnly&) = delete;

  int value;
};

TEST(ThreadsafeCopyOnWriteTest, CloneCopyConstructsData) {
  const ThreadsafeCopyOnWrite<CopyConstructOnly> original(42);
  auto copy = original;

  EXPECT_EQ(copy.write()->value, 42);
}

TEST_F(PerProcessTest, EmbeddedSources) {
  const auto& sources = PerProcessTest::get_sources_for_test();
  ASSERT_TRUE(std::any_of(sources.cbegin(), sources.cend(), [](auto p) {
    return p.second.source.is_one_byte();
  })) << "BuiltinLoader::source_ should have some 8bit items";

  ASSERT_TRUE(std::any_of(sources.cbegin(), sources.cend(), [](auto p) {
    return !p.second.source.is_one_byte();
  })) << "BuiltinLoader::source_ should have some 16bit items";
}

}  // end namespace
