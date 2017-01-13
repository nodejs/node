// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Implementation for v8-experimental.h.
 */

#include "src/api-experimental.h"

#include "include/v8.h"
#include "include/v8-experimental.h"
#include "src/api.h"
#include "src/fast-accessor-assembler.h"

namespace {

v8::internal::FastAccessorAssembler* FromApi(
    v8::experimental::FastAccessorBuilder* builder) {
  return reinterpret_cast<v8::internal::FastAccessorAssembler*>(builder);
}

v8::experimental::FastAccessorBuilder* FromInternal(
    v8::internal::FastAccessorAssembler* fast_accessor_assembler) {
  return reinterpret_cast<v8::experimental::FastAccessorBuilder*>(
      fast_accessor_assembler);
}

}  // namespace

namespace v8 {
namespace internal {
namespace experimental {


MaybeHandle<Code> BuildCodeFromFastAccessorBuilder(
    v8::experimental::FastAccessorBuilder* fast_handler) {
  i::MaybeHandle<i::Code> code;
  if (fast_handler != nullptr) {
    auto faa = FromApi(fast_handler);
    code = faa->Build();
    CHECK(!code.is_null());
    delete faa;
  }
  return code;
}

}  // namespace experimental
}  // namespace internal


namespace experimental {


FastAccessorBuilder* FastAccessorBuilder::New(Isolate* isolate) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  internal::FastAccessorAssembler* faa =
      new internal::FastAccessorAssembler(i_isolate);
  return FromInternal(faa);
}


FastAccessorBuilder::ValueId FastAccessorBuilder::IntegerConstant(
    int const_value) {
  return FromApi(this)->IntegerConstant(const_value);
}


FastAccessorBuilder::ValueId FastAccessorBuilder::GetReceiver() {
  return FromApi(this)->GetReceiver();
}


FastAccessorBuilder::ValueId FastAccessorBuilder::LoadInternalField(
    ValueId value, int field_no) {
  return FromApi(this)->LoadInternalField(value, field_no);
}

FastAccessorBuilder::ValueId FastAccessorBuilder::LoadInternalFieldUnchecked(
    ValueId value, int field_no) {
  return FromApi(this)->LoadInternalFieldUnchecked(value, field_no);
}

FastAccessorBuilder::ValueId FastAccessorBuilder::LoadValue(ValueId value_id,
                                                            int offset) {
  return FromApi(this)->LoadValue(value_id, offset);
}


FastAccessorBuilder::ValueId FastAccessorBuilder::LoadObject(ValueId value_id,
                                                             int offset) {
  return FromApi(this)->LoadObject(value_id, offset);
}

FastAccessorBuilder::ValueId FastAccessorBuilder::ToSmi(ValueId value_id) {
  return FromApi(this)->ToSmi(value_id);
}

void FastAccessorBuilder::ReturnValue(ValueId value) {
  FromApi(this)->ReturnValue(value);
}


void FastAccessorBuilder::CheckFlagSetOrReturnNull(ValueId value_id, int mask) {
  FromApi(this)->CheckFlagSetOrReturnNull(value_id, mask);
}


void FastAccessorBuilder::CheckNotZeroOrReturnNull(ValueId value_id) {
  FromApi(this)->CheckNotZeroOrReturnNull(value_id);
}


FastAccessorBuilder::LabelId FastAccessorBuilder::MakeLabel() {
  return FromApi(this)->MakeLabel();
}


void FastAccessorBuilder::SetLabel(LabelId label_id) {
  FromApi(this)->SetLabel(label_id);
}

void FastAccessorBuilder::Goto(LabelId label_id) {
  FromApi(this)->Goto(label_id);
}

void FastAccessorBuilder::CheckNotZeroOrJump(ValueId value_id,
                                             LabelId label_id) {
  FromApi(this)->CheckNotZeroOrJump(value_id, label_id);
}

FastAccessorBuilder::ValueId FastAccessorBuilder::Call(
    v8::FunctionCallback callback, ValueId value_id) {
  return FromApi(this)->Call(callback, value_id);
}

}  // namespace experimental
}  // namespace v8
