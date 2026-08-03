#include <string>
#include <vector>
#include "gtest/gtest.h"
#include "lru_cache-inl.h"
#include "node_internals.h"

// Test basic Put and Get operations
TEST(LRUCache, PutAndGet) {
  LRUCache<int, std::string> cache(2);
  cache.Put(1, "one");
  cache.Put(2, "two");

  EXPECT_TRUE(cache.Exists(1));
  EXPECT_EQ(cache.Get(1), "one");

  EXPECT_TRUE(cache.Exists(2));
  EXPECT_EQ(cache.Get(2), "two");

  EXPECT_FALSE(cache.Exists(3));
}

// Test that Putting an existing key updates its value and moves it to the front
TEST(LRUCache, PutUpdatesExisting) {
  LRUCache<int, std::string> cache(2);
  cache.Put(1, "one");
  cache.Put(2, "two");
  cache.Put(1, "updated one");

  EXPECT_EQ(cache.Size(), 2u);
  EXPECT_EQ(cache.Get(1), "updated one");

  // Now, if we add another element, key 2 should be evicted, not key 1
  cache.Put(3, "three");
  EXPECT_FALSE(cache.Exists(2));
  EXPECT_TRUE(cache.Exists(1));
  EXPECT_TRUE(cache.Exists(3));
}

// Test the eviction of the least recently used item
TEST(LRUCache, Eviction) {
  LRUCache<int, int> cache(3);
  cache.Put(1, 10);
  cache.Put(2, 20);
  cache.Put(3, 30);

  // At this point, the order of use is 3, 2, 1
  cache.Put(4, 40);  // This should evict key 1

  EXPECT_EQ(cache.Size(), 3u);
  EXPECT_FALSE(cache.Exists(1));
  EXPECT_TRUE(cache.Exists(2));
  EXPECT_TRUE(cache.Exists(3));
  EXPECT_TRUE(cache.Exists(4));
}

// Test that Get() moves an item to the front (most recently used)
TEST(LRUCache, GetMovesToFront) {
  LRUCache<char, int> cache(2);
  cache.Put('a', 1);
  cache.Put('b', 2);

  // Access 'a', making it the most recently used
  cache.Get('a');

  // Add 'c', which should evict 'b'
  cache.Put('c', 3);

  EXPECT_EQ(cache.Size(), 2u);
  EXPECT_TRUE(cache.Exists('a'));
  EXPECT_FALSE(cache.Exists('b'));
  EXPECT_TRUE(cache.Exists('c'));
}

// Test the Erase() method
TEST(LRUCache, Erase) {
  LRUCache<int, std::string> cache(2);
  cache.Put(1, "one");
  cache.Put(2, "two");

  cache.Erase(1);

  EXPECT_EQ(cache.Size(), 1u);
  EXPECT_FALSE(cache.Exists(1));
  EXPECT_TRUE(cache.Exists(2));

  // Erasing a non-existent key should not fail
  cache.Erase(99);
  EXPECT_EQ(cache.Size(), 1u);
}

// Test the Exists() method
TEST(LRUCache, Exists) {
  LRUCache<int, int> cache(1);
  cache.Put(1, 100);

  EXPECT_TRUE(cache.Exists(1));
  EXPECT_FALSE(cache.Exists(2));
}

// Test the Size() method
TEST(LRUCache, Size) {
  LRUCache<int, int> cache(5);
  EXPECT_EQ(cache.Size(), 0u);

  cache.Put(1, 1);
  EXPECT_EQ(cache.Size(), 1u);
  cache.Put(2, 2);
  EXPECT_EQ(cache.Size(), 2u);

  cache.Put(1, 11);  // Update
  EXPECT_EQ(cache.Size(), 2u);

  cache.Erase(2);
  EXPECT_EQ(cache.Size(), 1u);
}

// Test with a max_Size of 0
TEST(LRUCache, ZeroSizeCache) {
  LRUCache<int, int> cache(0);
  cache.Put(1, 1);
  EXPECT_FALSE(cache.Exists(1));
  EXPECT_EQ(cache.Size(), 0u);
}

// Test with a max_Size of 1
TEST(LRUCache, OneSizeCache) {
  LRUCache<int, int> cache(1);
  cache.Put(1, 1);
  EXPECT_TRUE(cache.Exists(1));
  EXPECT_EQ(cache.Size(), 1u);

  cache.Put(2, 2);
  EXPECT_FALSE(cache.Exists(1));
  EXPECT_TRUE(cache.Exists(2));
  EXPECT_EQ(cache.Size(), 1u);
}

// Test with complex key and value types
TEST(LRUCache, ComplexTypes) {
  LRUCache<std::string, std::vector<int>> cache(2);
  std::vector<int> vec1 = {1, 2, 3};
  std::vector<int> vec2 = {4, 5, 6};
  std::vector<int> vec3 = {7, 8, 9};

  cache.Put("vec1", vec1);
  cache.Put("vec2", vec2);

  EXPECT_EQ(cache.Get("vec1"), vec1);
  EXPECT_EQ(cache.Get("vec2"), vec2);

  cache.Put("vec3", vec3);
  EXPECT_FALSE(cache.Exists("vec1"));
  EXPECT_TRUE(cache.Exists("vec2"));
  EXPECT_TRUE(cache.Exists("vec3"));
}
