#include "gtest/gtest.h"
#include "node.h"
#include "node_test_fixture.h"
#include "string_bytes.h"
#include "util-inl.h"

using node::MaybeStackBuffer;
using node::StringBytes;
using v8::HandleScope;
using v8::Local;
using v8::Maybe;
using v8::String;

class StringBytesTest : public EnvironmentTestFixture {};

// Data "Hello, ÆÊÎÖÿ"
static const char latin1_data[] = "Hello, \xC6\xCA\xCE\xD6\xFF";
static const char utf8_data[] = "Hello, ÆÊÎÖÿ";

TEST_F(StringBytesTest, WriteLatin1WithOneByteString) {
  const HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env_{handle_scope, argv};

  Local<String> one_byte_str =
      String::NewFromOneByte(isolate_,
                             reinterpret_cast<const uint8_t*>(latin1_data))
          .ToLocalChecked();

  Maybe<size_t> size_maybe =
      StringBytes::StorageSize(isolate_, one_byte_str, node::LATIN1);

  ASSERT_TRUE(size_maybe.IsJust());
  size_t size = size_maybe.FromJust();
  ASSERT_EQ(size, 12u);

  MaybeStackBuffer<char> buf;
  size_t written = StringBytes::Write(
      isolate_, buf.out(), buf.capacity(), one_byte_str, node::LATIN1);
  ASSERT_EQ(written, 12u);

  // Null-terminate the buffer and compare the contents.
  buf.SetLength(13);
  buf[12] = '\0';
  ASSERT_STREQ(latin1_data, buf.out());
}

TEST_F(StringBytesTest, WriteLatin1WithUtf8String) {
  const HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env_{handle_scope, argv};

  Local<String> utf8_str =
      String::NewFromUtf8(isolate_, utf8_data).ToLocalChecked();

  Maybe<size_t> size_maybe =
      StringBytes::StorageSize(isolate_, utf8_str, node::LATIN1);

  ASSERT_TRUE(size_maybe.IsJust());
  size_t size = size_maybe.FromJust();
  ASSERT_EQ(size, 12u);

  MaybeStackBuffer<char> buf;
  size_t written = StringBytes::Write(
      isolate_, buf.out(), buf.capacity(), utf8_str, node::LATIN1);
  ASSERT_EQ(written, 12u);

  // Null-terminate the buffer and compare the contents.
  buf.SetLength(13);
  buf[12] = '\0';
  ASSERT_STREQ(latin1_data, buf.out());
}

// Verify that StringBytes::Write converts two-byte characters to one-byte
// characters, even if there is no valid one-byte representation.
TEST_F(StringBytesTest, WriteLatin1WithInvalidChar) {
  const HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env_{handle_scope, argv};

  Local<String> utf8_str =
      String::NewFromUtf8(isolate_, "Hello, 世界").ToLocalChecked();

  Maybe<size_t> size_maybe =
      StringBytes::StorageSize(isolate_, utf8_str, node::LATIN1);

  ASSERT_TRUE(size_maybe.IsJust());
  size_t size = size_maybe.FromJust();
  ASSERT_EQ(size, 9u);

  MaybeStackBuffer<char> buf;
  size_t written = StringBytes::Write(
      isolate_, buf.out(), buf.capacity(), utf8_str, node::LATIN1);
  ASSERT_EQ(written, 9u);

  // Null-terminate the buffer and compare the contents.
  buf.SetLength(10);
  buf[9] = '\0';
  ASSERT_STREQ("Hello, \x16\x4C", buf.out());
}
