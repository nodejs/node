// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_REGEXP_MATCH_INFO_INL_H_
#define V8_OBJECTS_REGEXP_MATCH_INFO_INL_H_

#include "src/objects/fixed-array-inl.h"
#include "src/objects/regexp-match-info.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/regexp-match-info-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(RegExpMatchInfo)

int RegExpMatchInfo::NumberOfCaptureRegisters() {
  DCHECK_GE(length(), kLastMatchOverhead);
  Tagged<Object> obj = get(kNumberOfCapturesIndex);
  return Smi::ToInt(obj);
}

void RegExpMatchInfo::SetNumberOfCaptureRegisters(int value) {
  DCHECK_GE(length(), kLastMatchOverhead);
  set(kNumberOfCapturesIndex, Smi::FromInt(value));
}

Tagged<String> RegExpMatchInfo::LastSubject() {
  DCHECK_GE(length(), kLastMatchOverhead);
  return String::cast(get(kLastSubjectIndex));
}

void RegExpMatchInfo::SetLastSubject(Tagged<String> value,
                                     WriteBarrierMode mode) {
  DCHECK_GE(length(), kLastMatchOverhead);
  set(kLastSubjectIndex, value, mode);
}

Tagged<Object> RegExpMatchInfo::LastInput() {
  DCHECK_GE(length(), kLastMatchOverhead);
  return get(kLastInputIndex);
}

void RegExpMatchInfo::SetLastInput(Tagged<Object> value,
                                   WriteBarrierMode mode) {
  DCHECK_GE(length(), kLastMatchOverhead);
  set(kLastInputIndex, value, mode);
}

int RegExpMatchInfo::Capture(int i) {
  DCHECK_LT(i, NumberOfCaptureRegisters());
  Tagged<Object> obj = get(kFirstCaptureIndex + i);
  return Smi::ToInt(obj);
}

void RegExpMatchInfo::SetCapture(int i, int value) {
  DCHECK_LT(i, NumberOfCaptureRegisters());
  set(kFirstCaptureIndex + i, Smi::FromInt(value));
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_REGEXP_MATCH_INFO_INL_H_
