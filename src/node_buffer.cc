// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "node_buffer.h"
#include "node.h"
#include "node_errors.h"
#include "node_internals.h"

#include "env-inl.h"
#include "string_bytes.h"
#include "string_search.h"
#include "util-inl.h"
#include "v8-profiler.h"
#include "v8.h"

#include <string.h>
#include <limits.h>

#define THROW_AND_RETURN_UNLESS_BUFFER(env, obj)                            \
  THROW_AND_RETURN_IF_NOT_BUFFER(env, obj, "argument")                      \

#define THROW_AND_RETURN_IF_OOB(r)                                          \
  do {                                                                      \
    if ((r).IsNothing()) return;                                            \
    if (!(r).FromJust())                                                    \
      return node::THROW_ERR_OUT_OF_RANGE(env, "Index out of range");       \
  } while (0)                                                               \

#define SLICE_START_END(env, start_arg, end_arg, end_max)                   \
  size_t start = 0;                                                         \
  size_t end = 0;                                                           \
  THROW_AND_RETURN_IF_OOB(ParseArrayIndex(env, start_arg, 0, &start));      \
  THROW_AND_RETURN_IF_OOB(ParseArrayIndex(env, end_arg, end_max, &end));    \
  if (end < start) end = start;                                             \
  THROW_AND_RETURN_IF_OOB(Just(end <= end_max));                            \
  size_t length = end - start;

namespace node {

namespace {

inline void* BufferMalloc(size_t length) {
  return per_process::cli_options->zero_fill_all_buffers ?
             node::UncheckedCalloc(length) :
             node::UncheckedMalloc(length);
}

}  // namespace

namespace Buffer {

using v8::ArrayBuffer;
using v8::ArrayBufferCreationMode;
using v8::ArrayBufferView;
using v8::Context;
using v8::EscapableHandleScope;
using v8::FunctionCallbackInfo;
using v8::Integer;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Uint32Array;
using v8::Uint8Array;
using v8::Value;
using v8::WeakCallbackInfo;

namespace {

class CallbackInfo {
 public:
  static inline void Free(char* data, void* hint);
  static inline CallbackInfo* New(Isolate* isolate,
                                  Local<ArrayBuffer> object,
                                  FreeCallback callback,
                                  char* data,
                                  void* hint = nullptr);
 private:
  static void WeakCallback(const WeakCallbackInfo<CallbackInfo>&);
  inline void WeakCallback(Isolate* isolate);
  inline CallbackInfo(Isolate* isolate,
                      Local<ArrayBuffer> object,
                      FreeCallback callback,
                      char* data,
                      void* hint);
  Persistent<ArrayBuffer> persistent_;
  FreeCallback const callback_;
  char* const data_;
  void* const hint_;
  DISALLOW_COPY_AND_ASSIGN(CallbackInfo);
};


void CallbackInfo::Free(char* data, void*) {
  ::free(data);
}


CallbackInfo* CallbackInfo::New(Isolate* isolate,
                                Local<ArrayBuffer> object,
                                FreeCallback callback,
                                char* data,
                                void* hint) {
  return new CallbackInfo(isolate, object, callback, data, hint);
}


CallbackInfo::CallbackInfo(Isolate* isolate,
                           Local<ArrayBuffer> object,
                           FreeCallback callback,
                           char* data,
                           void* hint)
    : persistent_(isolate, object),
      callback_(callback),
      data_(data),
      hint_(hint) {
  ArrayBuffer::Contents obj_c = object->GetContents();
  CHECK_EQ(data_, static_cast<char*>(obj_c.Data()));
  if (object->ByteLength() != 0)
    CHECK_NOT_NULL(data_);

  persistent_.SetWeak(this, WeakCallback, v8::WeakCallbackType::kParameter);
  isolate->AdjustAmountOfExternalAllocatedMemory(sizeof(*this));
}


void CallbackInfo::WeakCallback(
    const WeakCallbackInfo<CallbackInfo>& data) {
  CallbackInfo* self = data.GetParameter();
  self->WeakCallback(data.GetIsolate());
  delete self;
}


void CallbackInfo::WeakCallback(Isolate* isolate) {
  callback_(data_, hint_);
  int64_t change_in_bytes = -static_cast<int64_t>(sizeof(*this));
  isolate->AdjustAmountOfExternalAllocatedMemory(change_in_bytes);
}


// Parse index for external array data. An empty Maybe indicates
// a pending exception. `false` indicates that the index is out-of-bounds.
inline MUST_USE_RESULT Maybe<bool> ParseArrayIndex(Environment* env,
                                                   Local<Value> arg,
                                                   size_t def,
                                                   size_t* ret) {
  if (arg->IsUndefined()) {
    *ret = def;
    return Just(true);
  }

  int64_t tmp_i;
  if (!arg->IntegerValue(env->context()).To(&tmp_i))
    return Nothing<bool>();

  if (tmp_i < 0)
    return Just(false);

  // Check that the result fits in a size_t.
  const uint64_t kSizeMax = static_cast<uint64_t>(static_cast<size_t>(-1));
  // coverity[pointless_expression]
  if (static_cast<uint64_t>(tmp_i) > kSizeMax)
    return Just(false);

  *ret = static_cast<size_t>(tmp_i);
  return Just(true);
}

}  // anonymous namespace

// Buffer methods

bool HasInstance(Local<Value> val) {
  return val->IsArrayBufferView();
}


bool HasInstance(Local<Object> obj) {
  return obj->IsArrayBufferView();
}


char* Data(Local<Value> val) {
  CHECK(val->IsArrayBufferView());
  Local<ArrayBufferView> ui = val.As<ArrayBufferView>();
  ArrayBuffer::Contents ab_c = ui->Buffer()->GetContents();
  return static_cast<char*>(ab_c.Data()) + ui->ByteOffset();
}


char* Data(Local<Object> obj) {
  CHECK(obj->IsArrayBufferView());
  Local<ArrayBufferView> ui = obj.As<ArrayBufferView>();
  ArrayBuffer::Contents ab_c = ui->Buffer()->GetContents();
  return static_cast<char*>(ab_c.Data()) + ui->ByteOffset();
}


size_t Length(Local<Value> val) {
  CHECK(val->IsArrayBufferView());
  Local<ArrayBufferView> ui = val.As<ArrayBufferView>();
  return ui->ByteLength();
}


size_t Length(Local<Object> obj) {
  CHECK(obj->IsArrayBufferView());
  Local<ArrayBufferView> ui = obj.As<ArrayBufferView>();
  return ui->ByteLength();
}


MaybeLocal<Object> New(Isolate* isolate,
                       Local<String> string,
                       enum encoding enc) {
  EscapableHandleScope scope(isolate);

