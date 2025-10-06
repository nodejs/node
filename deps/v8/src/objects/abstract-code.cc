// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/abstract-code.h"

#include "src/objects/abstract-code-inl.h"

namespace v8 {
namespace internal {

int AbstractCode::SourcePosition(Isolate* isolate, int offset) {
  PtrComprCageBase cage_base(isolate);
  Tagged<Map> map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode()->SourcePosition(offset);
  } else {
    return GetBytecodeArray()->SourcePosition(offset);
  }
}

int AbstractCode::SourceStatementPosition(Isolate* isolate, int offset) {
  PtrComprCageBase cage_base(isolate);
  Tagged<Map> map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode()->SourceStatementPosition(offset);
  } else {
    return GetBytecodeArray()->SourceStatementPosition(offset);
  }
}

bool AbstractCode::is_context_specialized(PtrComprCageBase cage_base) {
  Tagged<Map> map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode()->is_context_specialized();
  } else {
    DCHECK(InstanceTypeChecker::IsBytecodeArray(map_object));
    return false;
  }
}

BytecodeOffset AbstractCode::osr_offset(PtrComprCageBase cage_base) {
  Tagged<Map> map_object = map(cage_base);
  if (InstanceTypeChecker::IsCode(map_object)) {
    return GetCode()->osr_offset();
  } else {
    DCHECK(InstanceTypeChecker::IsBytecodeArray(map_object));
    return BytecodeOffset::None();
  }
}

}  // namespace internal
}  // namespace v8
