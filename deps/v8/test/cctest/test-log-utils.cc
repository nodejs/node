// Copyright 2006-2009 the V8 project authors. All rights reserved.
//
// Tests of logging utilities from log-utils.h

#ifdef ENABLE_LOGGING_AND_PROFILING

#include "v8.h"

#include "log-utils.h"
#include "cctest.h"

using v8::internal::CStrVector;
using v8::internal::EmbeddedVector;
using v8::internal::LogDynamicBuffer;
using v8::internal::LogRecordCompressor;
using v8::internal::MutableCStrVector;
using v8::internal::ScopedVector;
using v8::internal::Vector;

// Fills 'ref_buffer' with test data: a sequence of two-digit
// hex numbers: '0001020304...'. Then writes 'ref_buffer' contents to 'dynabuf'.
static void WriteData(LogDynamicBuffer* dynabuf, Vector<char>* ref_buffer) {
  static const char kHex[] = "0123456789ABCDEF";
  CHECK_GT(ref_buffer->length(), 0);
  CHECK_GT(513, ref_buffer->length());
  for (int i = 0, half_len = ref_buffer->length() >> 1; i < half_len; ++i) {
    (*ref_buffer)[i << 1] = kHex[i >> 4];
    (*ref_buffer)[(i << 1) + 1] = kHex[i & 15];
  }
  if (ref_buffer->length() & 1) {
    ref_buffer->last() = kHex[ref_buffer->length() >> 5];
  }
  CHECK_EQ(ref_buffer->length(),
           dynabuf->Write(ref_buffer->start(), ref_buffer->length()));
}


static int ReadData(
    LogDynamicBuffer* dynabuf, int start_pos, i::Vector<char>* buffer) {
  return dynabuf->Read(start_pos, buffer->start(), buffer->length());
}


// Helper function used by CHECK_EQ to compare Vectors. Templatized to
// accept both "char" and "const char" vector contents.
template <typename E, typename V>
static inline void CheckEqualsHelper(const char* file, int line,
                                     const char* expected_source,
                                     const Vector<E>& expected,
                                     const char* value_source,
                                     const Vector<V>& value) {
  if (expected.length() != value.length()) {
    V8_Fatal(file, line, "CHECK_EQ(%s, %s) failed\n"
             "#   Vectors lengths differ: %d expected, %d found\n"
             "#   Expected: %.*s\n"
             "#   Found: %.*s",
             expected_source, value_source,
             expected.length(), value.length(),
             expected.length(), expected.start(),
             value.length(), value.start());
  }
  if (strncmp(expected.start(), value.start(), expected.length()) != 0) {
    V8_Fatal(file, line, "CHECK_EQ(%s, %s) failed\n"
             "#   Vectors contents differ:\n"
             "#   Expected: %.*s\n"
             "#   Found: %.*s",
             expected_source, value_source,
             expected.length(), expected.start(),
             value.length(), value.start());
  }
}


TEST(DynaBufSingleBlock) {
  LogDynamicBuffer dynabuf(32, 32, "", 0);
  EmbeddedVector<char, 32> ref_buf;
  WriteData(&dynabuf, &ref_buf);
  EmbeddedVector<char, 32> buf;
  CHECK_EQ(32, dynabuf.Read(0, buf.start(), buf.length()));
  CHECK_EQ(32, ReadData(&dynabuf, 0, &buf));
  CHECK_EQ(ref_buf, buf);

  // Verify that we can't read and write past the end.
  CHECK_EQ(0, dynabuf.Read(32, buf.start(), buf.length()));
  CHECK_EQ(0, dynabuf.Write(buf.start(), buf.length()));
}


TEST(DynaBufCrossBlocks) {
  LogDynamicBuffer dynabuf(32, 128, "", 0);
  EmbeddedVector<char, 48> ref_buf;
  WriteData(&dynabuf, &ref_buf);
  CHECK_EQ(48, dynabuf.Write(ref_buf.start(), ref_buf.length()));
  // Verify that we can't write data when remaining buffer space isn't enough.
  CHECK_EQ(0, dynabuf.Write(ref_buf.start(), ref_buf.length()));
  EmbeddedVector<char, 48> buf;
  CHECK_EQ(48, ReadData(&dynabuf, 0, &buf));
  CHECK_EQ(ref_buf, buf);
  CHECK_EQ(48, ReadData(&dynabuf, 48, &buf));
  CHECK_EQ(ref_buf, buf);
  CHECK_EQ(0, ReadData(&dynabuf, 48 * 2, &buf));
}