  size_t length;
  if (!StringBytes::Size(isolate, string, enc).To(&length))
    return Local<Object>();
  size_t actual = 0;
  char* data = nullptr;

  if (length > 0) {
    data = static_cast<char*>(BufferMalloc(length));

    if (data == nullptr) {
      THROW_ERR_MEMORY_ALLOCATION_FAILED(isolate);
      return Local<Object>();
    }

    actual = StringBytes::Write(isolate, data, length, string, enc);
    CHECK(actual <= length);

    if (actual == 0) {
      free(data);
      data = nullptr;
    } else if (actual < length) {
      data = node::Realloc(data, actual);
    }
  }

  Local<Object> buf;
  if (New(isolate, data, actual).ToLocal(&buf))
    return scope.Escape(buf);

  // Object failed to be created. Clean up resources.
  free(data);
  return Local<Object>();
}


MaybeLocal<Object> New(Isolate* isolate, size_t length) {
  EscapableHandleScope handle_scope(isolate);
  Local<Object> obj;
  Environment* env = Environment::GetCurrent(isolate);
  if (env == nullptr) {
    THROW_ERR_BUFFER_CONTEXT_NOT_AVAILABLE(isolate);
    return MaybeLocal<Object>();
  }
  if (Buffer::New(env, length).ToLocal(&obj))
    return handle_scope.Escape(obj);
  return Local<Object>();
}


MaybeLocal<Object> New(Environment* env, size_t length) {
  EscapableHandleScope scope(env->isolate());

  // V8 currently only allows a maximum Typed Array index of max Smi.
  if (length > kMaxLength) {
    env->isolate()->ThrowException(ERR_BUFFER_TOO_LARGE(env->isolate()));
    return Local<Object>();
  }

  void* data;
  if (length > 0) {
    data = BufferMalloc(length);
    if (data == nullptr) {
      THROW_ERR_MEMORY_ALLOCATION_FAILED(env);
      return Local<Object>();
    }
  } else {
    data = nullptr;
  }

  Local<ArrayBuffer> ab =
    ArrayBuffer::New(env->isolate(),
        data,
        length,
        ArrayBufferCreationMode::kInternalized);
  Local<Object> obj;
  if (Buffer::New(env, ab, 0, length).ToLocal(&obj))
    return scope.Escape(obj);
  return Local<Object>();
}


MaybeLocal<Object> Copy(Isolate* isolate, const char* data, size_t length) {
  EscapableHandleScope handle_scope(isolate);
  Environment* env = Environment::GetCurrent(isolate);
  if (env == nullptr) {
    THROW_ERR_BUFFER_CONTEXT_NOT_AVAILABLE(isolate);
    return MaybeLocal<Object>();
  }
  Local<Object> obj;
  if (Buffer::Copy(env, data, length).ToLocal(&obj))
    return handle_scope.Escape(obj);
  return Local<Object>();
}


MaybeLocal<Object> Copy(Environment* env, const char* data, size_t length) {
  EscapableHandleScope scope(env->isolate());

  // V8 currently only allows a maximum Typed Array index of max Smi.
  if (length > kMaxLength) {
    env->isolate()->ThrowException(ERR_BUFFER_TOO_LARGE(env->isolate()));
    return Local<Object>();
  }

  void* new_data;
  if (length > 0) {
    CHECK_NOT_NULL(data);
    new_data = node::UncheckedMalloc(length);
    if (new_data == nullptr) {
      THROW_ERR_MEMORY_ALLOCATION_FAILED(env);
      return Local<Object>();
    }
    memcpy(new_data, data, length);
  } else {
    new_data = nullptr;
  }

  Local<ArrayBuffer> ab =
    ArrayBuffer::New(env->isolate(),
        new_data,
        length,
        ArrayBufferCreationMode::kInternalized);
  Local<Object> obj;
  if (Buffer::New(env, ab, 0, length).ToLocal(&obj))
    return scope.Escape(obj);
  return Local<Object>();
}


MaybeLocal<Object> New(Isolate* isolate,
                       char* data,
                       size_t length,
                       FreeCallback callback,
                       void* hint) {
  EscapableHandleScope handle_scope(isolate);
  Environment* env = Environment::GetCurrent(isolate);
  if (env == nullptr) {
    callback(data, hint);
    THROW_ERR_BUFFER_CONTEXT_NOT_AVAILABLE(isolate);
    return MaybeLocal<Object>();
  }
  Local<Object> obj;
  if (Buffer::New(env, data, length, callback, hint).ToLocal(&obj))
    return handle_scope.Escape(obj);
  return Local<Object>();
}


MaybeLocal<Object> New(Environment* env,
                       char* data,
                       size_t length,
                       FreeCallback callback,
                       void* hint) {
  EscapableHandleScope scope(env->isolate());

  if (length > kMaxLength) {
    env->isolate()->ThrowException(ERR_BUFFER_TOO_LARGE(env->isolate()));
    callback(data, hint);
    return Local<Object>();
  }

  Local<ArrayBuffer> ab = ArrayBuffer::New(env->isolate(), data, length);
  MaybeLocal<Uint8Array> ui = Buffer::New(env, ab, 0, length);

  CallbackInfo::New(env->isolate(), ab, callback, data, hint);

  if (ui.IsEmpty())
    return MaybeLocal<Object>();

  return scope.Escape(ui.ToLocalChecked());
}


MaybeLocal<Object> New(Isolate* isolate, char* data, size_t length) {
  EscapableHandleScope handle_scope(isolate);
  Environment* env = Environment::GetCurrent(isolate);
  if (env == nullptr) {
    free(data);
    THROW_ERR_BUFFER_CONTEXT_NOT_AVAILABLE(isolate);
    return MaybeLocal<Object>();
  }
  Local<Object> obj;
  if (Buffer::New(env, data, length).ToLocal(&obj))
    return handle_scope.Escape(obj);
  return Local<Object>();
}


MaybeLocal<Object> New(Environment* env, char* data, size_t length) {
  if (length > 0) {
    CHECK_NOT_NULL(data);
    CHECK(length <= kMaxLength);
  }

  Local<ArrayBuffer> ab =
      ArrayBuffer::New(env->isolate(),
                       data,
                       length,
                       ArrayBufferCreationMode::kInternalized);
  return Buffer::New(env, ab, 0, length).FromMaybe(Local<Object>());
}

namespace {

void CreateFromString(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsString());

