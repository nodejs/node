#include "util.h"
#include "string_bytes.h"
#include "node_buffer.h"
#include <stdio.h>

namespace node {

using v8::Isolate;
using v8::Local;
using v8::String;
using v8::Value;

static int MakeUtf8String(Isolate* isolate,
                          Local<Value> value,
                          char** dst,
                          const size_t size) {
  Local<String> string = value->ToString(isolate);
  if (string.IsEmpty())
    return 0;
  size_t len = StringBytes::StorageSize(isolate, string, UTF8) + 1;
  if (len > size) {
    *dst = static_cast<char*>(malloc(len));
    CHECK_NE(*dst, nullptr);
  }
  const int flags =
      String::NO_NULL_TERMINATION | String::REPLACE_INVALID_UTF8;
  const int length = string->WriteUtf8(*dst, len, 0, flags);
  (*dst)[length] = '\0';
  return length;
}

Utf8Value::Utf8Value(Isolate* isolate, Local<Value> value)
    : length_(0), str_(str_st_) {
  if (value.IsEmpty())
    return;
  length_ = MakeUtf8String(isolate, value, &str_, sizeof(str_st_));
}


TwoByteValue::TwoByteValue(Isolate* isolate, Local<Value> value)
    : length_(0), str_(str_st_) {
  if (value.IsEmpty())
    return;

  Local<String> string = value->ToString(isolate);
  if (string.IsEmpty())
    return;

  // Allocate enough space to include the null terminator
  size_t len =
      StringBytes::StorageSize(isolate, string, UCS2) +
      sizeof(uint16_t);
  if (len > sizeof(str_st_)) {
    str_ = static_cast<uint16_t*>(malloc(len));
    CHECK_NE(str_, nullptr);
  }

  const int flags =
      String::NO_NULL_TERMINATION | String::REPLACE_INVALID_UTF8;
  length_ = string->Write(str_, 0, len, flags);
  str_[length_] = '\0';
}

BufferValue::BufferValue(Isolate* isolate, Local<Value> value)
    : str_(str_st_), fail_(true) {
  // Slightly different take on Utf8Value. If value is a String,
  // it will return a Utf8 encoded string. If value is a Buffer,
  // it will copy the data out of the Buffer as is.
  if (value.IsEmpty())
    return;
  if (value->IsString()) {
    MakeUtf8String(isolate, value, &str_, sizeof(str_st_));
    fail_ = false;
  } else if (Buffer::HasInstance(value)) {
    size_t len = Buffer::Length(value) + 1;
    if (len > sizeof(str_st_)) {
      str_ = static_cast<char*>(malloc(len));
      CHECK_NE(str_, nullptr);
    }
    memcpy(str_, Buffer::Data(value), len);
    str_[len - 1] = '\0';
    fail_ = false;
  }
}

}  // namespace node
