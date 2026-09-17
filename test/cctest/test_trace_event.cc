#include "gtest/gtest.h"

#ifndef V8_USE_PERFETTO

#include <cstdint>
#include <cstring>
#include <type_traits>

#include "tracing/trace_event.h"

namespace {

template <typename T>
T DecodeTraceValue(uint64_t value) {
  static_assert(std::is_trivially_copyable_v<T>);
  T result{};
  std::memcpy(&result, &value, sizeof(result));
  return result;
}

enum class TestEnum : uint16_t {
  kValue = 42,
};

TEST(TraceEvent, SetIntegralTraceValue) {
  unsigned char type;
  uint64_t value;

  node::tracing::SetTraceValue(false, &type, &value);
  EXPECT_EQ(TRACE_VALUE_TYPE_BOOL, type);
  EXPECT_EQ(uint64_t{0}, value);

  node::tracing::SetTraceValue(-42, &type, &value);
  EXPECT_EQ(TRACE_VALUE_TYPE_INT, type);
  EXPECT_EQ(static_cast<uint64_t>(-42), value);

  node::tracing::SetTraceValue(TestEnum::kValue, &type, &value);
  EXPECT_EQ(TRACE_VALUE_TYPE_UINT, type);
  EXPECT_EQ(uint64_t{42}, value);
}

TEST(TraceEvent, SetNonIntegralTraceValue) {
  unsigned char type;
  uint64_t value;

  const double number = 1.25;
  node::tracing::SetTraceValue(number, &type, &value);
  EXPECT_EQ(TRACE_VALUE_TYPE_DOUBLE, type);
  EXPECT_EQ(number, DecodeTraceValue<double>(value));

  const void* pointer = &value;
  node::tracing::SetTraceValue(pointer, &type, &value);
  EXPECT_EQ(TRACE_VALUE_TYPE_POINTER, type);
  EXPECT_EQ(pointer, DecodeTraceValue<const void*>(value));

  const char* string = "trace value";
  node::tracing::SetTraceValue(string, &type, &value);
  EXPECT_EQ(TRACE_VALUE_TYPE_STRING, type);
  EXPECT_EQ(string, DecodeTraceValue<const char*>(value));

  const node::tracing::TraceStringWithCopy copied_string(string);
  node::tracing::SetTraceValue(copied_string, &type, &value);
  EXPECT_EQ(TRACE_VALUE_TYPE_COPY_STRING, type);
  EXPECT_EQ(string, DecodeTraceValue<const char*>(value));
}

}  // namespace

#endif  // V8_USE_PERFETTO