  enum encoding enc = ParseEncoding(args.GetIsolate(),
                                    args[1].As<String>(),
                                    UTF8);
  Local<Object> buf;
  if (New(args.GetIsolate(), args[0].As<String>(), enc).ToLocal(&buf))
    args.GetReturnValue().Set(buf);
}


template <encoding encoding>
void StringSlice(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  THROW_AND_RETURN_UNLESS_BUFFER(env, args.This());
  SPREAD_BUFFER_ARG(args.This(), ts_obj);

  if (ts_obj_length == 0)
    return args.GetReturnValue().SetEmptyString();

  SLICE_START_END(env, args[0], args[1], ts_obj_length)

  Local<Value> error;
  MaybeLocal<Value> ret =
      StringBytes::Encode(isolate,
                          ts_obj_data + start,
                          length,
                          encoding,
                          &error);
  if (ret.IsEmpty()) {
    CHECK(!error.IsEmpty());
    isolate->ThrowException(error);
    return;
  }
  args.GetReturnValue().Set(ret.ToLocalChecked());
}


// bytesCopied = copy(buffer, target[, targetStart][, sourceStart][, sourceEnd])
void Copy(const FunctionCallbackInfo<Value> &args) {
  Environment* env = Environment::GetCurrent(args);

  THROW_AND_RETURN_UNLESS_BUFFER(env, args[0]);
  THROW_AND_RETURN_UNLESS_BUFFER(env, args[1]);
  Local<Object> buffer_obj = args[0].As<Object>();
  Local<Object> target_obj = args[1].As<Object>();
  SPREAD_BUFFER_ARG(buffer_obj, ts_obj);
  SPREAD_BUFFER_ARG(target_obj, target);

  size_t target_start = 0;
  size_t source_start = 0;
  size_t source_end = 0;

  THROW_AND_RETURN_IF_OOB(ParseArrayIndex(env, args[2], 0, &target_start));
  THROW_AND_RETURN_IF_OOB(ParseArrayIndex(env, args[3], 0, &source_start));
  THROW_AND_RETURN_IF_OOB(ParseArrayIndex(env, args[4], ts_obj_length,
                                          &source_end));

  // Copy 0 bytes; we're done
  if (target_start >= target_length || source_start >= source_end)
    return args.GetReturnValue().Set(0);

  if (source_start > ts_obj_length)
    return THROW_ERR_OUT_OF_RANGE(
        env, "The value of \"sourceStart\" is out of range.");

  if (source_end - source_start > target_length - target_start)
    source_end = source_start + target_length - target_start;

  uint32_t to_copy = std::min(
      std::min(source_end - source_start, target_length - target_start),
      ts_obj_length - source_start);

  memmove(target_data + target_start, ts_obj_data + source_start, to_copy);
  args.GetReturnValue().Set(to_copy);
}


void Fill(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> ctx = env->context();

  THROW_AND_RETURN_UNLESS_BUFFER(env, args[0]);
  SPREAD_BUFFER_ARG(args[0], ts_obj);

  uint32_t start;
  if (!args[2]->Uint32Value(ctx).To(&start)) return;
  uint32_t end;
  if (!args[3]->Uint32Value(ctx).To(&end)) return;
  size_t fill_length = end - start;
  Local<String> str_obj;
  size_t str_length;
  enum encoding enc;

  // OOB Check. Throw the error in JS.
  if (start > end || fill_length + start > ts_obj_length)
    return args.GetReturnValue().Set(-2);

  // First check if Buffer has been passed.
  if (Buffer::HasInstance(args[1])) {
    SPREAD_BUFFER_ARG(args[1], fill_obj);
    str_length = fill_obj_length;
    memcpy(
        ts_obj_data + start, fill_obj_data, std::min(str_length, fill_length));
    goto start_fill;
  }

  // Then coerce everything that's not a string.
  if (!args[1]->IsString()) {
    uint32_t val;
    if (!args[1]->Uint32Value(ctx).To(&val)) return;
    int value = val & 255;
    memset(ts_obj_data + start, value, fill_length);
    return;
  }

  str_obj = args[1]->ToString(env->context()).ToLocalChecked();
  enc = ParseEncoding(env->isolate(), args[4], UTF8);

  // Can't use StringBytes::Write() in all cases. For example if attempting
  // to write a two byte character into a one byte Buffer.
  if (enc == UTF8) {
    str_length = str_obj->Utf8Length(env->isolate());
    node::Utf8Value str(env->isolate(), args[1]);
    memcpy(ts_obj_data + start, *str, std::min(str_length, fill_length));

  } else if (enc == UCS2) {
    str_length = str_obj->Length() * sizeof(uint16_t);
    node::TwoByteValue str(env->isolate(), args[1]);
    if (IsBigEndian())
      SwapBytes16(reinterpret_cast<char*>(&str[0]), str_length);

    memcpy(ts_obj_data + start, *str, std::min(str_length, fill_length));

  } else {
    // Write initial String to Buffer, then use that memory to copy remainder
    // of string. Correct the string length for cases like HEX where less than
    // the total string length is written.
    str_length = StringBytes::Write(env->isolate(),
                                    ts_obj_data + start,
                                    fill_length,
                                    str_obj,
                                    enc,
                                    nullptr);
  }

 start_fill:

  if (str_length >= fill_length)
    return;

  // If str_length is zero, then either an empty buffer was provided, or Write()
  // indicated that no bytes could be written. If no bytes could be written,
  // then return -1 because the fill value is invalid. This will trigger a throw
  // in JavaScript. Silently failing should be avoided because it can lead to
  // buffers with unexpected contents.
  if (str_length == 0)
    return args.GetReturnValue().Set(-1);

  size_t in_there = str_length;
  char* ptr = ts_obj_data + start + str_length;

  while (in_there < fill_length - in_there) {
    memcpy(ptr, ts_obj_data + start, in_there);
    ptr += in_there;
    in_there *= 2;
  }

  if (in_there < fill_length) {
    memcpy(ptr, ts_obj_data + start, fill_length - in_there);
  }
}


template <encoding encoding>
void StringWrite(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  THROW_AND_RETURN_UNLESS_BUFFER(env, args.This());
  SPREAD_BUFFER_ARG(args.This(), ts_obj);

  THROW_AND_RETURN_IF_NOT_STRING(env, args[0], "argument");

  Local<String> str = args[0]->ToString(env->context()).ToLocalChecked();

  size_t offset;
  size_t max_length;

  THROW_AND_RETURN_IF_OOB(ParseArrayIndex(env, args[1], 0, &offset));
  if (offset > ts_obj_length) {
    return node::THROW_ERR_BUFFER_OUT_OF_BOUNDS(
        env, "\"offset\" is outside of buffer bounds");
  }

  THROW_AND_RETURN_IF_OOB(ParseArrayIndex(env, args[2], ts_obj_length - offset,
                                          &max_length));

  max_length = std::min(ts_obj_length - offset, max_length);

  if (max_length == 0)
    return args.GetReturnValue().Set(0);

  uint32_t written = StringBytes::Write(env->isolate(),
                                        ts_obj_data + offset,
                                        max_length,
                                        str,
                                        encoding,
                                        nullptr);
  args.GetReturnValue().Set(written);
}

void ByteLengthUtf8(const FunctionCallbackInfo<Value> &args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsString());

