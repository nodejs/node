#include "util.h"

#include "string_bytes.h"

namespace node {

Utf8Value::Utf8Value(v8::Isolate* isolate, v8::Handle<v8::Value> value)
  : length_(0), str_(nullptr) {
  if (value.IsEmpty())
    return;

  v8::Local<v8::String> val_ = value->ToString(isolate);
  if (val_.IsEmpty())
    return;

  // Allocate enough space to include the null terminator
  size_t len = StringBytes::StorageSize(val_, UTF8) + 1;

  char* str;
  if (len > kStorageSize)
    str = static_cast<char*>(malloc(len));
  else
    str = str_st_;
  CHECK_NE(str, NULL);

  int flags = WRITE_UTF8_FLAGS;

  length_ = val_->WriteUtf8(str,
                            len,
                            0,
                            flags);
  str[length_] = '\0';

  str_ = reinterpret_cast<char*>(str);
}
}  // namespace node
