// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_BUILTIN_SERIALIZER_H_
#define V8_SNAPSHOT_BUILTIN_SERIALIZER_H_

#include "src/interpreter/interpreter.h"
#include "src/snapshot/builtin-serializer-allocator.h"
#include "src/snapshot/builtin-snapshot-utils.h"
#include "src/snapshot/serializer.h"

namespace v8 {
namespace internal {

class StartupSerializer;

// Responsible for serializing builtin and bytecode handler objects during
// startup snapshot creation into a dedicated area of the snapshot.
// See snapshot.h for documentation of the snapshot layout.
class BuiltinSerializer : public Serializer<BuiltinSerializerAllocator> {
  using BSU = BuiltinSnapshotUtils;

 public:
  BuiltinSerializer(Isolate* isolate, StartupSerializer* startup_serializer);
  ~BuiltinSerializer() override;

  void SerializeBuiltinsAndHandlers();

 private:
  void VisitRootPointers(Root root, const char* description, Object** start,
                         Object** end) override;

  void SerializeBuiltin(Code* code);
  void SerializeHandler(Code* code);
  void SerializeObject(HeapObject* o, HowToCode how_to_code,
                       WhereToPoint where_to_point, int skip) override;

  void SetBuiltinOffset(int builtin_id, uint32_t offset);
  void SetHandlerOffset(interpreter::Bytecode bytecode,
                        interpreter::OperandScale operand_scale,
                        uint32_t offset);

  // The startup serializer is needed for access to the partial snapshot cache,
  // which is used to serialize things like embedded constants.
  StartupSerializer* startup_serializer_;

  // Stores the starting offset, within the serialized data, of each code
  // object. This is later packed into the builtin snapshot, and used by the
  // builtin deserializer to deserialize individual builtins and bytecode
  // handlers.
  //
  // Indices [kFirstBuiltinIndex, kFirstBuiltinIndex + kNumberOfBuiltins[:
  //     Builtin offsets.
  // Indices [kFirstHandlerIndex, kFirstHandlerIndex + kNumberOfHandlers[:
  //     Bytecode handler offsets.
  uint32_t code_offsets_[BuiltinSnapshotUtils::kNumberOfCodeObjects];

  DISALLOW_COPY_AND_ASSIGN(BuiltinSerializer);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_BUILTIN_SERIALIZER_H_