  // Fast case: avoid StringBytes on UTF8 string. Jump to v8.
  args.GetReturnValue().Set(args[0].As<String>()->Utf8Length(env->isolate()));
}

// Normalize val to be an integer in the range of [1, -1] since
// implementations of memcmp() can vary by platform.
static int normalizeCompareVal(int val, size_t a_length, size_t b_length) {
  if (val == 0) {
    if (a_length > b_length)
      return 1;
    else if (a_length < b_length)
      return -1;
  } else {
    if (val > 0)
      return 1;
    else
      return -1;
  }
  return val;
}

void CompareOffset(const FunctionCallbackInfo<Value> &args) {
  Environment* env = Environment::GetCurrent(args);

  THROW_AND_RETURN_UNLESS_BUFFER(env, args[0]);
  THROW_AND_RETURN_UNLESS_BUFFER(env, args[1]);
  SPREAD_BUFFER_ARG(args[0], ts_obj);
  SPREAD_BUFFER_ARG(args[1], target);

  size_t target_start = 0;
  size_t source_start = 0;
  size_t source_end = 0;
  size_t target_end = 0;

  THROW_AND_RETURN_IF_OOB(ParseArrayIndex(env, args[2], 0, &target_start));
  THROW_AND_RETURN_IF_OOB(ParseArrayIndex(env, args[3], 0, &source_start));
  THROW_AND_RETURN_IF_OOB(ParseArrayIndex(env, args[4], target_length,
                                          &target_end));
  THROW_AND_RETURN_IF_OOB(ParseArrayIndex(env, args[5], ts_obj_length,
                                          &source_end));

  if (source_start > ts_obj_length)
    return THROW_ERR_OUT_OF_RANGE(
        env, "The value of \"sourceStart\" is out of range.");
  if (target_start > target_length)
    return THROW_ERR_OUT_OF_RANGE(
        env, "The value of \"targetStart\" is out of range.");

  CHECK_LE(source_start, source_end);
  CHECK_LE(target_start, target_end);

  size_t to_cmp =
      std::min(std::min(source_end - source_start, target_end - target_start),
               ts_obj_length - source_start);

  int val = normalizeCompareVal(to_cmp > 0 ?
                                  memcmp(ts_obj_data + source_start,
                                         target_data + target_start,
                                         to_cmp) : 0,
                                source_end - source_start,
                                target_end - target_start);

  args.GetReturnValue().Set(val);
}

void Compare(const FunctionCallbackInfo<Value> &args) {
  Environment* env = Environment::GetCurrent(args);

  THROW_AND_RETURN_UNLESS_BUFFER(env, args[0]);
  THROW_AND_RETURN_UNLESS_BUFFER(env, args[1]);
  SPREAD_BUFFER_ARG(args[0], obj_a);
  SPREAD_BUFFER_ARG(args[1], obj_b);

  size_t cmp_length = std::min(obj_a_length, obj_b_length);

  int val = normalizeCompareVal(cmp_length > 0 ?
                                memcmp(obj_a_data, obj_b_data, cmp_length) : 0,
                                obj_a_length, obj_b_length);
  args.GetReturnValue().Set(val);
}


// Computes the offset for starting an indexOf or lastIndexOf search.
// Returns either a valid offset in [0...<length - 1>], ie inside the Buffer,
// or -1 to signal that there is no possible match.
int64_t IndexOfOffset(size_t length,
                      int64_t offset_i64,
                      int64_t needle_length,
                      bool is_forward) {
  int64_t length_i64 = static_cast<int64_t>(length);
  if (offset_i64 < 0) {
    if (offset_i64 + length_i64 >= 0) {
      // Negative offsets count backwards from the end of the buffer.
      return length_i64 + offset_i64;
    } else if (is_forward || needle_length == 0) {
      // indexOf from before the start of the buffer: search the whole buffer.
      return 0;
    } else {
      // lastIndexOf from before the start of the buffer: no match.
      return -1;
    }
  } else {
    if (offset_i64 + needle_length <= length_i64) {
      // Valid positive offset.
      return offset_i64;
    } else if (needle_length == 0) {
      // Out of buffer bounds, but empty needle: point to end of buffer.
      return length_i64;
    } else if (is_forward) {
      // indexOf from past the end of the buffer: no match.
      return -1;
    } else {
      // lastIndexOf from past the end of the buffer: search the whole buffer.
      return length_i64 - 1;
    }
  }
}

void IndexOfString(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  CHECK(args[1]->IsString());
  CHECK(args[2]->IsNumber());
  CHECK(args[4]->IsBoolean());

  enum encoding enc = ParseEncoding(isolate, args[3], UTF8);

  THROW_AND_RETURN_UNLESS_BUFFER(env, args[0]);
  SPREAD_BUFFER_ARG(args[0], ts_obj);

  Local<String> needle = args[1].As<String>();
  int64_t offset_i64 = args[2].As<Integer>()->Value();
  bool is_forward = args[4]->IsTrue();

  const char* haystack = ts_obj_data;
  // Round down to the nearest multiple of 2 in case of UCS2.
  const size_t haystack_length = (enc == UCS2) ?
      ts_obj_length &~ 1 : ts_obj_length;  // NOLINT(whitespace/operators)

  size_t needle_length;
  if (!StringBytes::Size(isolate, needle, enc).To(&needle_length)) return;

  int64_t opt_offset = IndexOfOffset(haystack_length,
                                     offset_i64,
                                     needle_length,
                                     is_forward);

  if (needle_length == 0) {
    // Match String#indexOf() and String#lastIndexOf() behavior.
    args.GetReturnValue().Set(static_cast<double>(opt_offset));
    return;
  }

  if (haystack_length == 0) {
    return args.GetReturnValue().Set(-1);
  }

  if (opt_offset <= -1) {
    return args.GetReturnValue().Set(-1);
  }
  size_t offset = static_cast<size_t>(opt_offset);
  CHECK_LT(offset, haystack_length);
  if ((is_forward && needle_length + offset > haystack_length) ||
      needle_length > haystack_length) {
    return args.GetReturnValue().Set(-1);
  }

  size_t result = haystack_length;

  if (enc == UCS2) {
    String::Value needle_value(isolate, needle);
    if (*needle_value == nullptr)
      return args.GetReturnValue().Set(-1);

    if (haystack_length < 2 || needle_value.length() < 1) {
      return args.GetReturnValue().Set(-1);
    }

    if (IsBigEndian()) {
      StringBytes::InlineDecoder decoder;
      if (decoder.Decode(env, needle, args[3], UCS2).IsNothing()) return;
      const uint16_t* decoded_string =
          reinterpret_cast<const uint16_t*>(decoder.out());

      if (decoded_string == nullptr)
        return args.GetReturnValue().Set(-1);

      result = SearchString(reinterpret_cast<const uint16_t*>(haystack),
                            haystack_length / 2,
                            decoded_string,
                            decoder.size() / 2,
                            offset / 2,
                            is_forward);
    } else {
      result = SearchString(reinterpret_cast<const uint16_t*>(haystack),
                            haystack_length / 2,
                            reinterpret_cast<const uint16_t*>(*needle_value),
                            needle_value.length(),
                            offset / 2,
                            is_forward);
    }
    result *= 2;
  } else if (enc == UTF8) {
    String::Utf8Value needle_value(isolate, needle);
    if (*needle_value == nullptr)
      return args.GetReturnValue().Set(-1);

    result = SearchString(reinterpret_cast<const uint8_t*>(haystack),
                          haystack_length,
                          reinterpret_cast<const uint8_t*>(*needle_value),
                          needle_length,
                          offset,
                          is_forward);
  } else if (enc == LATIN1) {
    uint8_t* needle_data = node::UncheckedMalloc<uint8_t>(needle_length);
    if (needle_data == nullptr) {
      return args.GetReturnValue().Set(-1);
    }
    needle->WriteOneByte(
        isolate, needle_data, 0, needle_length, String::NO_NULL_TERMINATION);

    result = SearchString(reinterpret_cast<const uint8_t*>(haystack),
                          haystack_length,
                          needle_data,
                          needle_length,
                          offset,
                          is_forward);
    free(needle_data);
  }

  args.GetReturnValue().Set(
      result == haystack_length ? -1 : static_cast<int>(result));
}

void IndexOfBuffer(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[1]->IsObject());
  CHECK(args[2]->IsNumber());
  CHECK(args[4]->IsBoolean());

