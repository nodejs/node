#include <string>
#include <vector>
#include "gtest/gtest.h"
#include "lru_cache-inl.h"
#include "node_internals.h"

// Test basic put and get operations
TEST(LruCache, PutAndGet) {
  LruCache<int, std::string> cache(2);
  cache.put(1, "one");
  cache.put(2, "two");

  EXPECT_TRUE(cache.exists(1));
  EXPECT_EQ(cache.get(1), "one");

  EXPECT_TRUE(cache.exists(2));
  EXPECT_EQ(cache.get(2), "two");

  EXPECT_FALSE(cache.exists(3));
}

// Test that putting an existing key updates its value and moves it to the front
TEST(LruCache, PutUpdatesExisting) {
  LruCache<int, std::string> cache(2);
  cache.put(1, "one");
  cache.put(2, "two");
  cache.put(1, "updated one");

  EXPECT_EQ(cache.size(), 2u);
  EXPECT_EQ(cache.get(1), "updated one");

  // Now, if we add another element, key 2 should be evicted, not key 1
  cache.put(3, "three");
  EXPECT_FALSE(cache.exists(2));
  EXPECT_TRUE(cache.exists(1));
  EXPECT_TRUE(cache.exists(3));
}

// Test the eviction of the least recently used item
TEST(LruCache, Eviction) {
  LruCache<int, int> cache(3);
  cache.put(1, 10);
  cache.put(2, 20);
  cache.put(3, 30);

  // At this point, the order of use is 3, 2, 1
  cache.put(4, 40);  // This should evict key 1

  EXPECT_EQ(cache.size(), 3u);
  EXPECT_FALSE(cache.exists(1));
  EXPECT_TRUE(cache.exists(2));
  EXPECT_TRUE(cache.exists(3));
  EXPECT_TRUE(cache.exists(4));
}

// Test that get() moves an item to the front (most recently used)
TEST(LruCache, GetMovesToFront) {
  LruCache<char, int> cache(2);
  cache.put('a', 1);
  cache.put('b', 2);

  // Access 'a', making it the most recently used
  cache.get('a');

  // Add 'c', which should evict 'b'
  cache.put('c', 3);

  EXPECT_EQ(cache.size(), 2u);
  EXPECT_TRUE(cache.exists('a'));
  EXPECT_FALSE(cache.exists('b'));
  EXPECT_TRUE(cache.exists('c'));
}

// Test the erase() method
TEST(LruCache, Erase) {
  LruCache<int, std::string> cache(2);
  cache.put(1, "one");
  cache.put(2, "two");

  cache.erase(1);

  EXPECT_EQ(cache.size(), 1u);
  EXPECT_FALSE(cache.exists(1));
  EXPECT_TRUE(cache.exists(2));

  // Erasing a non-existent key should not fail
  cache.erase(99);
  EXPECT_EQ(cache.size(), 1u);
}

// Test the exists() method
TEST(LruCache, Exists) {
  LruCache<int, int> cache(1);
  cache.put(1, 100);

  EXPECT_TRUE(cache.exists(1));
  EXPECT_FALSE(cache.exists(2));
}

// Test the size() method
TEST(LruCache, Size) {
  LruCache<int, int> cache(5);
  EXPECT_EQ(cache.size(), 0u);

  cache.put(1, 1);
  EXPECT_EQ(cache.size(), 1u);
  cache.put(2, 2);
  EXPECT_EQ(cache.size(), 2u);

  cache.put(1, 11);  // Update
  EXPECT_EQ(cache.size(), 2u);

  cache.erase(2);
  EXPECT_EQ(cache.size(), 1u);
}

// Test with a max_size of 0
TEST(LruCache, ZeroSizeCache) {
  LruCache<int, int> cache(0);
  cache.put(1, 1);
  EXPECT_FALSE(cache.exists(1));
  EXPECT_EQ(cache.size(), 0u);
}

// Test with a max_size of 1
TEST(LruCache, OneSizeCache) {
  LruCache<int, int> cache(1);
  cache.put(1, 1);
  EXPECT_TRUE(cache.exists(1));
  EXPECT_EQ(cache.size(), 1u);

  cache.put(2, 2);
  EXPECT_FALSE(cache.exists(1));
  EXPECT_TRUE(cache.exists(2));
  EXPECT_EQ(cache.size(), 1u);
}

// Test with complex key and value types
TEST(LruCache, ComplexTypes) {
  LruCache<std::string, std::vector<int>> cache(2);
  std::vector<int> vec1 = {1, 2, 3};
  std::vector<int> vec2 = {4, 5, 6};
  std::vector<int> vec3 = {7, 8, 9};

  cache.put("vec1", vec1);
  cache.put("vec2", vec2);

  EXPECT_EQ(cache.get("vec1"), vec1);
  EXPECT_EQ(cache.get("vec2"), vec2);

  cache.put("vec3", vec3);
  EXPECT_FALSE(cache.exists("vec1"));
  EXPECT_TRUE(cache.exists("vec2"));
  EXPECT_TRUE(cache.exists("vec3"));
}
