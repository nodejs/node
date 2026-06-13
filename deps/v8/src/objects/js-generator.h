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
class StructBodyDescriptor;

#include "torque-generated/src/objects/js-generator-tq.inc"

class JSGeneratorObject
    : public TorqueGeneratedJSGeneratorObject<JSGeneratorObject, JSObject> {
 public:
  enum ResumeMode { kNext, kReturn, kThrow, kRethrow };

  inline bool is_closed() const;
  inline bool is_executing() const;
  inline bool is_suspended() const;

  // For suspended generators: the source position at which the generator
  // is suspended.
  int source_position() const;
  int code_offset() const;

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
  DECL_PRINTER(JSAsyncFunctionObject)

  TQ_OBJECT_CONSTRUCTORS(JSAsyncFunctionObject)
};

class JSAsyncGeneratorObject
    : public TorqueGeneratedJSAsyncGeneratorObject<JSAsyncGeneratorObject,
                                                   JSGeneratorObject> {
 public:
  // Dispatched behavior.
  DECL_VERIFIER(JSAsyncGeneratorObject)
  DECL_PRINTER(JSAsyncGeneratorObject)

  TQ_OBJECT_CONSTRUCTORS(JSAsyncGeneratorObject)
};

V8_OBJECT class AsyncGeneratorRequest : public StructLayout {
 public:
  inline Tagged<UnionOf<AsyncGeneratorRequest, Undefined>> next() const;
  inline void set_next(Tagged<UnionOf<AsyncGeneratorRequest, Undefined>> value,
                       WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int resume_mode() const;
  inline void set_resume_mode(int value);

  inline Tagged<Object> value() const;
  inline void set_value(Tagged<Object> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<JSPromise> promise() const;
  inline void set_promise(Tagged<JSPromise> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(AsyncGeneratorRequest)
  DECL_VERIFIER(AsyncGeneratorRequest)

  using BodyDescriptor = StructBodyDescriptor;

 public:
  TaggedMember<UnionOf<AsyncGeneratorRequest, Undefined>> next_;
  TaggedMember<Smi> resume_mode_;
  TaggedMember<Object> value_;
  TaggedMember<JSPromise> promise_;
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_GENERATOR_H_
