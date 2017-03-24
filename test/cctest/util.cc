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

TEST(UtilTest, StringToUint64) {
  using node::NumberBase;
  using node::StringToUint64;
  EXPECT_TRUE(StringToUint64("").IsNothing());
  EXPECT_TRUE(StringToUint64("", 1).IsNothing());
  EXPECT_TRUE(StringToUint64("+1").IsNothing());
  EXPECT_TRUE(StringToUint64("-1").IsNothing());
  EXPECT_TRUE(StringToUint64("bad").IsNothing());
  EXPECT_TRUE(StringToUint64("8", NumberBase::kOctal).IsNothing());
  EXPECT_TRUE(StringToUint64("18", NumberBase::kOctal).IsNothing());
  EXPECT_TRUE(StringToUint64("A", NumberBase::kDecimal).IsNothing());
  EXPECT_TRUE(StringToUint64("2A", NumberBase::kDecimal).IsNothing());
  EXPECT_TRUE(StringToUint64("G", NumberBase::kHexadecimal).IsNothing());
  EXPECT_TRUE(StringToUint64("3G", NumberBase::kHexadecimal).IsNothing());
  EXPECT_EQ(0ULL, StringToUint64("0").FromMaybe(-1));
  EXPECT_EQ(0ULL, StringToUint64("0x").FromMaybe(-1));
  EXPECT_EQ(0ULL, StringToUint64("0X").FromMaybe(-1));
  EXPECT_EQ(42ULL, StringToUint64("42").FromMaybe(-1));
  EXPECT_EQ(493ULL, StringToUint64("0755").FromMaybe(-1));
  EXPECT_EQ(2989ULL, StringToUint64("0xbad").FromMaybe(-1));
  EXPECT_EQ(2989ULL, StringToUint64("0XBAD").FromMaybe(-1));
  EXPECT_EQ(2989ULL,
            StringToUint64("BAD", NumberBase::kHexadecimal).FromMaybe(-1));
  static const uint64_t kMaxUint64(-1);
  EXPECT_EQ(kMaxUint64,
            StringToUint64("01777777777777777777777").FromMaybe(0));
  EXPECT_EQ(kMaxUint64,
            StringToUint64("18446744073709551615").FromMaybe(0));
  EXPECT_EQ(kMaxUint64,
            StringToUint64("0xFFFFFFFFFFFFFFFF").FromMaybe(0));
}

