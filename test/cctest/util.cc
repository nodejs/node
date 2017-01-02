#include "util.h"
#include "util-inl.h"

#include "gtest/gtest.h"

TEST(UtilTest, ListHead) {
  struct Item { node::ListNode<Item> node_; };
  typedef node::ListHead<Item, &Item::node_> List;

  List list;
  EXPECT_TRUE(list.IsEmpty());

  Item one;
  EXPECT_TRUE(one.node_.IsEmpty());

  list.PushBack(&one);
  EXPECT_FALSE(list.IsEmpty());
  EXPECT_FALSE(one.node_.IsEmpty());

  {
    List::Iterator it = list.begin();
    EXPECT_NE(list.end(), it);
    EXPECT_EQ(&one, *it);
    ++it;
    EXPECT_FALSE(it != list.end());  // Iterator only implements != operator.
  }

  Item two;
  list.PushBack(&two);

  {
    List::Iterator it = list.begin();
    EXPECT_NE(list.end(), it);
    EXPECT_EQ(&one, *it);
    ++it;
    EXPECT_NE(list.end(), it);
    EXPECT_EQ(&two, *it);
    ++it;
    EXPECT_FALSE(it != list.end());  // Iterator only implements != operator.
  }

  EXPECT_EQ(&one, list.PopFront());
  EXPECT_TRUE(one.node_.IsEmpty());
  EXPECT_FALSE(list.IsEmpty());

  {
    List::Iterator it = list.begin();
    EXPECT_NE(list.end(), it);
    EXPECT_EQ(&two, *it);
    ++it;
    EXPECT_FALSE(it != list.end());  // Iterator only implements != operator.
  }

  EXPECT_EQ(&two, list.PopFront());
  EXPECT_TRUE(two.node_.IsEmpty());
  EXPECT_TRUE(list.IsEmpty());
  EXPECT_FALSE(list.begin() != list.end());
}

TEST(UtilTest, StringEqualNoCase) {
  using node::StringEqualNoCase;
  EXPECT_FALSE(StringEqualNoCase("a", "b"));
  EXPECT_TRUE(StringEqualNoCase("", ""));
  EXPECT_TRUE(StringEqualNoCase("equal", "equal"));
  EXPECT_TRUE(StringEqualNoCase("equal", "EQUAL"));
  EXPECT_TRUE(StringEqualNoCase("EQUAL", "EQUAL"));
  EXPECT_FALSE(StringEqualNoCase("equal", "equals"));
  EXPECT_FALSE(StringEqualNoCase("equals", "equal"));
}

TEST(UtilTest, StringEqualNoCaseN) {
  using node::StringEqualNoCaseN;
  EXPECT_FALSE(StringEqualNoCaseN("a", "b", strlen("a")));
  EXPECT_TRUE(StringEqualNoCaseN("", "", strlen("")));
  EXPECT_TRUE(StringEqualNoCaseN("equal", "equal", strlen("equal")));
  EXPECT_TRUE(StringEqualNoCaseN("equal", "EQUAL", strlen("equal")));
  EXPECT_TRUE(StringEqualNoCaseN("EQUAL", "EQUAL", strlen("equal")));
  EXPECT_TRUE(StringEqualNoCaseN("equal", "equals", strlen("equal")));
  EXPECT_FALSE(StringEqualNoCaseN("equal", "equals", strlen("equals")));
  EXPECT_TRUE(StringEqualNoCaseN("equals", "equal", strlen("equal")));
  EXPECT_FALSE(StringEqualNoCaseN("equals", "equal", strlen("equals")));
  EXPECT_TRUE(StringEqualNoCaseN("abc\0abc", "abc\0efg", strlen("abcdefgh")));
  EXPECT_FALSE(StringEqualNoCaseN("abc\0abc", "abcd\0efg", strlen("abcdefgh")));
}

TEST(UtilTest, ToLower) {
  using node::ToLower;
  EXPECT_EQ('0', ToLower('0'));
  EXPECT_EQ('a', ToLower('a'));
  EXPECT_EQ('a', ToLower('A'));
}

namespace node {
  void LowMemoryNotification() {}
}

#define TEST_AND_FREE(expression)                                             \
  do {                                                                        \
    auto pointer = expression;                                                \
    EXPECT_NE(nullptr, pointer);                                              \
    free(pointer);                                                            \
  } while (0)

TEST(UtilTest, Malloc) {
  using node::Malloc;
  TEST_AND_FREE(Malloc<char>(0));
  TEST_AND_FREE(Malloc<char>(1));
  TEST_AND_FREE(Malloc(0));
  TEST_AND_FREE(Malloc(1));
}

TEST(UtilTest, Calloc) {
  using node::Calloc;
  TEST_AND_FREE(Calloc<char>(0));
  TEST_AND_FREE(Calloc<char>(1));
  TEST_AND_FREE(Calloc(0));
  TEST_AND_FREE(Calloc(1));
}

TEST(UtilTest, UncheckedMalloc) {
  using node::UncheckedMalloc;
  TEST_AND_FREE(UncheckedMalloc<char>(0));
  TEST_AND_FREE(UncheckedMalloc<char>(1));
  TEST_AND_FREE(UncheckedMalloc(0));
  TEST_AND_FREE(UncheckedMalloc(1));
}

TEST(UtilTest, UncheckedCalloc) {
  using node::UncheckedCalloc;
  TEST_AND_FREE(UncheckedCalloc<char>(0));
  TEST_AND_FREE(UncheckedCalloc<char>(1));
  TEST_AND_FREE(UncheckedCalloc(0));
  TEST_AND_FREE(UncheckedCalloc(1));
}
