// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/compile-time-value.h"

#include "src/ast/ast.h"
#include "src/factory.h"
#include "src/handles-inl.h"
#include "src/isolate.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

bool CompileTimeValue::IsCompileTimeValue(Expression* expression) {
  if (expression->IsLiteral()) return true;
  MaterializedLiteral* literal = expression->AsMaterializedLiteral();
  if (literal == nullptr) return false;
  return literal->IsSimple();
}

Handle<FixedArray> CompileTimeValue::GetValue(Isolate* isolate,
                                              Expression* expression) {
  Factory* factory = isolate->factory();
  DCHECK(IsCompileTimeValue(expression));
  Handle<FixedArray> result = factory->NewFixedArray(2, TENURED);
  if (expression->IsObjectLiteral()) {
    ObjectLiteral* object_literal = expression->AsObjectLiteral();
    DCHECK(object_literal->is_simple());
    int literalTypeFlag = object_literal->EncodeLiteralType();
    DCHECK_NE(kArrayLiteralFlag, literalTypeFlag);
    result->set(kLiteralTypeSlot, Smi::FromInt(literalTypeFlag));
    result->set(kElementsSlot, *object_literal->constant_properties());
  } else {
    ArrayLiteral* array_literal = expression->AsArrayLiteral();
    DCHECK(array_literal->is_simple());
    result->set(kLiteralTypeSlot, Smi::FromInt(kArrayLiteralFlag));
    result->set(kElementsSlot, *array_literal->constant_elements());
  }
  return result;
}

int CompileTimeValue::GetLiteralTypeFlags(Handle<FixedArray> value) {
  return Smi::ToInt(value->get(kLiteralTypeSlot));
}

Handle<HeapObject> CompileTimeValue::GetElements(Handle<FixedArray> value) {
  return Handle<HeapObject>(HeapObject::cast(value->get(kElementsSlot)));
}

}  // namespace internal
}  // namespace v8
