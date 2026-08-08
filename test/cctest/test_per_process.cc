#include "node_builtins.h"
#include "node_threadsafe_cow-inl.h"

#include "gtest/gtest.h"
#include "node_test_fixture.h"

#include <string>

using node::CopyOnWrite;
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

TEST(CopyOnWriteTest, CopiesFromNonConstLvalue) {
  CopyOnWrite<int> original(42);
  CopyOnWrite<int> copy(original);

  *copy.write() = 43;

  EXPECT_EQ(*original, 42);
  EXPECT_EQ(*copy, 43);
}

TEST(ThreadsafeCopyOnWriteTest, CopiesFromNonConstLvalue) {
  ThreadsafeCopyOnWrite<int> original(42);
  ThreadsafeCopyOnWrite<int> copy(original);

  *copy.write() = 43;

  EXPECT_EQ(*original.read(), 42);
  EXPECT_EQ(*copy.read(), 43);
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
