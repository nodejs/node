// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_BUILTIN_DESERIALIZER_H_
#define V8_SNAPSHOT_BUILTIN_DESERIALIZER_H_

#include "src/interpreter/interpreter.h"
#include "src/snapshot/builtin-deserializer-allocator.h"
#include "src/snapshot/builtin-snapshot-utils.h"
#include "src/snapshot/deserializer.h"

namespace v8 {
namespace internal {

class BuiltinSnapshotData;

// Deserializes the builtins blob.
class BuiltinDeserializer final
    : public Deserializer<BuiltinDeserializerAllocator> {
  using BSU = BuiltinSnapshotUtils;
  using Bytecode = interpreter::Bytecode;
  using OperandScale = interpreter::OperandScale;

 public:
  BuiltinDeserializer(Isolate* isolate, const BuiltinSnapshotData* data);

  // Builtins deserialization is tightly integrated with deserialization of the
  // startup blob. In particular, we need to ensure that no GC can occur
  // between startup- and builtins deserialization, as all builtins have been
  // pre-allocated and their pointers may not be invalidated.
  //
  // After this, the instruction cache must be flushed by the caller (we don't
  // do it ourselves since the startup serializer batch-flushes all code pages).
  void DeserializeEagerBuiltinsAndHandlers();

  // Deserializes the single given builtin. This is used whenever a builtin is
  // lazily deserialized at runtime.
  Code* DeserializeBuiltin(int builtin_id);

  // Deserializes the single given handler. This is used whenever a handler is
  // lazily deserialized at runtime.
  Code* DeserializeHandler(Bytecode bytecode, OperandScale operand_scale);

 private:
  // Deserializes the single given builtin. Assumes that reservations have
  // already been allocated.
  Code* DeserializeBuiltinRaw(int builtin_id);

  // Deserializes the single given bytecode handler. Assumes that reservations
  // have already been allocated.
  Code* DeserializeHandlerRaw(Bytecode bytecode, OperandScale operand_scale);

  // Extracts the size builtin Code objects (baked into the snapshot).
  uint32_t ExtractCodeObjectSize(int builtin_id);

  // BuiltinDeserializer implements its own builtin iteration logic. Make sure
  // the RootVisitor API is not used accidentally.
  void VisitRootPointers(Root root, Object** start, Object** end) override {
    UNREACHABLE();
  }

  int CurrentCodeObjectId() const { return current_code_object_id_; }

  // Convenience function to grab the handler off the heap's strong root list.
  Code* GetDeserializeLazyHandler(OperandScale operand_scale) const;

 private:
  // Stores the code object currently being deserialized. The
  // {current_code_object_id} stores the index of the currently-deserialized
  // code object within the snapshot (and within {code_offsets_}). We need this
  // to determine where to 'allocate' from during deserialization.
  static const int kNoCodeObjectId = -1;
  int current_code_object_id_ = kNoCodeObjectId;

  // The offsets of each builtin within the serialized data. Equivalent to
  // BuiltinSerializer::builtin_offsets_ but on the deserialization side.
  Vector<const uint32_t> code_offsets_;

  // For current_code_object_id_.
  friend class DeserializingCodeObjectScope;

  // For isolate(), IsLazyDeserializationEnabled(), CurrentCodeObjectId() and
  // ExtractBuiltinSize().
  friend class BuiltinDeserializerAllocator;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_BUILTIN_DESERIALIZER_H_