TEST(UtilTest, StringToUint64InRange) {
  using node::NumberBase;
  using node::StringToUint64InRange;
  EXPECT_TRUE(StringToUint64InRange("0", 1, 42).IsNothing());
  EXPECT_TRUE(StringToUint64InRange("43", 7, 42).IsNothing());
  EXPECT_EQ(1ULL, StringToUint64InRange("1", 1, 42).FromMaybe(-1));
  EXPECT_EQ(42ULL, StringToUint64InRange("42", 1, 42).FromMaybe(-1));
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

template <typename T>
static void MaybeStackBufferBasic() {
  using node::MaybeStackBuffer;

  MaybeStackBuffer<T> buf;
  size_t old_length;
  size_t old_capacity;

  /* Default constructor */
  EXPECT_EQ(0U, buf.length());
  EXPECT_FALSE(buf.IsAllocated());
  EXPECT_GT(buf.capacity(), buf.length());

  /* SetLength() expansion */
  buf.SetLength(buf.capacity());
  EXPECT_EQ(buf.capacity(), buf.length());
  EXPECT_FALSE(buf.IsAllocated());

  /* Means of accessing raw buffer */
  EXPECT_EQ(buf.out(), *buf);
  EXPECT_EQ(&buf[0], *buf);

  /* Basic I/O */
  for (size_t i = 0; i < buf.length(); i++)
    buf[i] = static_cast<T>(i);
  for (size_t i = 0; i < buf.length(); i++)
    EXPECT_EQ(static_cast<T>(i), buf[i]);

  /* SetLengthAndZeroTerminate() */
  buf.SetLengthAndZeroTerminate(buf.capacity() - 1);
  EXPECT_EQ(buf.capacity() - 1, buf.length());
  for (size_t i = 0; i < buf.length(); i++)
    EXPECT_EQ(static_cast<T>(i), buf[i]);
  buf.SetLength(buf.capacity());
  EXPECT_EQ(0, buf[buf.length() - 1]);

  /* Initial Realloc */
  old_length = buf.length() - 1;
  old_capacity = buf.capacity();
  buf.AllocateSufficientStorage(buf.capacity() * 2);
  EXPECT_EQ(buf.capacity(), buf.length());
  EXPECT_TRUE(buf.IsAllocated());
  for (size_t i = 0; i < old_length; i++)
    EXPECT_EQ(static_cast<T>(i), buf[i]);
  EXPECT_EQ(0, buf[old_length]);

  /* SetLength() reduction and expansion */
  for (size_t i = 0; i < buf.length(); i++)
    buf[i] = static_cast<T>(i);
  buf.SetLength(10);
  for (size_t i = 0; i < buf.length(); i++)
    EXPECT_EQ(static_cast<T>(i), buf[i]);
  buf.SetLength(buf.capacity());
  for (size_t i = 0; i < buf.length(); i++)
    EXPECT_EQ(static_cast<T>(i), buf[i]);

  /* Subsequent Realloc */
  old_length = buf.length();
  old_capacity = buf.capacity();
  buf.AllocateSufficientStorage(old_capacity * 1.5);
  EXPECT_EQ(buf.capacity(), buf.length());
  EXPECT_EQ(static_cast<size_t>(old_capacity * 1.5), buf.length());
  EXPECT_TRUE(buf.IsAllocated());
  for (size_t i = 0; i < old_length; i++)
    EXPECT_EQ(static_cast<T>(i), buf[i]);

  /* Basic I/O on Realloc'd buffer */
  for (size_t i = 0; i < buf.length(); i++)
    buf[i] = static_cast<T>(i);
  for (size_t i = 0; i < buf.length(); i++)
    EXPECT_EQ(static_cast<T>(i), buf[i]);

  /* Release() */
  T* rawbuf = buf.out();
  buf.Release();
  EXPECT_EQ(0U, buf.length());
  EXPECT_FALSE(buf.IsAllocated());
  EXPECT_GT(buf.capacity(), buf.length());
  free(rawbuf);
}

TEST(UtilTest, MaybeStackBuffer) {
  using node::MaybeStackBuffer;

  MaybeStackBufferBasic<uint8_t>();
  MaybeStackBufferBasic<uint16_t>();

  // Constructor with size parameter
  {
    MaybeStackBuffer<unsigned char> buf(100);
    EXPECT_EQ(100U, buf.length());
    EXPECT_FALSE(buf.IsAllocated());
    EXPECT_GT(buf.capacity(), buf.length());
    buf.SetLength(buf.capacity());
    EXPECT_EQ(buf.capacity(), buf.length());
    EXPECT_FALSE(buf.IsAllocated());
    for (size_t i = 0; i < buf.length(); i++)
      buf[i] = static_cast<unsigned char>(i);
    for (size_t i = 0; i < buf.length(); i++)
      EXPECT_EQ(static_cast<unsigned char>(i), buf[i]);

    MaybeStackBuffer<unsigned char> bigbuf(10000);
    EXPECT_EQ(10000U, bigbuf.length());
    EXPECT_TRUE(bigbuf.IsAllocated());
    EXPECT_EQ(bigbuf.length(), bigbuf.capacity());
    for (size_t i = 0; i < bigbuf.length(); i++)
      bigbuf[i] = static_cast<unsigned char>(i);
    for (size_t i = 0; i < bigbuf.length(); i++)
      EXPECT_EQ(static_cast<unsigned char>(i), bigbuf[i]);
  }

  // Invalidated buffer
  {
    MaybeStackBuffer<char> buf;
    buf.Invalidate();
    EXPECT_TRUE(buf.IsInvalidated());
    EXPECT_FALSE(buf.IsAllocated());
    EXPECT_EQ(0U, buf.length());
    EXPECT_EQ(0U, buf.capacity());
    buf.Invalidate();
    EXPECT_TRUE(buf.IsInvalidated());
  }
}