TEST(DynaBufReadTruncation) {
  LogDynamicBuffer dynabuf(32, 128, "", 0);
  EmbeddedVector<char, 128> ref_buf;
  WriteData(&dynabuf, &ref_buf);
  EmbeddedVector<char, 128> buf;
  CHECK_EQ(128, ReadData(&dynabuf, 0, &buf));
  CHECK_EQ(ref_buf, buf);
  // Try to read near the end with a buffer larger than remaining data size.
  EmbeddedVector<char, 48> tail_buf;
  CHECK_EQ(32, ReadData(&dynabuf, 128 - 32, &tail_buf));
  CHECK_EQ(ref_buf.SubVector(128 - 32, 128), tail_buf.SubVector(0, 32));
}


TEST(DynaBufSealing) {
  const char* seal = "Sealed";
  const int seal_size = strlen(seal);
  LogDynamicBuffer dynabuf(32, 128, seal, seal_size);
  EmbeddedVector<char, 100> ref_buf;
  WriteData(&dynabuf, &ref_buf);
  // Try to write data that will not fit in the buffer.
  CHECK_EQ(0, dynabuf.Write(ref_buf.start(), 128 - 100 - seal_size + 1));
  // Now the buffer is sealed, writing of any amount of data is forbidden.
  CHECK_EQ(0, dynabuf.Write(ref_buf.start(), 1));
  EmbeddedVector<char, 100> buf;
  CHECK_EQ(100, ReadData(&dynabuf, 0, &buf));
  CHECK_EQ(ref_buf, buf);
  // Check the seal.
  EmbeddedVector<char, 50> seal_buf;
  CHECK_EQ(seal_size, ReadData(&dynabuf, 100, &seal_buf));
  CHECK_EQ(CStrVector(seal), seal_buf.SubVector(0, seal_size));
  // Verify that there's no data beyond the seal.
  CHECK_EQ(0, ReadData(&dynabuf, 100 + seal_size, &buf));
}


TEST(CompressorStore) {
  LogRecordCompressor comp(2);
  const Vector<const char> empty = CStrVector("");
  CHECK(comp.Store(empty));
  CHECK(!comp.Store(empty));
  CHECK(!comp.Store(empty));
  const Vector<const char> aaa = CStrVector("aaa");
  CHECK(comp.Store(aaa));
  CHECK(!comp.Store(aaa));
  CHECK(!comp.Store(aaa));
  CHECK(comp.Store(empty));
  CHECK(!comp.Store(empty));
  CHECK(!comp.Store(empty));
}


void CheckCompression(LogRecordCompressor* comp,
                      const Vector<const char>& after) {
  EmbeddedVector<char, 100> result;
  CHECK(comp->RetrievePreviousCompressed(&result));
  CHECK_EQ(after, result);
}


void CheckCompression(LogRecordCompressor* comp,
                      const char* after) {
  CheckCompression(comp, CStrVector(after));
}


TEST(CompressorNonCompressed) {
  LogRecordCompressor comp(0);
  CHECK(!comp.RetrievePreviousCompressed(NULL));
  const Vector<const char> empty = CStrVector("");
  CHECK(comp.Store(empty));
  CHECK(!comp.RetrievePreviousCompressed(NULL));
  const Vector<const char> a_x_20 = CStrVector("aaaaaaaaaaaaaaaaaaaa");
  CHECK(comp.Store(a_x_20));
  CheckCompression(&comp, empty);
  CheckCompression(&comp, empty);
  CHECK(comp.Store(empty));
  CheckCompression(&comp, a_x_20);
  CheckCompression(&comp, a_x_20);
}


