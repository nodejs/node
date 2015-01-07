// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/ic/ic.h"
#include "src/ic/ic-state.h"

namespace v8 {
namespace internal {

// static
void ICUtility::Clear(Isolate* isolate, Address address,
                      ConstantPoolArray* constant_pool) {
  IC::Clear(isolate, address, constant_pool);
}


CallICState::CallICState(ExtraICState extra_ic_state)
    : argc_(ArgcBits::decode(extra_ic_state)),
      call_type_(CallTypeBits::decode(extra_ic_state)) {}


ExtraICState CallICState::GetExtraICState() const {
  ExtraICState extra_ic_state =
      ArgcBits::encode(argc_) | CallTypeBits::encode(call_type_);
  return extra_ic_state;
}


std::ostream& operator<<(std::ostream& os, const CallICState& s) {
  return os << "(args(" << s.arg_count() << "), "
            << (s.call_type() == CallICState::METHOD ? "METHOD" : "FUNCTION")
            << ", ";
}


BinaryOpICState::BinaryOpICState(Isolate* isolate, ExtraICState extra_ic_state)
    : isolate_(isolate) {
  op_ =
      static_cast<Token::Value>(FIRST_TOKEN + OpField::decode(extra_ic_state));
  mode_ = OverwriteModeField::decode(extra_ic_state);
  fixed_right_arg_ =
      Maybe<int>(HasFixedRightArgField::decode(extra_ic_state),
                 1 << FixedRightArgValueField::decode(extra_ic_state));
  left_kind_ = LeftKindField::decode(extra_ic_state);
  if (fixed_right_arg_.has_value) {
    right_kind_ = Smi::IsValid(fixed_right_arg_.value) ? SMI : INT32;
  } else {
    right_kind_ = RightKindField::decode(extra_ic_state);
  }
  result_kind_ = ResultKindField::decode(extra_ic_state);
  DCHECK_LE(FIRST_TOKEN, op_);
  DCHECK_LE(op_, LAST_TOKEN);
}


ExtraICState BinaryOpICState::GetExtraICState() const {
  ExtraICState extra_ic_state =
      OpField::encode(op_ - FIRST_TOKEN) | OverwriteModeField::encode(mode_) |
      LeftKindField::encode(left_kind_) |
      ResultKindField::encode(result_kind_) |
      HasFixedRightArgField::encode(fixed_right_arg_.has_value);
  if (fixed_right_arg_.has_value) {
    extra_ic_state = FixedRightArgValueField::update(
        extra_ic_state, WhichPowerOf2(fixed_right_arg_.value));
  } else {
    extra_ic_state = RightKindField::update(extra_ic_state, right_kind_);
  }
  return extra_ic_state;
}


// static
void BinaryOpICState::GenerateAheadOfTime(
    Isolate* isolate, void (*Generate)(Isolate*, const BinaryOpICState&)) {
// TODO(olivf) We should investigate why adding stubs to the snapshot is so
// expensive at runtime. When solved we should be able to add most binops to
// the snapshot instead of hand-picking them.
// Generated list of commonly used stubs
#define GENERATE(op, left_kind, right_kind, result_kind, mode) \
  do {                                                         \
    BinaryOpICState state(isolate, op, mode);                  \
    state.left_kind_ = left_kind;                              \
    state.fixed_right_arg_.has_value = false;                  \
    state.right_kind_ = right_kind;                            \
    state.result_kind_ = result_kind;                          \
    Generate(isolate, state);                                  \
  } while (false)
  GENERATE(Token::ADD, INT32, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::ADD, INT32, INT32, INT32, OVERWRITE_LEFT);
  GENERATE(Token::ADD, INT32, INT32, NUMBER, NO_OVERWRITE);
  GENERATE(Token::ADD, INT32, INT32, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::ADD, INT32, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::ADD, INT32, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::ADD, INT32, NUMBER, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::ADD, INT32, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::ADD, INT32, SMI, INT32, OVERWRITE_LEFT);
  GENERATE(Token::ADD, INT32, SMI, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::ADD, NUMBER, INT32, NUMBER, NO_OVERWRITE);
  GENERATE(Token::ADD, NUMBER, INT32, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::ADD, NUMBER, INT32, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::ADD, NUMBER, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::ADD, NUMBER, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::ADD, NUMBER, NUMBER, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::ADD, NUMBER, SMI, NUMBER, NO_OVERWRITE);
  GENERATE(Token::ADD, NUMBER, SMI, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::ADD, NUMBER, SMI, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::ADD, SMI, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::ADD, SMI, INT32, INT32, OVERWRITE_LEFT);
  GENERATE(Token::ADD, SMI, INT32, NUMBER, NO_OVERWRITE);
  GENERATE(Token::ADD, SMI, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::ADD, SMI, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::ADD, SMI, NUMBER, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::ADD, SMI, SMI, INT32, OVERWRITE_LEFT);
  GENERATE(Token::ADD, SMI, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_AND, INT32, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::BIT_AND, INT32, INT32, INT32, OVERWRITE_LEFT);
  GENERATE(Token::BIT_AND, INT32, INT32, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_AND, INT32, INT32, SMI, NO_OVERWRITE);
  GENERATE(Token::BIT_AND, INT32, INT32, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_AND, INT32, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::BIT_AND, INT32, SMI, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_AND, INT32, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::BIT_AND, INT32, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::BIT_AND, INT32, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_AND, NUMBER, INT32, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_AND, NUMBER, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::BIT_AND, NUMBER, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_AND, SMI, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::BIT_AND, SMI, INT32, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_AND, SMI, NUMBER, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_AND, SMI, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::BIT_AND, SMI, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::BIT_AND, SMI, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_OR, INT32, INT32, INT32, OVERWRITE_LEFT);
  GENERATE(Token::BIT_OR, INT32, INT32, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_OR, INT32, INT32, SMI, OVERWRITE_LEFT);
  GENERATE(Token::BIT_OR, INT32, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::BIT_OR, INT32, SMI, INT32, OVERWRITE_LEFT);
  GENERATE(Token::BIT_OR, INT32, SMI, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_OR, INT32, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::BIT_OR, INT32, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_OR, NUMBER, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::BIT_OR, NUMBER, SMI, INT32, OVERWRITE_LEFT);
  GENERATE(Token::BIT_OR, NUMBER, SMI, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_OR, NUMBER, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::BIT_OR, NUMBER, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::BIT_OR, SMI, INT32, INT32, OVERWRITE_LEFT);
  GENERATE(Token::BIT_OR, SMI, INT32, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_OR, SMI, INT32, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_OR, SMI, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::BIT_OR, SMI, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_XOR, INT32, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::BIT_XOR, INT32, INT32, INT32, OVERWRITE_LEFT);
  GENERATE(Token::BIT_XOR, INT32, INT32, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_XOR, INT32, INT32, SMI, NO_OVERWRITE);
  GENERATE(Token::BIT_XOR, INT32, INT32, SMI, OVERWRITE_LEFT);
  GENERATE(Token::BIT_XOR, INT32, NUMBER, SMI, NO_OVERWRITE);
  GENERATE(Token::BIT_XOR, INT32, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::BIT_XOR, INT32, SMI, INT32, OVERWRITE_LEFT);
  GENERATE(Token::BIT_XOR, INT32, SMI, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_XOR, NUMBER, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::BIT_XOR, NUMBER, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::BIT_XOR, NUMBER, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::BIT_XOR, SMI, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::BIT_XOR, SMI, INT32, INT32, OVERWRITE_LEFT);
  GENERATE(Token::BIT_XOR, SMI, INT32, SMI, OVERWRITE_LEFT);
  GENERATE(Token::BIT_XOR, SMI, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::BIT_XOR, SMI, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::BIT_XOR, SMI, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::DIV, INT32, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::DIV, INT32, INT32, NUMBER, NO_OVERWRITE);
  GENERATE(Token::DIV, INT32, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::DIV, INT32, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::DIV, INT32, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::DIV, INT32, SMI, NUMBER, NO_OVERWRITE);
  GENERATE(Token::DIV, NUMBER, INT32, NUMBER, NO_OVERWRITE);
  GENERATE(Token::DIV, NUMBER, INT32, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::DIV, NUMBER, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::DIV, NUMBER, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::DIV, NUMBER, NUMBER, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::DIV, NUMBER, SMI, NUMBER, NO_OVERWRITE);
  GENERATE(Token::DIV, NUMBER, SMI, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::DIV, SMI, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::DIV, SMI, INT32, NUMBER, NO_OVERWRITE);
  GENERATE(Token::DIV, SMI, INT32, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::DIV, SMI, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::DIV, SMI, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::DIV, SMI, NUMBER, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::DIV, SMI, SMI, NUMBER, NO_OVERWRITE);
  GENERATE(Token::DIV, SMI, SMI, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::DIV, SMI, SMI, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::DIV, SMI, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::DIV, SMI, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::DIV, SMI, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::MOD, NUMBER, SMI, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::MOD, SMI, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::MOD, SMI, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::MUL, INT32, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::MUL, INT32, INT32, NUMBER, NO_OVERWRITE);
  GENERATE(Token::MUL, INT32, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::MUL, INT32, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::MUL, INT32, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::MUL, INT32, SMI, INT32, OVERWRITE_LEFT);
  GENERATE(Token::MUL, INT32, SMI, NUMBER, NO_OVERWRITE);
  GENERATE(Token::MUL, NUMBER, INT32, NUMBER, NO_OVERWRITE);
  GENERATE(Token::MUL, NUMBER, INT32, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::MUL, NUMBER, INT32, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::MUL, NUMBER, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::MUL, NUMBER, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::MUL, NUMBER, SMI, NUMBER, NO_OVERWRITE);
  GENERATE(Token::MUL, NUMBER, SMI, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::MUL, NUMBER, SMI, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::MUL, SMI, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::MUL, SMI, INT32, INT32, OVERWRITE_LEFT);
  GENERATE(Token::MUL, SMI, INT32, NUMBER, NO_OVERWRITE);
  GENERATE(Token::MUL, SMI, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::MUL, SMI, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::MUL, SMI, NUMBER, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::MUL, SMI, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::MUL, SMI, SMI, NUMBER, NO_OVERWRITE);
  GENERATE(Token::MUL, SMI, SMI, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::MUL, SMI, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::MUL, SMI, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::MUL, SMI, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::SAR, INT32, SMI, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::SAR, INT32, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::SAR, INT32, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::SAR, NUMBER, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::SAR, NUMBER, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::SAR, SMI, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::SAR, SMI, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::SHL, INT32, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::SHL, INT32, SMI, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::SHL, INT32, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::SHL, INT32, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::SHL, NUMBER, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::SHL, SMI, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::SHL, SMI, SMI, INT32, OVERWRITE_LEFT);
  GENERATE(Token::SHL, SMI, SMI, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::SHL, SMI, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::SHL, SMI, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::SHL, SMI, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::SHR, INT32, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::SHR, INT32, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::SHR, INT32, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::SHR, NUMBER, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::SHR, NUMBER, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::SHR, NUMBER, SMI, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::SHR, SMI, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::SHR, SMI, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::SHR, SMI, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::SUB, INT32, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::SUB, INT32, INT32, INT32, OVERWRITE_LEFT);
  GENERATE(Token::SUB, INT32, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::SUB, INT32, NUMBER, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::SUB, INT32, SMI, INT32, OVERWRITE_LEFT);
  GENERATE(Token::SUB, INT32, SMI, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::SUB, NUMBER, INT32, NUMBER, NO_OVERWRITE);
  GENERATE(Token::SUB, NUMBER, INT32, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::SUB, NUMBER, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::SUB, NUMBER, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::SUB, NUMBER, NUMBER, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::SUB, NUMBER, SMI, NUMBER, NO_OVERWRITE);
  GENERATE(Token::SUB, NUMBER, SMI, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::SUB, NUMBER, SMI, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::SUB, SMI, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::SUB, SMI, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::SUB, SMI, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::SUB, SMI, NUMBER, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::SUB, SMI, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::SUB, SMI, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::SUB, SMI, SMI, SMI, OVERWRITE_RIGHT);
#undef GENERATE
#define GENERATE(op, left_kind, fixed_right_arg_value, result_kind, mode) \
  do {                                                                    \
    BinaryOpICState state(isolate, op, mode);                             \
    state.left_kind_ = left_kind;                                         \
    state.fixed_right_arg_.has_value = true;                              \
    state.fixed_right_arg_.value = fixed_right_arg_value;                 \
    state.right_kind_ = SMI;                                              \
    state.result_kind_ = result_kind;                                     \
    Generate(isolate, state);                                             \
  } while (false)
  GENERATE(Token::MOD, SMI, 2, SMI, NO_OVERWRITE);
  GENERATE(Token::MOD, SMI, 4, SMI, NO_OVERWRITE);
  GENERATE(Token::MOD, SMI, 4, SMI, OVERWRITE_LEFT);
  GENERATE(Token::MOD, SMI, 8, SMI, NO_OVERWRITE);
  GENERATE(Token::MOD, SMI, 16, SMI, OVERWRITE_LEFT);
  GENERATE(Token::MOD, SMI, 32, SMI, NO_OVERWRITE);
  GENERATE(Token::MOD, SMI, 2048, SMI, NO_OVERWRITE);
#undef GENERATE
}


Type* BinaryOpICState::GetResultType(Zone* zone) const {
  Kind result_kind = result_kind_;
  if (HasSideEffects()) {
    result_kind = NONE;
  } else if (result_kind == GENERIC && op_ == Token::ADD) {
    return Type::Union(Type::Number(zone), Type::String(zone), zone);
  } else if (result_kind == NUMBER && op_ == Token::SHR) {
    return Type::Unsigned32(zone);
  }
  DCHECK_NE(GENERIC, result_kind);
  return KindToType(result_kind, zone);
}


std::ostream& operator<<(std::ostream& os, const BinaryOpICState& s) {
  os << "(" << Token::Name(s.op_);
  if (s.mode_ == OVERWRITE_LEFT)
    os << "_ReuseLeft";
  else if (s.mode_ == OVERWRITE_RIGHT)
    os << "_ReuseRight";
  if (s.CouldCreateAllocationMementos()) os << "_CreateAllocationMementos";
  os << ":" << BinaryOpICState::KindToString(s.left_kind_) << "*";
  if (s.fixed_right_arg_.has_value) {
    os << s.fixed_right_arg_.value;
  } else {
    os << BinaryOpICState::KindToString(s.right_kind_);
  }
  return os << "->" << BinaryOpICState::KindToString(s.result_kind_) << ")";
}


void BinaryOpICState::Update(Handle<Object> left, Handle<Object> right,
                             Handle<Object> result) {
  ExtraICState old_extra_ic_state = GetExtraICState();

  left_kind_ = UpdateKind(left, left_kind_);
  right_kind_ = UpdateKind(right, right_kind_);

  int32_t fixed_right_arg_value = 0;
  bool has_fixed_right_arg =
      op_ == Token::MOD && right->ToInt32(&fixed_right_arg_value) &&
      fixed_right_arg_value > 0 &&
      base::bits::IsPowerOfTwo32(fixed_right_arg_value) &&
      FixedRightArgValueField::is_valid(WhichPowerOf2(fixed_right_arg_value)) &&
      (left_kind_ == SMI || left_kind_ == INT32) &&
      (result_kind_ == NONE || !fixed_right_arg_.has_value);
  fixed_right_arg_ = Maybe<int32_t>(has_fixed_right_arg, fixed_right_arg_value);

  result_kind_ = UpdateKind(result, result_kind_);

  if (!Token::IsTruncatingBinaryOp(op_)) {
    Kind input_kind = Max(left_kind_, right_kind_);
    if (result_kind_ < input_kind && input_kind <= NUMBER) {
      result_kind_ = input_kind;
    }
  }

  // We don't want to distinguish INT32 and NUMBER for string add (because
  // NumberToString can't make use of this anyway).
  if (left_kind_ == STRING && right_kind_ == INT32) {
    DCHECK_EQ(STRING, result_kind_);
    DCHECK_EQ(Token::ADD, op_);
    right_kind_ = NUMBER;
  } else if (right_kind_ == STRING && left_kind_ == INT32) {
    DCHECK_EQ(STRING, result_kind_);
    DCHECK_EQ(Token::ADD, op_);
    left_kind_ = NUMBER;
  }

  // Reset overwrite mode unless we can actually make use of it, or may be able
  // to make use of it at some point in the future.
  if ((mode_ == OVERWRITE_LEFT && left_kind_ > NUMBER) ||
      (mode_ == OVERWRITE_RIGHT && right_kind_ > NUMBER) ||
      result_kind_ > NUMBER) {
    mode_ = NO_OVERWRITE;
  }

  if (old_extra_ic_state == GetExtraICState()) {
    // Tagged operations can lead to non-truncating HChanges
    if (left->IsUndefined() || left->IsBoolean()) {
      left_kind_ = GENERIC;
    } else {
      DCHECK(right->IsUndefined() || right->IsBoolean());
      right_kind_ = GENERIC;
    }
  }
}


BinaryOpICState::Kind BinaryOpICState::UpdateKind(Handle<Object> object,
                                                  Kind kind) const {
  Kind new_kind = GENERIC;
  bool is_truncating = Token::IsTruncatingBinaryOp(op());
  if (object->IsBoolean() && is_truncating) {
    // Booleans will be automatically truncated by HChange.
    new_kind = INT32;
  } else if (object->IsUndefined()) {
    // Undefined will be automatically truncated by HChange.
    new_kind = is_truncating ? INT32 : NUMBER;
  } else if (object->IsSmi()) {
    new_kind = SMI;
  } else if (object->IsHeapNumber()) {
    double value = Handle<HeapNumber>::cast(object)->value();
    new_kind = IsInt32Double(value) ? INT32 : NUMBER;
  } else if (object->IsString() && op() == Token::ADD) {
    new_kind = STRING;
  }
  if (new_kind == INT32 && SmiValuesAre32Bits()) {
    new_kind = NUMBER;
  }
  if (kind != NONE && ((new_kind <= NUMBER && kind > NUMBER) ||
                       (new_kind > NUMBER && kind <= NUMBER))) {
    new_kind = GENERIC;
  }
  return Max(kind, new_kind);
}


// static
const char* BinaryOpICState::KindToString(Kind kind) {
  switch (kind) {
    case NONE:
      return "None";
    case SMI:
      return "Smi";
    case INT32:
      return "Int32";
    case NUMBER:
      return "Number";
    case STRING:
      return "String";
    case GENERIC:
      return "Generic";
  }
  UNREACHABLE();
  return NULL;
}


// static
Type* BinaryOpICState::KindToType(Kind kind, Zone* zone) {
  switch (kind) {
    case NONE:
      return Type::None(zone);
    case SMI:
      return Type::SignedSmall(zone);
    case INT32:
      return Type::Signed32(zone);
    case NUMBER:
      return Type::Number(zone);
    case STRING:
      return Type::String(zone);
    case GENERIC:
      return Type::Any(zone);
  }
  UNREACHABLE();
  return NULL;
}


const char* CompareICState::GetStateName(State state) {
  switch (state) {
    case UNINITIALIZED:
      return "UNINITIALIZED";
    case SMI:
      return "SMI";
    case NUMBER:
      return "NUMBER";
    case INTERNALIZED_STRING:
      return "INTERNALIZED_STRING";
    case STRING:
      return "STRING";
    case UNIQUE_NAME:
      return "UNIQUE_NAME";
    case OBJECT:
      return "OBJECT";
    case KNOWN_OBJECT:
      return "KNOWN_OBJECT";
    case GENERIC:
      return "GENERIC";
  }
  UNREACHABLE();
  return NULL;
}


Type* CompareICState::StateToType(Zone* zone, State state, Handle<Map> map) {
  switch (state) {
    case UNINITIALIZED:
      return Type::None(zone);
    case SMI:
      return Type::SignedSmall(zone);
    case NUMBER:
      return Type::Number(zone);
    case STRING:
      return Type::String(zone);
    case INTERNALIZED_STRING:
      return Type::InternalizedString(zone);
    case UNIQUE_NAME:
      return Type::UniqueName(zone);
    case OBJECT:
      return Type::Receiver(zone);
    case KNOWN_OBJECT:
      return map.is_null() ? Type::Receiver(zone) : Type::Class(map, zone);
    case GENERIC:
      return Type::Any(zone);
  }
  UNREACHABLE();
  return NULL;
}


CompareICState::State CompareICState::NewInputState(State old_state,
                                                    Handle<Object> value) {
  switch (old_state) {
    case UNINITIALIZED:
      if (value->IsSmi()) return SMI;
      if (value->IsHeapNumber()) return NUMBER;
      if (value->IsInternalizedString()) return INTERNALIZED_STRING;
      if (value->IsString()) return STRING;
      if (value->IsSymbol()) return UNIQUE_NAME;
      if (value->IsJSObject()) return OBJECT;
      break;
    case SMI:
      if (value->IsSmi()) return SMI;
      if (value->IsHeapNumber()) return NUMBER;
      break;
    case NUMBER:
      if (value->IsNumber()) return NUMBER;
      break;
    case INTERNALIZED_STRING:
      if (value->IsInternalizedString()) return INTERNALIZED_STRING;
      if (value->IsString()) return STRING;
      if (value->IsSymbol()) return UNIQUE_NAME;
      break;
    case STRING:
      if (value->IsString()) return STRING;
      break;
    case UNIQUE_NAME:
      if (value->IsUniqueName()) return UNIQUE_NAME;
      break;
    case OBJECT:
      if (value->IsJSObject()) return OBJECT;
      break;
    case GENERIC:
      break;
    case KNOWN_OBJECT:
      UNREACHABLE();
      break;
  }
  return GENERIC;
}


// static
CompareICState::State CompareICState::TargetState(
    State old_state, State old_left, State old_right, Token::Value op,
    bool has_inlined_smi_code, Handle<Object> x, Handle<Object> y) {
  switch (old_state) {
    case UNINITIALIZED:
      if (x->IsSmi() && y->IsSmi()) return SMI;
      if (x->IsNumber() && y->IsNumber()) return NUMBER;
      if (Token::IsOrderedRelationalCompareOp(op)) {
        // Ordered comparisons treat undefined as NaN, so the
        // NUMBER stub will do the right thing.
        if ((x->IsNumber() && y->IsUndefined()) ||
            (y->IsNumber() && x->IsUndefined())) {
          return NUMBER;
        }
      }
      if (x->IsInternalizedString() && y->IsInternalizedString()) {
        // We compare internalized strings as plain ones if we need to determine
        // the order in a non-equality compare.
        return Token::IsEqualityOp(op) ? INTERNALIZED_STRING : STRING;
      }
      if (x->IsString() && y->IsString()) return STRING;
      if (!Token::IsEqualityOp(op)) return GENERIC;
      if (x->IsUniqueName() && y->IsUniqueName()) return UNIQUE_NAME;
      if (x->IsJSObject() && y->IsJSObject()) {
        if (Handle<JSObject>::cast(x)->map() ==
            Handle<JSObject>::cast(y)->map()) {
          return KNOWN_OBJECT;
        } else {
          return OBJECT;
        }
      }
      return GENERIC;
    case SMI:
      return x->IsNumber() && y->IsNumber() ? NUMBER : GENERIC;
    case INTERNALIZED_STRING:
      DCHECK(Token::IsEqualityOp(op));
      if (x->IsString() && y->IsString()) return STRING;
      if (x->IsUniqueName() && y->IsUniqueName()) return UNIQUE_NAME;
      return GENERIC;
    case NUMBER:
      // If the failure was due to one side changing from smi to heap number,
      // then keep the state (if other changed at the same time, we will get
      // a second miss and then go to generic).
      if (old_left == SMI && x->IsHeapNumber()) return NUMBER;
      if (old_right == SMI && y->IsHeapNumber()) return NUMBER;
      return GENERIC;
    case KNOWN_OBJECT:
      DCHECK(Token::IsEqualityOp(op));
      if (x->IsJSObject() && y->IsJSObject()) {
        return OBJECT;
      }
      return GENERIC;
    case STRING:
    case UNIQUE_NAME:
    case OBJECT:
    case GENERIC:
      return GENERIC;
  }
  UNREACHABLE();
  return GENERIC;  // Make the compiler happy.
}
}
}  // namespace v8::internal
