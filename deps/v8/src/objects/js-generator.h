// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_GENERATOR_H_
#define V8_OBJECTS_JS_GENERATOR_H_

#include "src/objects/js-objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

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
  static const int kFunctionOffset = JSObject::kHeaderSize;
  static const int kContextOffset = kFunctionOffset + kPointerSize;
  static const int kReceiverOffset = kContextOffset + kPointerSize;
  static const int kInputOrDebugPosOffset = kReceiverOffset + kPointerSize;
  static const int kResumeModeOffset = kInputOrDebugPosOffset + kPointerSize;
  static const int kContinuationOffset = kResumeModeOffset + kPointerSize;
  static const int kParametersAndRegistersOffset =
      kContinuationOffset + kPointerSize;
  static const int kSize = kParametersAndRegistersOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSGeneratorObject);
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
  static const int kQueueOffset = JSGeneratorObject::kSize;
  static const int kIsAwaitingOffset = kQueueOffset + kPointerSize;
  static const int kSize = kIsAwaitingOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSAsyncGeneratorObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_GENERATOR_H_
