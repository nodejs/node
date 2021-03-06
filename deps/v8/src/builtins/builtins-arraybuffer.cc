// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/handles/maybe-handles-inl.h"
#include "src/heap/heap-inl.h"  // For ToBoolean. TODO(jkummerow): Drop.
#include "src/logging/counters.h"
#include "src/numbers/conversions.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

#define CHECK_SHARED(expected, name, method)                                \
  if (name->is_shared() != expected) {                                      \
    THROW_NEW_ERROR_RETURN_FAILURE(                                         \
        isolate,                                                            \
        NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,          \
                     isolate->factory()->NewStringFromAsciiChecked(method), \
                     name));                                                \
  }

// -----------------------------------------------------------------------------
// ES#sec-arraybuffer-objects

namespace {

Object ConstructBuffer(Isolate* isolate, Handle<JSFunction> target,
                       Handle<JSReceiver> new_target, Handle<Object> length,
                       InitializedFlag initialized) {
  SharedFlag shared = (*target != target->native_context().array_buffer_fun())
                          ? SharedFlag::kShared
                          : SharedFlag::kNotShared;
  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::New(target, new_target, Handle<AllocationSite>::null()));
  auto array_buffer = Handle<JSArrayBuffer>::cast(result);
  // Ensure that all fields are initialized because BackingStore::Allocate is
  // allowed to GC. Note that we cannot move the allocation of the ArrayBuffer
  // after BackingStore::Allocate because of the spec.
  array_buffer->Setup(shared, nullptr);

  size_t byte_length;
  if (!TryNumberToSize(*length, &byte_length) ||
      byte_length > JSArrayBuffer::kMaxByteLength) {
    // ToNumber failed.
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidArrayBufferLength));
  }

  auto backing_store =
      BackingStore::Allocate(isolate, byte_length, shared, initialized);
  if (!backing_store) {
    // Allocation of backing store failed.
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kArrayBufferAllocationFailed));
  }

  array_buffer->Attach(std::move(backing_store));
  return *array_buffer;
}

}  // namespace

// ES #sec-arraybuffer-constructor
BUILTIN(ArrayBufferConstructor) {
  HandleScope scope(isolate);
  Handle<JSFunction> target = args.target();
  DCHECK(*target == target->native_context().array_buffer_fun() ||
         *target == target->native_context().shared_array_buffer_fun());
  if (args.new_target()->IsUndefined(isolate)) {  // [[Call]]
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kConstructorNotFunction,
                              handle(target->shared().Name(), isolate)));
  }
  // [[Construct]]
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());
  Handle<Object> length = args.atOrUndefined(isolate, 1);

  Handle<Object> number_length;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, number_length,
                                     Object::ToInteger(isolate, length));
  if (number_length->Number() < 0.0) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidArrayBufferLength));
    }

    return ConstructBuffer(isolate, target, new_target, number_length,
                           InitializedFlag::kZeroInitialized);
}

// This is a helper to construct an ArrayBuffer with uinitialized memory.
// This means the caller must ensure the buffer is totally initialized in
// all cases, or we will expose uinitialized memory to user code.
BUILTIN(ArrayBufferConstructor_DoNotInitialize) {
  HandleScope scope(isolate);
  Handle<JSFunction> target(isolate->native_context()->array_buffer_fun(),
                            isolate);
  Handle<Object> length = args.atOrUndefined(isolate, 1);
  return ConstructBuffer(isolate, target, target, length,
                         InitializedFlag::kUninitialized);
}

