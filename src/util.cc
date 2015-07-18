#include "util.h"
#include "string_bytes.h"

namespace node {

Utf8Value::Utf8Value(v8::Isolate* isolate, v8::Local<v8::Value> value)
    : length_(0), str_(str_st_) {
  if (value.IsEmpty())
    return;

  v8::Local<v8::String> string = value->ToString(isolate);
  if (string.IsEmpty())
    return;

  // Allocate enough space to include the null terminator
  size_t len = StringBytes::StorageSize(isolate, string, UTF8) + 1;
  if (len > sizeof(str_st_)) {
    str_ = static_cast<char*>(malloc(len));
    CHECK_NE(str_, nullptr);
  }

  const int flags =
      v8::String::NO_NULL_TERMINATION | v8::String::REPLACE_INVALID_UTF8;
  length_ = string->WriteUtf8(str_, len, 0, flags);
  str_[length_] = '\0';
}

}  // namespace node