  enum encoding enc = ParseEncoding(args.GetIsolate(),
                                    args[3],
                                    UTF8);

  THROW_AND_RETURN_UNLESS_BUFFER(Environment::GetCurrent(args), args[0]);
  THROW_AND_RETURN_UNLESS_BUFFER(Environment::GetCurrent(args), args[1]);
  SPREAD_BUFFER_ARG(args[0], ts_obj);
  SPREAD_BUFFER_ARG(args[1], buf);
  int64_t offset_i64 = args[2].As<Integer>()->Value();
  bool is_forward = args[4]->IsTrue();

  const char* haystack = ts_obj_data;
  const size_t haystack_length = ts_obj_length;
  const char* needle = buf_data;
  const size_t needle_length = buf_length;

  int64_t opt_offset = IndexOfOffset(haystack_length,
                                     offset_i64,
                                     needle_length,
                                     is_forward);

  if (needle_length == 0) {
    // Match String#indexOf() and String#lastIndexOf() behavior.
    args.GetReturnValue().Set(static_cast<double>(opt_offset));
    return;
  }

  if (haystack_length == 0) {
    return args.GetReturnValue().Set(-1);
  }

  if (opt_offset <= -1) {
    return args.GetReturnValue().Set(-1);
  }
  size_t offset = static_cast<size_t>(opt_offset);
  CHECK_LT(offset, haystack_length);
  if ((is_forward && needle_length + offset > haystack_length) ||
      needle_length > haystack_length) {
    return args.GetReturnValue().Set(-1);
  }

