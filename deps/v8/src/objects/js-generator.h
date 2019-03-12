// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_GENERATOR_H_
#define V8_OBJECTS_JS_GENERATOR_H_

#include "src/objects/js-objects.h"
#include "src/objects/struct.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// Forward declarations.
class JSPromise;

class JSGeneratorObject : public JSObject {
 public:
  // [function]: The function corresponding to this generator object.
  DECL_ACCESSORS(function, JSFunction)

  // [context]: The context of the suspended computation.
  DECL_ACCESSORS(context, Context)

  // [receiver]: The receiver of the suspended computation.
  DECL_ACCESSORS(receiver, Object)

  // [input_or_debug_pos]
  // For executing generators: the most recent input value.
  // For suspended generators: debug information (bytecode offset).
  // There is currently no need to remember the most recent input value for a
  // suspended generator.
  DECL_ACCESSORS(input_or_debug_pos, Object)

  // [resume_mode]: The most recent resume mode.
  enum ResumeMode { kNext, kReturn, kThrow };
  DECL_INT_ACCESSORS(resume_mode)

  // [continuation]
  //
  // A positive value indicates a suspended generator.  The special
  // kGeneratorExecuting and kGeneratorClosed values indicate that a generator
  // cannot be resumed.
  inline int continuation() const;
  inline void set_continuation(int continuation);
  inline bool is_closed() const;
  inline bool is_executing() const;
  inline bool is_suspended() const;

  // For suspended generators: the source position at which the generator
  // is suspended.
  int source_position() const;

  // [parameters_and_registers]: Saved interpreter register file.
  DECL_ACCESSORS(parameters_and_registers, FixedArray)

  DECL_CAST(JSGeneratorObject)

  // Dispatched behavior.
  DECL_PRINTER(JSGeneratorObject)
  DECL_VERIFIER(JSGeneratorObject)

  // Magic sentinel values for the continuation.
  static const int kGeneratorExecuting = -2;
  static const int kGeneratorClosed = -1;

  // Layout description.
#define JS_GENERATOR_FIELDS(V)                  \
  V(kFunctionOffset, kTaggedSize)               \
  V(kContextOffset, kTaggedSize)                \
  V(kReceiverOffset, kTaggedSize)               \
  V(kInputOrDebugPosOffset, kTaggedSize)        \
  V(kResumeModeOffset, kTaggedSize)             \
  V(kContinuationOffset, kTaggedSize)           \
  V(kParametersAndRegistersOffset, kTaggedSize) \
  /* Header size. */                            \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize, JS_GENERATOR_FIELDS)
#undef JS_GENERATOR_FIELDS

  OBJECT_CONSTRUCTORS(JSGeneratorObject, JSObject);
};

class JSAsyncFunctionObject : public JSGeneratorObject {
 public:
  DECL_CAST(JSAsyncFunctionObject)

  // Dispatched behavior.
  DECL_VERIFIER(JSAsyncFunctionObject)

  // [promise]: The promise of the async function.
  DECL_ACCESSORS(promise, JSPromise)

  // Layout description.
#define JS_ASYNC_FUNCTION_FIELDS(V) \
  V(kPromiseOffset, kTaggedSize)    \
  /* Header size. */                \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSGeneratorObject::kSize,
                                JS_ASYNC_FUNCTION_FIELDS)
#undef JS_ASYNC_FUNCTION_FIELDS

  OBJECT_CONSTRUCTORS(JSAsyncFunctionObject, JSGeneratorObject);
};

class JSAsyncGeneratorObject : public JSGeneratorObject {
 public:
  DECL_CAST(JSAsyncGeneratorObject)

  // Dispatched behavior.
  DECL_VERIFIER(JSAsyncGeneratorObject)

  // [queue]
  // Pointer to the head of a singly linked list of AsyncGeneratorRequest, or
  // undefined.
  DECL_ACCESSORS(queue, HeapObject)

  // [is_awaiting]
  // Whether or not the generator is currently awaiting.
  DECL_INT_ACCESSORS(is_awaiting)

  // Layout description.
#define JS_ASYNC_GENERATOR_FIELDS(V) \
  V(kQueueOffset, kTaggedSize)       \
  V(kIsAwaitingOffset, kTaggedSize)  \
  /* Header size. */                 \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSGeneratorObject::kSize,
                                JS_ASYNC_GENERATOR_FIELDS)
#undef JS_ASYNC_GENERATOR_FIELDS

  OBJECT_CONSTRUCTORS(JSAsyncGeneratorObject, JSGeneratorObject);
};

class AsyncGeneratorRequest : public Struct {
 public:
  // Holds an AsyncGeneratorRequest, or Undefined.
  DECL_ACCESSORS(next, Object)
  DECL_INT_ACCESSORS(resume_mode)
  DECL_ACCESSORS(value, Object)
  DECL_ACCESSORS(promise, Object)

// Layout description.
#define ASYNC_GENERATOR_REQUEST_FIELDS(V) \
  V(kNextOffset, kTaggedSize)             \
  V(kResumeModeOffset, kTaggedSize)       \
  V(kValueOffset, kTaggedSize)            \
  V(kPromiseOffset, kTaggedSize)          \
  /* Total size. */                       \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(Struct::kHeaderSize,
                                ASYNC_GENERATOR_REQUEST_FIELDS)
#undef ASYNC_GENERATOR_REQUEST_FIELDS

  DECL_CAST(AsyncGeneratorRequest)
  DECL_PRINTER(AsyncGeneratorRequest)
  DECL_VERIFIER(AsyncGeneratorRequest)

  OBJECT_CONSTRUCTORS(AsyncGeneratorRequest, Struct);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_GENERATOR_H_
