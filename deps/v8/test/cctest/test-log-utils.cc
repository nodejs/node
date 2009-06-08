// Copyright 2006-2009 the V8 project authors. All rights reserved.
//
// Tests of logging utilities from log-utils.h

#ifdef ENABLE_LOGGING_AND_PROFILING

#include "v8.h"

#include "log-utils.h"
#include "cctest.h"

using v8::internal::EmbeddedVector;
using v8::internal::LogDynamicBuffer;
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
             "#   Vectors lengths differ: %d expected, %d found",
             expected_source, value_source,
             expected.length(), value.length());
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
  CHECK_EQ(v8::internal::CStrVector(seal), seal_buf.SubVector(0, seal_size));
  // Verify that there's no data beyond the seal.
  CHECK_EQ(0, ReadData(&dynabuf, 100 + seal_size, &buf));
}

#endif  // ENABLE_LOGGING_AND_PROFILING