static Object SliceHelper(BuiltinArguments args, Isolate* isolate,
                          const char* kMethodName, bool is_shared) {
  HandleScope scope(isolate);
  Handle<Object> start = args.at(1);
  Handle<Object> end = args.atOrUndefined(isolate, 2);

  // * If Type(O) is not Object, throw a TypeError exception.
  // * If O does not have an [[ArrayBufferData]] internal slot, throw a
  //   TypeError exception.
  CHECK_RECEIVER(JSArrayBuffer, array_buffer, kMethodName);
  // * [AB] If IsSharedArrayBuffer(O) is true, throw a TypeError exception.
  // * [SAB] If IsSharedArrayBuffer(O) is false, throw a TypeError exception.
  CHECK_SHARED(is_shared, array_buffer, kMethodName);

  // * [AB] If IsDetachedBuffer(buffer) is true, throw a TypeError exception.
  if (!is_shared && array_buffer->was_detached()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kDetachedOperation,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  kMethodName)));
  }

  // * [AB] Let len be O.[[ArrayBufferByteLength]].
  // * [SAB] Let len be O.[[ArrayBufferByteLength]].
  double const len = array_buffer->byte_length();

  // * Let relativeStart be ? ToInteger(start).
  Handle<Object> relative_start;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, relative_start,
                                     Object::ToInteger(isolate, start));

  // * If relativeStart < 0, let first be max((len + relativeStart), 0); else
  //   let first be min(relativeStart, len).
  double const first = (relative_start->Number() < 0)
                           ? std::max(len + relative_start->Number(), 0.0)
                           : std::min(relative_start->Number(), len);
  Handle<Object> first_obj = isolate->factory()->NewNumber(first);

  // * If end is undefined, let relativeEnd be len; else let relativeEnd be ?
  //   ToInteger(end).
  double relative_end;
  if (end->IsUndefined(isolate)) {
    relative_end = len;
  } else {
    Handle<Object> relative_end_obj;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, relative_end_obj,
                                       Object::ToInteger(isolate, end));
    relative_end = relative_end_obj->Number();
  }

  // * If relativeEnd < 0, let final be max((len + relativeEnd), 0); else let
  //   final be min(relativeEnd, len).
  double const final_ = (relative_end < 0) ? std::max(len + relative_end, 0.0)
                                           : std::min(relative_end, len);

  // * Let newLen be max(final-first, 0).
  double const new_len = std::max(final_ - first, 0.0);
  Handle<Object> new_len_obj = isolate->factory()->NewNumber(new_len);

  // * [AB] Let ctor be ? SpeciesConstructor(O, %ArrayBuffer%).
  // * [SAB] Let ctor be ? SpeciesConstructor(O, %SharedArrayBuffer%).
  Handle<JSFunction> constructor_fun = is_shared
                                           ? isolate->shared_array_buffer_fun()
                                           : isolate->array_buffer_fun();
  Handle<Object> ctor;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, ctor,
      Object::SpeciesConstructor(
          isolate, Handle<JSReceiver>::cast(args.receiver()), constructor_fun));

  // * Let new be ? Construct(ctor, newLen).
  Handle<JSReceiver> new_;
  {
    const int argc = 1;

    ScopedVector<Handle<Object>> argv(argc);
    argv[0] = new_len_obj;

    Handle<Object> new_obj;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, new_obj, Execution::New(isolate, ctor, argc, argv.begin()));

    new_ = Handle<JSReceiver>::cast(new_obj);
  }

  // * If new does not have an [[ArrayBufferData]] internal slot, throw a
  //   TypeError exception.
  if (!new_->IsJSArrayBuffer()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                     isolate->factory()->NewStringFromAsciiChecked(kMethodName),
                     new_));
  }

  // * [AB] If IsSharedArrayBuffer(new) is true, throw a TypeError exception.
  // * [SAB] If IsSharedArrayBuffer(new) is false, throw a TypeError exception.
  Handle<JSArrayBuffer> new_array_buffer = Handle<JSArrayBuffer>::cast(new_);
  CHECK_SHARED(is_shared, new_array_buffer, kMethodName);

  // * [AB] If IsDetachedBuffer(new) is true, throw a TypeError exception.
  if (!is_shared && new_array_buffer->was_detached()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kDetachedOperation,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  kMethodName)));
  }

  // * [AB] If SameValue(new, O) is true, throw a TypeError exception.
  if (!is_shared && new_->SameValue(*args.receiver())) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kArrayBufferSpeciesThis));
  }

  // * [SAB] If new.[[ArrayBufferData]] and O.[[ArrayBufferData]] are the same
  //         Shared Data Block values, throw a TypeError exception.
  if (is_shared &&
      new_array_buffer->backing_store() == array_buffer->backing_store()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kSharedArrayBufferSpeciesThis));
  }

  // * If new.[[ArrayBufferByteLength]] < newLen, throw a TypeError exception.
  if (new_array_buffer->byte_length() < new_len) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(is_shared ? MessageTemplate::kSharedArrayBufferTooShort
                               : MessageTemplate::kArrayBufferTooShort));
  }

  // * [AB] NOTE: Side-effects of the above steps may have detached O.
  // * [AB] If IsDetachedBuffer(O) is true, throw a TypeError exception.
  if (!is_shared && array_buffer->was_detached()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kDetachedOperation,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  kMethodName)));
  }

  // * Let fromBuf be O.[[ArrayBufferData]].
  // * Let toBuf be new.[[ArrayBufferData]].
  // * Perform CopyDataBlockBytes(toBuf, 0, fromBuf, first, newLen).
  size_t first_size = 0, new_len_size = 0;
  CHECK(TryNumberToSize(*first_obj, &first_size));
  CHECK(TryNumberToSize(*new_len_obj, &new_len_size));
  DCHECK(new_array_buffer->byte_length() >= new_len_size);

  if (new_len_size != 0) {
    size_t from_byte_length = array_buffer->byte_length();
    USE(from_byte_length);
    DCHECK(first_size <= from_byte_length);
    DCHECK(from_byte_length - first_size >= new_len_size);
    uint8_t* from_data =
        reinterpret_cast<uint8_t*>(array_buffer->backing_store());
    uint8_t* to_data =
        reinterpret_cast<uint8_t*>(new_array_buffer->backing_store());
    CopyBytes(to_data, from_data + first_size, new_len_size);
  }

  return *new_;
}

// ES #sec-sharedarraybuffer.prototype.slice
BUILTIN(SharedArrayBufferPrototypeSlice) {
  const char* const kMethodName = "SharedArrayBuffer.prototype.slice";
  return SliceHelper(args, isolate, kMethodName, true);
}

// ES #sec-arraybuffer.prototype.slice
// ArrayBuffer.prototype.slice ( start, end )
BUILTIN(ArrayBufferPrototypeSlice) {
  const char* const kMethodName = "ArrayBuffer.prototype.slice";
  return SliceHelper(args, isolate, kMethodName, false);
}

}  // namespace internal
}  // namespace v8