  size_t result = haystack_length;

  if (enc == UCS2) {
    if (haystack_length < 2 || needle_length < 2) {
      return args.GetReturnValue().Set(-1);
    }
    result = SearchString(
        reinterpret_cast<const uint16_t*>(haystack),
        haystack_length / 2,
        reinterpret_cast<const uint16_t*>(needle),
        needle_length / 2,
        offset / 2,
        is_forward);
    result *= 2;
  } else {
    result = SearchString(
        reinterpret_cast<const uint8_t*>(haystack),
        haystack_length,
        reinterpret_cast<const uint8_t*>(needle),
        needle_length,
        offset,
        is_forward);
  }

  args.GetReturnValue().Set(
      result == haystack_length ? -1 : static_cast<int>(result));
}

void IndexOfNumber(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[1]->IsUint32());
  CHECK(args[2]->IsNumber());
  CHECK(args[3]->IsBoolean());

  THROW_AND_RETURN_UNLESS_BUFFER(Environment::GetCurrent(args), args[0]);
  SPREAD_BUFFER_ARG(args[0], ts_obj);

  uint32_t needle = args[1].As<Uint32>()->Value();
  int64_t offset_i64 = args[2].As<Integer>()->Value();
  bool is_forward = args[3]->IsTrue();

  int64_t opt_offset = IndexOfOffset(ts_obj_length, offset_i64, 1, is_forward);
  if (opt_offset <= -1 || ts_obj_length == 0) {
    return args.GetReturnValue().Set(-1);
  }
  size_t offset = static_cast<size_t>(opt_offset);
  CHECK_LT(offset, ts_obj_length);

  const void* ptr;
  if (is_forward) {
    ptr = memchr(ts_obj_data + offset, needle, ts_obj_length - offset);
  } else {
    ptr = node::stringsearch::MemrchrFill(ts_obj_data, needle, offset + 1);
  }
  const char* ptr_char = static_cast<const char*>(ptr);
  args.GetReturnValue().Set(ptr ? static_cast<int>(ptr_char - ts_obj_data)
                                : -1);
}


