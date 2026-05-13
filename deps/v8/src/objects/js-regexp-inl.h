// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_REGEXP_INL_H_
#define V8_OBJECTS_JS_REGEXP_INL_H_

#include "src/objects/js-regexp.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/js-array-inl.h"
#include "src/objects/smi-inl.h"
#include "src/objects/string-inl.h"
#include "src/objects/struct-inl.h"
#include "src/objects/tagged-field-inl.h"
#include "src/objects/trusted-pointer-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-regexp-tq-inl.inc"

Tagged<Object> JSRegExp::last_index() const {
  return TaggedField<Object, kLastIndexOffset>::load(this);
}
void JSRegExp::set_last_index(Tagged<Object> value, WriteBarrierMode mode) {
  TaggedField<Object, kLastIndexOffset>::store(this, value);
  CONDITIONAL_WRITE_BARRIER(this, kLastIndexOffset, value, mode);
}

Tagged<String> JSRegExp::source(IsolateForSandbox isolate) const {
  return data(isolate)->escaped_source();
}

JSRegExp::Flags JSRegExp::flags() const {
  Tagged<Smi> smi = Cast<Smi>(flags_.load());
  return Flags(smi.value());
}
void JSRegExp::set_flags(Tagged<Object> value, WriteBarrierMode mode) {
  flags_.store(this, value, mode);
}

Tagged<RegExpData> JSRegExp::data(IsolateForSandbox isolate) const {
  return data_.load(isolate);
}
Tagged<RegExpData> JSRegExp::data(IsolateForSandbox isolate,
                                  AcquireLoadTag) const {
  return data_.Acquire_Load(isolate);
}
void JSRegExp::set_data(Tagged<RegExpData> value, WriteBarrierMode mode) {
  data_.store(this, value, mode);
}
void JSRegExp::set_data(Tagged<RegExpData> value, ReleaseStoreTag,
                        WriteBarrierMode mode) {
  data_.Release_Store(this, value, mode);
}
bool JSRegExp::has_data() const { return !data_.is_empty(); }
bool JSRegExp::has_data_unpublished(IsolateForSandbox isolate) const {
  return TrustedPointerField::IsTrustedPointerFieldUnpublished(
      Tagged<HeapObject>(this), offsetof(JSRegExp, data_),
      kRegExpDataIndirectPointerTag, isolate);
}
void JSRegExp::clear_data() { data_.clear(this); }

