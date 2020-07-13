// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_REGEXP_INL_H_
#define V8_OBJECTS_JS_REGEXP_INL_H_

#include "src/objects/js-regexp.h"

#include "src/objects/js-array-inl.h"
#include "src/objects/objects-inl.h"  // Needed for write barriers
#include "src/objects/smi.h"
#include "src/objects/string.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

TQ_OBJECT_CONSTRUCTORS_IMPL(JSRegExp)
OBJECT_CONSTRUCTORS_IMPL_CHECK_SUPER(JSRegExpResult, JSArray)
OBJECT_CONSTRUCTORS_IMPL_CHECK_SUPER(JSRegExpResultIndices, JSArray)

CAST_ACCESSOR(JSRegExpResult)
CAST_ACCESSOR(JSRegExpResultIndices)

ACCESSORS(JSRegExp, last_index, Object, kLastIndexOffset)

JSRegExp::Type JSRegExp::TypeTag() const {
  Object data = this->data();
  if (data.IsUndefined()) return JSRegExp::NOT_COMPILED;
  Smi smi = Smi::cast(FixedArray::cast(data).get(kTagIndex));
  return static_cast<JSRegExp::Type>(smi.value());
}

int JSRegExp::CaptureCount() const {
  switch (TypeTag()) {
    case ATOM:
      return 0;
    case IRREGEXP:
      return Smi::ToInt(DataAt(kIrregexpCaptureCountIndex));
    default:
      UNREACHABLE();
  }
}

int JSRegExp::MaxRegisterCount() const {
  CHECK_EQ(TypeTag(), IRREGEXP);
  return Smi::ToInt(DataAt(kIrregexpMaxRegisterCountIndex));
}

JSRegExp::Flags JSRegExp::GetFlags() {
  DCHECK(this->data().IsFixedArray());
  Object data = this->data();
  Smi smi = Smi::cast(FixedArray::cast(data).get(kFlagsIndex));
  return Flags(smi.value());
}

String JSRegExp::Pattern() {
  DCHECK(this->data().IsFixedArray());
  Object data = this->data();
  String pattern = String::cast(FixedArray::cast(data).get(kSourceIndex));
  return pattern;
}

Object JSRegExp::CaptureNameMap() {
  DCHECK(this->data().IsFixedArray());
  DCHECK_EQ(TypeTag(), IRREGEXP);
  Object value = DataAt(kIrregexpCaptureNameMapIndex);
  DCHECK_NE(value, Smi::FromInt(JSRegExp::kUninitializedValue));
  return value;
}

Object JSRegExp::DataAt(int index) const {
  DCHECK(TypeTag() != NOT_COMPILED);
  return FixedArray::cast(data()).get(index);
}

void JSRegExp::SetDataAt(int index, Object value) {
  DCHECK(TypeTag() != NOT_COMPILED);
  DCHECK_GE(index,
            kDataIndex);  // Only implementation data can be set this way.
  FixedArray::cast(data()).set(index, value);
}

bool JSRegExp::HasCompiledCode() const {
  if (TypeTag() != IRREGEXP) return false;
  Smi uninitialized = Smi::FromInt(kUninitializedValue);
#ifdef DEBUG
  DCHECK(DataAt(kIrregexpLatin1CodeIndex).IsCode() ||
         DataAt(kIrregexpLatin1CodeIndex) == uninitialized);
  DCHECK(DataAt(kIrregexpUC16CodeIndex).IsCode() ||
         DataAt(kIrregexpUC16CodeIndex) == uninitialized);
  DCHECK(DataAt(kIrregexpLatin1BytecodeIndex).IsByteArray() ||
         DataAt(kIrregexpLatin1BytecodeIndex) == uninitialized);
  DCHECK(DataAt(kIrregexpUC16BytecodeIndex).IsByteArray() ||
         DataAt(kIrregexpUC16BytecodeIndex) == uninitialized);
#endif  // DEBUG
  return (DataAt(kIrregexpLatin1CodeIndex) != uninitialized ||
          DataAt(kIrregexpUC16CodeIndex) != uninitialized);
}

void JSRegExp::DiscardCompiledCodeForSerialization() {
  DCHECK(HasCompiledCode());
  Smi uninitialized = Smi::FromInt(kUninitializedValue);
  SetDataAt(kIrregexpLatin1CodeIndex, uninitialized);
  SetDataAt(kIrregexpUC16CodeIndex, uninitialized);
  SetDataAt(kIrregexpLatin1BytecodeIndex, uninitialized);
  SetDataAt(kIrregexpUC16BytecodeIndex, uninitialized);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_REGEXP_INL_H_
