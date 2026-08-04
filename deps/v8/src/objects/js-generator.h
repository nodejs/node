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
class AsyncGeneratorRequest;
class JSPromise;
class StructBodyDescriptor;

#include "torque-generated/src/objects/js-generator-tq.inc"

V8_OBJECT class JSGeneratorObject : public JSObject {
 public:
  enum ResumeMode { kNext, kReturn, kThrow, kRethrow };

  inline Tagged<JSFunction> function() const;
  inline void set_function(Tagged<JSFunction> value,
                           WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Context> context() const;
  inline void set_context(Tagged<Context> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<JSAny> receiver() const;
  inline void set_receiver(Tagged<JSAny> value,
                           WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Object> input_or_debug_pos() const;
  inline void set_input_or_debug_pos(
      Tagged<Object> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int resume_mode() const;
  inline void set_resume_mode(int value);

  inline int continuation() const;
  inline void set_continuation(int value);

  inline Tagged<FixedArray> parameters_and_registers() const;
  inline void set_parameters_and_registers(
      Tagged<FixedArray> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline bool is_closed() const;
  inline bool is_executing() const;
  inline bool is_suspended() const;

  // For suspended generators: the source position at which the generator
  // is suspended.
  int source_position() const;
  int code_offset() const;

  // Dispatched behavior.
  DECL_PRINTER(JSGeneratorObject)
  DECL_VERIFIER(JSGeneratorObject)

  // Magic sentinel values for the continuation.
  static const int kGeneratorExecuting = -2;
  static const int kGeneratorClosed = -1;

 public:
  TaggedMember<JSFunction> function_;
  TaggedMember<Context> context_;
  TaggedMember<JSAny> receiver_;
  TaggedMember<Object> input_or_debug_pos_;
  TaggedMember<Smi> resume_mode_;
  TaggedMember<Smi> continuation_;
  TaggedMember<FixedArray> parameters_and_registers_;
} V8_OBJECT_END;

V8_OBJECT class JSAsyncFunctionObject final : public JSGeneratorObject {
 public:
  inline Tagged<JSPromise> promise() const;
  inline void set_promise(Tagged<JSPromise> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<JSFunction, Undefined>> await_resolve_closure() const;
  inline void set_await_resolve_closure(
      Tagged<UnionOf<JSFunction, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<JSFunction, Undefined>> await_reject_closure() const;
  inline void set_await_reject_closure(
      Tagged<UnionOf<JSFunction, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Dispatched behavior.
  DECL_VERIFIER(JSAsyncFunctionObject)
  DECL_PRINTER(JSAsyncFunctionObject)

 public:
  TaggedMember<JSPromise> promise_;
  TaggedMember<UnionOf<JSFunction, Undefined>> await_resolve_closure_;
  TaggedMember<UnionOf<JSFunction, Undefined>> await_reject_closure_;
} V8_OBJECT_END;

V8_OBJECT class JSAsyncGeneratorObject final : public JSGeneratorObject {
 public:
  inline Tagged<UnionOf<AsyncGeneratorRequest, Undefined>> queue() const;
  inline void set_queue(Tagged<UnionOf<AsyncGeneratorRequest, Undefined>> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int is_awaiting() const;
  inline void set_is_awaiting(int value);

  // Dispatched behavior.
  DECL_VERIFIER(JSAsyncGeneratorObject)
  DECL_PRINTER(JSAsyncGeneratorObject)

 public:
  TaggedMember<UnionOf<AsyncGeneratorRequest, Undefined>> queue_;
  TaggedMember<Smi> is_awaiting_;
} V8_OBJECT_END;

V8_OBJECT class AsyncGeneratorRequest : public Struct {
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