// static
const char* JSRegExp::FlagsToString(Flags flags, FlagsBuffer* out_buffer) {
  int cursor = 0;
  FlagsBuffer& buffer = *out_buffer;
#define V(Lower, Camel, LowerCamel, Char, Bit) \
  if (flags & JSRegExp::k##Camel) buffer[cursor++] = Char;
  REGEXP_FLAG_LIST(V)
#undef V
  buffer[cursor++] = '\0';
  return buffer.begin();
}

RegExpData::Type RegExpData::type_tag() const {
  return Type(type_tag_.load().value());
}

void RegExpData::set_type_tag(Type type) {
  type_tag_.store(this, Smi::FromEnum(type));
}

Tagged<String> RegExpData::original_source() const {
  return original_source_.load();
}
void RegExpData::set_original_source(Tagged<String> value,
                                     WriteBarrierMode mode) {
  DCHECK(value->IsFlat());
  original_source_.store(this, value, mode);
}

Tagged<String> RegExpData::escaped_source() const {
  return escaped_source_.load();
}
void RegExpData::set_escaped_source(Tagged<String> value,
                                    WriteBarrierMode mode) {
  DCHECK(value->IsFlat());
  escaped_source_.store(this, value, mode);
}

JSRegExp::Flags RegExpData::flags() const {
  return JSRegExp::Flags(flags_.load().value());
}

void RegExpData::set_flags(JSRegExp::Flags flags) {
  flags_.store(this, Smi::FromInt(flags));
}

Tagged<RegExpDataWrapper> RegExpData::wrapper() const {
  return wrapper_.load();
}
void RegExpData::set_wrapper(Tagged<RegExpDataWrapper> value,
                             WriteBarrierMode mode) {
  wrapper_.store(this, value, mode);
}

uint32_t RegExpData::quick_check_mask() const { return quick_check_mask_; }
void RegExpData::set_quick_check_mask(uint32_t value) {
  quick_check_mask_ = value;
}

uint32_t RegExpData::quick_check_value() const { return quick_check_value_; }
void RegExpData::set_quick_check_value(uint32_t value) {
  quick_check_value_ = value;
}

int RegExpData::capture_count() const {
  switch (type_tag()) {
    case Type::ATOM:
      return 0;
    case Type::EXPERIMENTAL:
    case Type::IRREGEXP:
      return UncheckedCast<IrRegExpData>(this)->capture_count();
  }
}

Tagged<RegExpData> RegExpDataWrapper::data(IsolateForSandbox isolate) const {
  return data_.load(isolate);
}
void RegExpDataWrapper::set_data(Tagged<RegExpData> value,
                                 WriteBarrierMode mode) {
  data_.store(this, value, mode);
}
bool RegExpDataWrapper::has_data() const { return !data_.is_empty(); }
void RegExpDataWrapper::clear_data() { data_.clear(this); }

Tagged<String> AtomRegExpData::pattern() const { return pattern_.load(); }
void AtomRegExpData::set_pattern(Tagged<String> value, WriteBarrierMode mode) {
  pattern_.store(this, value, mode);
}

Tagged<Code> IrRegExpData::latin1_code(IsolateForSandbox isolate) const {
  return latin1_code_.load(isolate);
}
void IrRegExpData::set_latin1_code(Tagged<Code> value, WriteBarrierMode mode) {
  latin1_code_.store(this, value, mode);
}
bool IrRegExpData::has_latin1_code() const { return !latin1_code_.is_empty(); }
void IrRegExpData::clear_latin1_code() { latin1_code_.clear(this); }

Tagged<Code> IrRegExpData::uc16_code(IsolateForSandbox isolate) const {
  return uc16_code_.load(isolate);
}
void IrRegExpData::set_uc16_code(Tagged<Code> value, WriteBarrierMode mode) {
  uc16_code_.store(this, value, mode);
}
bool IrRegExpData::has_uc16_code() const { return !uc16_code_.is_empty(); }
void IrRegExpData::clear_uc16_code() { uc16_code_.clear(this); }

bool IrRegExpData::has_code(bool is_one_byte) const {
  return is_one_byte ? has_latin1_code() : has_uc16_code();
}
void IrRegExpData::set_code(bool is_one_byte, Tagged<Code> code) {
  if (is_one_byte) {
    set_latin1_code(code);
  } else {
    set_uc16_code(code);
  }
}
Tagged<Code> IrRegExpData::code(IsolateForSandbox isolate,
                                bool is_one_byte) const {
  return is_one_byte ? latin1_code(isolate) : uc16_code(isolate);
}

Tagged<TrustedByteArray> IrRegExpData::latin1_bytecode() const {
  return latin1_bytecode_.load();
}
void IrRegExpData::set_latin1_bytecode(Tagged<TrustedByteArray> value,
                                       WriteBarrierMode mode) {
  latin1_bytecode_.store(this, value, mode);
}
bool IrRegExpData::has_latin1_bytecode() const {
  return !latin1_bytecode_.load().is_null();
}
void IrRegExpData::clear_latin1_bytecode() {
  latin1_bytecode_.store(this, {}, SKIP_WRITE_BARRIER);
}

Tagged<TrustedByteArray> IrRegExpData::uc16_bytecode() const {
  return uc16_bytecode_.load();
}
void IrRegExpData::set_uc16_bytecode(Tagged<TrustedByteArray> value,
                                     WriteBarrierMode mode) {
  uc16_bytecode_.store(this, value, mode);
}
bool IrRegExpData::has_uc16_bytecode() const {
  return !uc16_bytecode_.load().is_null();
}
void IrRegExpData::clear_uc16_bytecode() {
  uc16_bytecode_.store(this, {}, SKIP_WRITE_BARRIER);
}

Tagged<TrustedFixedArray> IrRegExpData::capture_name_map() const {
  return capture_name_map_.load();
}
void IrRegExpData::set_capture_name_map(Tagged<TrustedFixedArray> value,
                                        WriteBarrierMode mode) {
  capture_name_map_.store(this, value, mode);
}
void IrRegExpData::set_capture_name_map(DirectHandle<TrustedFixedArray> value) {
  if (value.is_null()) {
    clear_capture_name_map();
  } else {
    set_capture_name_map(*value);
  }
}
bool IrRegExpData::has_capture_name_map() const {
  return !capture_name_map_.load().is_null();
}
void IrRegExpData::clear_capture_name_map() {
  capture_name_map_.store(this, {}, SKIP_WRITE_BARRIER);
}

bool IrRegExpData::has_bytecode(bool is_one_byte) const {
  return is_one_byte ? has_latin1_bytecode() : has_uc16_bytecode();
}
void IrRegExpData::clear_bytecode(bool is_one_byte) {
  if (is_one_byte) {
    clear_latin1_bytecode();
  } else {
    clear_uc16_bytecode();
  }
}
void IrRegExpData::set_bytecode(bool is_one_byte,
                                Tagged<TrustedByteArray> bytecode) {
  if (is_one_byte) {
    set_latin1_bytecode(bytecode);
  } else {
    set_uc16_bytecode(bytecode);
  }
}
Tagged<TrustedByteArray> IrRegExpData::bytecode(bool is_one_byte) const {
  return is_one_byte ? latin1_bytecode() : uc16_bytecode();
}

int IrRegExpData::max_register_count() const {
  return max_register_count_.load().value();
}
void IrRegExpData::set_max_register_count(int value) {
  max_register_count_.store(this, Smi::FromInt(value));
}
int IrRegExpData::capture_count() const {
  return capture_count_.load().value();
}
void IrRegExpData::set_capture_count(int value) {
  capture_count_.store(this, Smi::FromInt(value));
}
int IrRegExpData::ticks_until_tier_up() const {
  return ticks_until_tier_up_.load().value();
}
void IrRegExpData::set_ticks_until_tier_up(int value) {
  ticks_until_tier_up_.store(this, Smi::FromInt(value));
}
int IrRegExpData::backtrack_limit() const {
  return backtrack_limit_.load().value();
}
void IrRegExpData::set_backtrack_limit(int value) {
  backtrack_limit_.store(this, Smi::FromInt(value));
}

uint32_t IrRegExpData::bit_field() const { return bit_field_.load().value(); }
void IrRegExpData::set_bit_field(uint32_t value) {
  bit_field_.store(this, Smi::FromInt(value));
}

BOOL_ACCESSORS(IrRegExpData, bit_field, can_be_zero_length,
               Bits::CanBeZeroLengthBit::kShift)
BOOL_ACCESSORS(IrRegExpData, bit_field, is_linear_executable,
               Bits::IsLinearExecutableBit::kShift)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_REGEXP_INL_H_