TEST(CompressorSingleLine) {
  LogRecordCompressor comp(1);
  const Vector<const char> string_1 = CStrVector("eee,ddd,ccc,bbb,aaa");
  CHECK(comp.Store(string_1));
  const Vector<const char> string_2 = CStrVector("fff,ddd,ccc,bbb,aaa");
  CHECK(comp.Store(string_2));
  // string_1 hasn't been compressed.
  CheckCompression(&comp, string_1);
  CheckCompression(&comp, string_1);
  const Vector<const char> string_3 = CStrVector("hhh,ggg,ccc,bbb,aaa");
  CHECK(comp.Store(string_3));
  // string_2 compressed using string_1.
  CheckCompression(&comp, "fff#1:3");
  CheckCompression(&comp, "fff#1:3");
  CHECK(!comp.Store(string_3));
  // Expecting no changes.
  CheckCompression(&comp, "fff#1:3");
  CHECK(!comp.Store(string_3));
  // Expecting no changes.
  CheckCompression(&comp, "fff#1:3");
  const Vector<const char> string_4 = CStrVector("iii,hhh,ggg,ccc,bbb,aaa");
  CHECK(comp.Store(string_4));
  // string_3 compressed using string_2.
  CheckCompression(&comp, "hhh,ggg#1:7");
  const Vector<const char> string_5 = CStrVector("nnn,mmm,lll,kkk,jjj");
  CHECK(comp.Store(string_5));
  // string_4 compressed using string_3.
  CheckCompression(&comp, "iii,#1");
  const Vector<const char> string_6 = CStrVector("nnn,mmmmmm,lll,kkk,jjj");
  CHECK(comp.Store(string_6));
  // string_5 hasn't been compressed.
  CheckCompression(&comp, string_5);
  CHECK(comp.Store(string_5));
  // string_6 compressed using string_5.
  CheckCompression(&comp, "nnn,mmm#1:4");
  const Vector<const char> string_7 = CStrVector("nnnnnn,mmm,lll,kkk,jjj");
  CHECK(comp.Store(string_7));
  // string_5 compressed using string_6.
  CheckCompression(&comp, "nnn,#1:7");
  const Vector<const char> string_8 = CStrVector("xxn,mmm,lll,kkk,jjj");
  CHECK(comp.Store(string_8));
  // string_7 compressed using string_5.
  CheckCompression(&comp, "nnn#1");
  const Vector<const char> string_9 =
      CStrVector("aaaaaaaaaaaaa,bbbbbbbbbbbbbbbbb");
  CHECK(comp.Store(string_9));
  // string_8 compressed using string_7.
  CheckCompression(&comp, "xx#1:5");
  const Vector<const char> string_10 =
      CStrVector("aaaaaaaaaaaaa,cccccccbbbbbbbbbb");
  CHECK(comp.Store(string_10));
  // string_9 hasn't been compressed.
  CheckCompression(&comp, string_9);
  CHECK(comp.Store(string_1));
  // string_10 compressed using string_9.
  CheckCompression(&comp, "aaaaaaaaaaaaa,ccccccc#1:21");
}



TEST(CompressorMultiLines) {
  const int kWindowSize = 3;
  LogRecordCompressor comp(kWindowSize);
  const Vector<const char> string_1 = CStrVector("eee,ddd,ccc,bbb,aaa");
  CHECK(comp.Store(string_1));
  const Vector<const char> string_2 = CStrVector("iii,hhh,ggg,fff,aaa");
  CHECK(comp.Store(string_2));
  const Vector<const char> string_3 = CStrVector("mmm,lll,kkk,jjj,aaa");
  CHECK(comp.Store(string_3));
  const Vector<const char> string_4 = CStrVector("nnn,hhh,ggg,fff,aaa");
  CHECK(comp.Store(string_4));
  const Vector<const char> string_5 = CStrVector("ooo,lll,kkk,jjj,aaa");
  CHECK(comp.Store(string_5));
  // string_4 compressed using string_2.
  CheckCompression(&comp, "nnn#2:3");
  CHECK(comp.Store(string_1));
  // string_5 compressed using string_3.
  CheckCompression(&comp, "ooo#2:3");
  CHECK(comp.Store(string_4));
  // string_1 is out of buffer by now, so it shouldn't be compressed.
  CHECK_GE(3, kWindowSize);
  CheckCompression(&comp, string_1);
  CHECK(comp.Store(string_2));
  // string_4 compressed using itself.
  CheckCompression(&comp, "#3");
}


TEST(CompressorBestSelection) {
  LogRecordCompressor comp(3);
  const Vector<const char> string_1 = CStrVector("eee,ddd,ccc,bbb,aaa");
  CHECK(comp.Store(string_1));
  const Vector<const char> string_2 = CStrVector("ddd,ccc,bbb,aaa");
  CHECK(comp.Store(string_2));
  const Vector<const char> string_3 = CStrVector("fff,eee,ddd,ccc,bbb,aaa");
  CHECK(comp.Store(string_3));
  // string_2 compressed using string_1.
  CheckCompression(&comp, "#1:4");
  const Vector<const char> string_4 = CStrVector("nnn,hhh,ggg,fff,aaa");
  CHECK(comp.Store(string_4));
  // Compressing string_3 using string_1 gives a better compression than
  // using string_2.
  CheckCompression(&comp, "fff,#2");
}


TEST(CompressorCompressibility) {
  LogRecordCompressor comp(2);
  const Vector<const char> string_1 = CStrVector("eee,ddd,ccc,bbb,aaa");
  CHECK(comp.Store(string_1));
  const Vector<const char> string_2 = CStrVector("ccc,bbb,aaa");
  CHECK(comp.Store(string_2));
  const Vector<const char> string_3 = CStrVector("aaa");
  CHECK(comp.Store(string_3));
  // string_2 compressed using string_1.
  CheckCompression(&comp, "#1:8");
  const Vector<const char> string_4 = CStrVector("xxx");
  CHECK(comp.Store(string_4));
  // string_3 can't be compressed using string_2 --- too short.
  CheckCompression(&comp, string_3);
}

#endif  // ENABLE_LOGGING_AND_PROFILING
