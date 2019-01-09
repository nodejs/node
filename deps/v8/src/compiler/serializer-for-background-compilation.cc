// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/serializer-for-background-compilation.h"

#include "src/compiler/js-heap-broker.h"
#include "src/handles-inl.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/objects/code.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

class SerializerForBackgroundCompilation::Environment : public ZoneObject {
 public:
  explicit Environment(Zone* zone, Isolate* isolate,
                       MaybeHandle<JSFunction> closure);

 private:
  Zone* zone() const { return zone_; }

  Zone* zone_;

  MaybeHandle<JSFunction> closure_;
};

SerializerForBackgroundCompilation::Environment::Environment(
    Zone* zone, Isolate* isolate, MaybeHandle<JSFunction> closure)
    : zone_(zone), closure_(closure) {
  USE(isolate);
}

SerializerForBackgroundCompilation::SerializerForBackgroundCompilation(
    JSHeapBroker* broker, Zone* zone, Handle<JSFunction> closure)
    : broker_(broker),
      zone_(zone),
      environment_(new (zone) Environment(zone, broker_->isolate(), closure)),
      closure_(closure),
      bytecode_array_(
          handle(closure->shared()->GetBytecodeArray(), broker->isolate())) {}

void SerializerForBackgroundCompilation::Run() { TraverseBytecode(); }

void SerializerForBackgroundCompilation::TraverseBytecode() {
  interpreter::BytecodeArrayIterator bytecode_iterator(bytecode_array());

  for (; !bytecode_iterator.done(); bytecode_iterator.Advance()) {
    switch (bytecode_iterator.current_bytecode()) {
#define DEFINE_BYTECODE_CASE(name)     \
  case interpreter::Bytecode::k##name: \
    Visit##name();                     \
    break;
      SUPPORTED_BYTECODE_LIST(DEFINE_BYTECODE_CASE)
#undef DEFINE_BYTECODE_CASE
      default:
        break;
    }
  }
}

void SerializerForBackgroundCompilation::VisitIllegal() {}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
