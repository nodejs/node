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

  char* str = static_cast<char*>(calloc(1, len));

  int flags = WRITE_UTF8_FLAGS;
  flags |= ~v8::String::NO_NULL_TERMINATION;

  length_ = val_->WriteUtf8(str,
                            len,
                            0,
                            flags);

  str_ = reinterpret_cast<char*>(str);
}
}  // namespace node
