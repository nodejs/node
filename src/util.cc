#include "util.h"
#include "string_bytes.h"
#include "node_buffer.h"
#include "node_internals.h"
#include <stdio.h>

namespace node {

using v8::Isolate;
using v8::Local;
using v8::String;
using v8::Value;

template <typename T>
static void MakeUtf8String(Isolate* isolate,
                           Local<Value> value,
                           T* target) {
  Local<String> string = value->ToString(isolate);
  if (string.IsEmpty())
    return;

  const size_t storage = StringBytes::StorageSize(isolate, string, UTF8) + 1;
  target->AllocateSufficientStorage(storage);
  const int flags =
      String::NO_NULL_TERMINATION | String::REPLACE_INVALID_UTF8;
  const int length = string->WriteUtf8(target->out(), storage, 0, flags);
  target->SetLengthAndZeroTerminate(length);
}

Utf8Value::Utf8Value(Isolate* isolate, Local<Value> value) {
  if (value.IsEmpty())
    return;

  MakeUtf8String(isolate, value, this);
}


TwoByteValue::TwoByteValue(Isolate* isolate, Local<Value> value) {
  if (value.IsEmpty()) {
    return;
  }

  Local<String> string = value->ToString(isolate);
  if (string.IsEmpty())
    return;

  // Allocate enough space to include the null terminator
  const size_t storage = string->Length() + 1;
  AllocateSufficientStorage(storage);

  const int flags =
      String::NO_NULL_TERMINATION | String::REPLACE_INVALID_UTF8;
  const int length = string->Write(out(), 0, storage, flags);
  SetLengthAndZeroTerminate(length);
}

BufferValue::BufferValue(Isolate* isolate, Local<Value> value) {
  // Slightly different take on Utf8Value. If value is a String,
  // it will return a Utf8 encoded string. If value is a Buffer,
  // it will copy the data out of the Buffer as is.
  if (value.IsEmpty()) {
    // Dereferencing this object will return nullptr.
    Invalidate();
    return;
  }

  if (value->IsString()) {
    MakeUtf8String(isolate, value, this);
  } else if (Buffer::HasInstance(value)) {
    const size_t len = Buffer::Length(value);
    // Leave place for the terminating '\0' byte.
    AllocateSufficientStorage(len + 1);
    memcpy(out(), Buffer::Data(value), len);
    SetLengthAndZeroTerminate(len);
  } else {
    Invalidate();
  }
}

void LowMemoryNotification() {
  if (v8_initialized) {
    auto isolate = v8::Isolate::GetCurrent();
    if (isolate != nullptr) {
      isolate->LowMemoryNotification();
    }
  }
}

}  // namespace node
