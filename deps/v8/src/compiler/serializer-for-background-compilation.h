// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SERIALIZER_FOR_BACKGROUND_COMPILATION_H_
#define V8_COMPILER_SERIALIZER_FOR_BACKGROUND_COMPILATION_H_

#include "src/handles.h"

namespace v8 {
namespace internal {

namespace interpreter {
class BytecodeArrayIterator;
}

class FeedbackVector;
class SharedFunctionInfo;
class BytecodeArray;
class SourcePositionTableIterator;
class Zone;

namespace compiler {

#define SUPPORTED_BYTECODE_LIST(V) V(Illegal)

class JSHeapBroker;

// The SerializerForBackgroundCompilation makes sure that the relevant function
// data such as bytecode, SharedFunctionInfo and FeedbackVector, used by later
// optimizations in the compiler, is copied to the heap broker.
class SerializerForBackgroundCompilation {
 public:
  explicit SerializerForBackgroundCompilation(JSHeapBroker* broker, Zone* zone,
                                              Handle<JSFunction> closure);

  void Run();

  Zone* zone() const { return zone_; }

  const Handle<BytecodeArray>& bytecode_array() const {
    return bytecode_array_;
  }

 private:
  class Environment;

  void TraverseBytecode();

#define DECLARE_VISIT_BYTECODE(name, ...) void Visit##name();
  SUPPORTED_BYTECODE_LIST(DECLARE_VISIT_BYTECODE)
#undef DECLARE_VISIT_BYTECODE

  JSHeapBroker* broker() { return broker_; }
  Environment* environment() { return environment_; }

  JSHeapBroker* broker_;
  Zone* zone_;
  Environment* environment_;

  Handle<JSFunction> closure_;
  Handle<BytecodeArray> bytecode_array_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_SERIALIZER_FOR_BACKGROUND_COMPILATION_H_
