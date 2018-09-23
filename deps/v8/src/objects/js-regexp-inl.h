// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_REGEXP_INL_H_
#define V8_OBJECTS_JS_REGEXP_INL_H_

#include "src/objects/js-regexp.h"

#include "src/objects-inl.h"  // Needed for write barriers
#include "src/objects/string.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CAST_ACCESSOR(JSRegExp)

ACCESSORS(JSRegExp, data, Object, kDataOffset)
ACCESSORS(JSRegExp, flags, Object, kFlagsOffset)
ACCESSORS(JSRegExp, source, Object, kSourceOffset)
ACCESSORS(JSRegExp, last_index, Object, kLastIndexOffset)

JSRegExp::Type JSRegExp::TypeTag() {
  Object* data = this->data();
  if (data->IsUndefined()) return JSRegExp::NOT_COMPILED;
  Smi* smi = Smi::cast(FixedArray::cast(data)->get(kTagIndex));
  return static_cast<JSRegExp::Type>(smi->value());
}

int JSRegExp::CaptureCount() {
  switch (TypeTag()) {
    case ATOM:
      return 0;
    case IRREGEXP:
      return Smi::ToInt(DataAt(kIrregexpCaptureCountIndex));
    default:
      UNREACHABLE();
  }
}

JSRegExp::Flags JSRegExp::GetFlags() {
  DCHECK(this->data()->IsFixedArray());
  Object* data = this->data();
  Smi* smi = Smi::cast(FixedArray::cast(data)->get(kFlagsIndex));
  return Flags(smi->value());
}

String* JSRegExp::Pattern() {
  DCHECK(this->data()->IsFixedArray());
  Object* data = this->data();
  String* pattern = String::cast(FixedArray::cast(data)->get(kSourceIndex));
  return pattern;
}

Object* JSRegExp::CaptureNameMap() {
  DCHECK(this->data()->IsFixedArray());
  DCHECK_EQ(TypeTag(), IRREGEXP);
  Object* value = DataAt(kIrregexpCaptureNameMapIndex);
  DCHECK_NE(value, Smi::FromInt(JSRegExp::kUninitializedValue));
  return value;
}

Object* JSRegExp::DataAt(int index) {
  DCHECK(TypeTag() != NOT_COMPILED);
  return FixedArray::cast(data())->get(index);
}

void JSRegExp::SetDataAt(int index, Object* value) {
  DCHECK(TypeTag() != NOT_COMPILED);
  DCHECK_GE(index,
            kDataIndex);  // Only implementation data can be set this way.
  FixedArray::cast(data())->set(index, value);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_REGEXP_INL_H_
