#include "node_builtins.h"
#include "node_threadsafe_cow-inl.h"

#include "gtest/gtest.h"
#include "node_test_fixture.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using node::builtins::BuiltinCodeCacheData;
using node::builtins::BuiltinLoader;
using node::builtins::BuiltinSourceMap;
using node::builtins::CodeCacheInfo;

class PerProcessTest : public ::testing::Test {
 protected:
  static const BuiltinSourceMap get_sources_for_test() {
    return *BuiltinLoader().source_.read();
  }

  // id -> first byte of the cached data, after feeding `batches` in order.
  static std::vector<std::pair<std::string, uint8_t>> RefreshCodeCacheWith(
      const std::vector<std::vector<CodeCacheInfo>>& batches,
      bool seed_from_process = false) {
    BuiltinLoader loader;
    if (seed_from_process) loader.SeedFromProcessCodeCache();
    for (const auto& batch : batches) loader.RefreshCodeCache(batch);
    std::vector<std::pair<std::string, uint8_t>> out;
    node::RwLock::ScopedReadLock lock(loader.code_cache_->mutex);
    EXPECT_TRUE(loader.code_cache_->has_code_cache);
    for (const auto& [id, data] : loader.code_cache_->map) {
      out.emplace_back(id, data.data[0]);
    }
    std::sort(out.begin(), out.end());
    return out;
  }
};

CodeCacheInfo MakeCodeCacheInfo(const std::string& id, uint8_t marker) {
  auto* bytes = new uint8_t[4]{marker, marker, marker, marker};
  auto cached_data = std::make_shared<v8::ScriptCompiler::CachedData>(
      bytes, 4, v8::ScriptCompiler::CachedData::BufferOwned);
  return CodeCacheInfo{id, BuiltinCodeCacheData(std::move(cached_data))};
}

namespace {

TEST_F(PerProcessTest, EmbeddedSources) {
  const auto& sources = PerProcessTest::get_sources_for_test();
  ASSERT_TRUE(std::any_of(sources.cbegin(), sources.cend(), [](auto p) {
    return p.second.source.is_one_byte();
  })) << "BuiltinLoader::source_ should have some 8bit items";

  ASSERT_TRUE(std::any_of(sources.cbegin(), sources.cend(), [](auto p) {
    return !p.second.source.is_one_byte();
  })) << "BuiltinLoader::source_ should have some 16bit items";
}

// RefreshCodeCache() merges: it can be fed the snapshot's code cache and then
// an embedder's, and the entry supplied last wins for a shared id.
TEST_F(PerProcessTest, RefreshCodeCacheMerges) {
  const auto merged = PerProcessTest::RefreshCodeCacheWith({
      {MakeCodeCacheInfo("internal/a", 1), MakeCodeCacheInfo("internal/b", 1)},
      {MakeCodeCacheInfo("internal/b", 2), MakeCodeCacheInfo("embedder/c", 2)},
  });
  const std::vector<std::pair<std::string, uint8_t>> expected = {
      {"embedder/c", 2}, {"internal/a", 1}, {"internal/b", 2}};
  EXPECT_EQ(merged, expected);

  // A single call still behaves as before.
  const auto single = PerProcessTest::RefreshCodeCacheWith(
      {{MakeCodeCacheInfo("internal/a", 7)}});
  ASSERT_EQ(single.size(), 1u);
  EXPECT_EQ(single[0].second, 7);
}

// SeedFromProcessCodeCache() starts a loader from SetProcessCodeCache()'s
// entries, and a later RefreshCodeCache() (e.g. from a snapshot) merges on top.
TEST_F(PerProcessTest, ProcessCodeCacheSeedsLoaders) {
  BuiltinLoader::SetProcessCodeCache(
      {MakeCodeCacheInfo("internal/a", 3), MakeCodeCacheInfo("embedder/x", 3)});
  EXPECT_EQ(PerProcessTest::RefreshCodeCacheWith({{}}).size(), 0u);
  const auto seeded = PerProcessTest::RefreshCodeCacheWith(
      {{MakeCodeCacheInfo("internal/a", 4)}}, true);
  const std::vector<std::pair<std::string, uint8_t>> expected = {
      {"embedder/x", 3}, {"internal/a", 4}};
  EXPECT_EQ(seeded, expected);
  BuiltinLoader::SetProcessCodeCache({});
  EXPECT_TRUE(PerProcessTest::RefreshCodeCacheWith({{}}, true).empty());
}

}  // end namespace