void Swap16(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_UNLESS_BUFFER(env, args[0]);
  SPREAD_BUFFER_ARG(args[0], ts_obj);
  SwapBytes16(ts_obj_data, ts_obj_length);
  args.GetReturnValue().Set(args[0]);
}


void Swap32(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_UNLESS_BUFFER(env, args[0]);
  SPREAD_BUFFER_ARG(args[0], ts_obj);
  SwapBytes32(ts_obj_data, ts_obj_length);
  args.GetReturnValue().Set(args[0]);
}


void Swap64(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  THROW_AND_RETURN_UNLESS_BUFFER(env, args[0]);
  SPREAD_BUFFER_ARG(args[0], ts_obj);
  SwapBytes64(ts_obj_data, ts_obj_length);
  args.GetReturnValue().Set(args[0]);
}


// Encode a single string to a UTF-8 Uint8Array (not Buffer).
// Used in TextEncoder.prototype.encode.
static void EncodeUtf8String(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());

  Local<String> str = args[0].As<String>();
  size_t length = str->Utf8Length(isolate);
  char* data = node::UncheckedMalloc(length);
  str->WriteUtf8(isolate,
                 data,
                 -1,  // We are certain that `data` is sufficiently large
                 nullptr,
                 String::NO_NULL_TERMINATION | String::REPLACE_INVALID_UTF8);
  auto array_buf = ArrayBuffer::New(
      isolate, data, length, ArrayBufferCreationMode::kInternalized);
  auto array = Uint8Array::New(array_buf, 0, length);
  args.GetReturnValue().Set(array);
}


