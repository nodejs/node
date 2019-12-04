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

class JSGeneratorObject
    : public TorqueGeneratedJSGeneratorObject<JSGeneratorObject, JSObject> {
 public:
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

  // Dispatched behavior.
  DECL_PRINTER(JSGeneratorObject)

  // Magic sentinel values for the continuation.
  static const int kGeneratorExecuting = -2;
  static const int kGeneratorClosed = -1;

  TQ_OBJECT_CONSTRUCTORS(JSGeneratorObject)
};

class JSAsyncFunctionObject
    : public TorqueGeneratedJSAsyncFunctionObject<JSAsyncFunctionObject,
                                                  JSGeneratorObject> {
 public:
  // Dispatched behavior.
  DECL_VERIFIER(JSAsyncFunctionObject)

  TQ_OBJECT_CONSTRUCTORS(JSAsyncFunctionObject)
};

class JSAsyncGeneratorObject
    : public TorqueGeneratedJSAsyncGeneratorObject<JSAsyncGeneratorObject,
                                                   JSGeneratorObject> {
 public:
  // Dispatched behavior.
  DECL_VERIFIER(JSAsyncGeneratorObject)

  // [is_awaiting]
  // Whether or not the generator is currently awaiting.
  DECL_INT_ACCESSORS(is_awaiting)

  TQ_OBJECT_CONSTRUCTORS(JSAsyncGeneratorObject)
};

class AsyncGeneratorRequest
    : public TorqueGeneratedAsyncGeneratorRequest<AsyncGeneratorRequest,
                                                  Struct> {
 public:
  DECL_INT_ACCESSORS(resume_mode)

  DECL_PRINTER(AsyncGeneratorRequest)
  DECL_VERIFIER(AsyncGeneratorRequest)

  TQ_OBJECT_CONSTRUCTORS(AsyncGeneratorRequest)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_GENERATOR_H_