void SetBufferPrototype(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsObject());
  Local<Object> proto = args[0].As<Object>();
  env->set_buffer_prototype_object(proto);
}


void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);

  env->SetMethod(target, "setBufferPrototype", SetBufferPrototype);
  env->SetMethodNoSideEffect(target, "createFromString", CreateFromString);

  env->SetMethodNoSideEffect(target, "byteLengthUtf8", ByteLengthUtf8);
  env->SetMethod(target, "copy", Copy);
  env->SetMethodNoSideEffect(target, "compare", Compare);
  env->SetMethodNoSideEffect(target, "compareOffset", CompareOffset);
  env->SetMethod(target, "fill", Fill);
  env->SetMethodNoSideEffect(target, "indexOfBuffer", IndexOfBuffer);
  env->SetMethodNoSideEffect(target, "indexOfNumber", IndexOfNumber);
  env->SetMethodNoSideEffect(target, "indexOfString", IndexOfString);

  env->SetMethod(target, "swap16", Swap16);
  env->SetMethod(target, "swap32", Swap32);
  env->SetMethod(target, "swap64", Swap64);

  env->SetMethodNoSideEffect(target, "encodeUtf8String", EncodeUtf8String);

  target->Set(env->context(),
              FIXED_ONE_BYTE_STRING(env->isolate(), "kMaxLength"),
              Integer::NewFromUnsigned(env->isolate(), kMaxLength)).FromJust();

  target->Set(env->context(),
              FIXED_ONE_BYTE_STRING(env->isolate(), "kStringMaxLength"),
              Integer::New(env->isolate(), String::kMaxLength)).FromJust();

  env->SetMethodNoSideEffect(target, "asciiSlice", StringSlice<ASCII>);
  env->SetMethodNoSideEffect(target, "base64Slice", StringSlice<BASE64>);
  env->SetMethodNoSideEffect(target, "latin1Slice", StringSlice<LATIN1>);
  env->SetMethodNoSideEffect(target, "hexSlice", StringSlice<HEX>);
  env->SetMethodNoSideEffect(target, "ucs2Slice", StringSlice<UCS2>);
  env->SetMethodNoSideEffect(target, "utf8Slice", StringSlice<UTF8>);

  env->SetMethod(target, "asciiWrite", StringWrite<ASCII>);
  env->SetMethod(target, "base64Write", StringWrite<BASE64>);
  env->SetMethod(target, "latin1Write", StringWrite<LATIN1>);
  env->SetMethod(target, "hexWrite", StringWrite<HEX>);
  env->SetMethod(target, "ucs2Write", StringWrite<UCS2>);
  env->SetMethod(target, "utf8Write", StringWrite<UTF8>);

  // It can be a nullptr when running inside an isolate where we
  // do not own the ArrayBuffer allocator.
  if (uint32_t* zero_fill_field = env->isolate_data()->zero_fill_field()) {
    Local<ArrayBuffer> array_buffer = ArrayBuffer::New(
        env->isolate(), zero_fill_field, sizeof(*zero_fill_field));
    CHECK(target
              ->Set(env->context(),
                    FIXED_ONE_BYTE_STRING(env->isolate(), "zeroFill"),
                    Uint32Array::New(array_buffer, 0, 1))
              .FromJust());
  }
}

}  // anonymous namespace
}  // namespace Buffer
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(buffer, node::Buffer::Initialize)
