// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_STATE_H_
#define V8_IC_STATE_H_

#include "src/macro-assembler.h"

namespace v8 {
namespace internal {


const int kMaxKeyedPolymorphism = 4;


class ICUtility : public AllStatic {
 public:
  // Clear the inline cache to initial state.
  static void Clear(Isolate* isolate, Address address, Address constant_pool);
};


class CallICState final BASE_EMBEDDED {
 public:
  explicit CallICState(ExtraICState extra_ic_state)
      : bit_field_(extra_ic_state) {}
  CallICState(int argc, ConvertReceiverMode convert_mode)
      : bit_field_(ArgcBits::encode(argc) |
                   ConvertModeBits::encode(convert_mode)) {}

  ExtraICState GetExtraICState() const { return bit_field_; }

  static void GenerateAheadOfTime(Isolate*,
                                  void (*Generate)(Isolate*,
                                                   const CallICState&));

  int argc() const { return ArgcBits::decode(bit_field_); }
  ConvertReceiverMode convert_mode() const {
    return ConvertModeBits::decode(bit_field_);
  }

 private:
  typedef BitField<int, 0, Code::kArgumentsBits> ArgcBits;
  typedef BitField<ConvertReceiverMode, Code::kArgumentsBits, 2>
      ConvertModeBits;

  int const bit_field_;
};


std::ostream& operator<<(std::ostream& os, const CallICState& s);


class BinaryOpICState final BASE_EMBEDDED {
 public:
  BinaryOpICState(Isolate* isolate, ExtraICState extra_ic_state);
  BinaryOpICState(Isolate* isolate, Token::Value op, Strength strength)
      : op_(op),
        strong_(is_strong(strength)),
        left_kind_(NONE),
        right_kind_(NONE),
        result_kind_(NONE),
        fixed_right_arg_(Nothing<int>()),
        isolate_(isolate) {
    DCHECK_LE(FIRST_TOKEN, op);
    DCHECK_LE(op, LAST_TOKEN);
  }

  InlineCacheState GetICState() const {
    if (Max(left_kind_, right_kind_) == NONE) {
      return ::v8::internal::UNINITIALIZED;
    }
    if (Max(left_kind_, right_kind_) == GENERIC) {
      return ::v8::internal::MEGAMORPHIC;
    }
    if (Min(left_kind_, right_kind_) == GENERIC) {
      return ::v8::internal::GENERIC;
    }
    return ::v8::internal::MONOMORPHIC;
  }

  ExtraICState GetExtraICState() const;

  static void GenerateAheadOfTime(Isolate*,
                                  void (*Generate)(Isolate*,
                                                   const BinaryOpICState&));

  // Returns true if the IC _could_ create allocation mementos.
  bool CouldCreateAllocationMementos() const {
    if (left_kind_ == STRING || right_kind_ == STRING) {
      DCHECK_EQ(Token::ADD, op_);
      return true;
    }
    return false;
  }

  // Returns true if the IC _should_ create allocation mementos.
  bool ShouldCreateAllocationMementos() const {
    return FLAG_allocation_site_pretenuring && CouldCreateAllocationMementos();
  }

  bool HasSideEffects() const {
    return Max(left_kind_, right_kind_) == GENERIC;
  }

  Strength strength() const {
    return strong_ ? Strength::STRONG : Strength::WEAK;
  }

  // Returns true if the IC should enable the inline smi code (i.e. if either
  // parameter may be a smi).
  bool UseInlinedSmiCode() const {
    return KindMaybeSmi(left_kind_) || KindMaybeSmi(right_kind_);
  }

  static const int FIRST_TOKEN = Token::BIT_OR;
  static const int LAST_TOKEN = Token::MOD;

  Token::Value op() const { return op_; }
  Maybe<int> fixed_right_arg() const { return fixed_right_arg_; }

  Type* GetLeftType() const { return KindToType(left_kind_); }
  Type* GetRightType() const { return KindToType(right_kind_); }
  Type* GetResultType() const;

  void Update(Handle<Object> left, Handle<Object> right, Handle<Object> result);

  Isolate* isolate() const { return isolate_; }

 private:
  friend std::ostream& operator<<(std::ostream& os, const BinaryOpICState& s);

  enum Kind { NONE, SMI, INT32, NUMBER, STRING, GENERIC };

  Kind UpdateKind(Handle<Object> object, Kind kind) const;

  static const char* KindToString(Kind kind);
  static Type* KindToType(Kind kind);
  static bool KindMaybeSmi(Kind kind) {
    return (kind >= SMI && kind <= NUMBER) || kind == GENERIC;
  }

  // We truncate the last bit of the token.
  STATIC_ASSERT(LAST_TOKEN - FIRST_TOKEN < (1 << 4));
  class OpField : public BitField<int, 0, 4> {};
  class ResultKindField : public BitField<Kind, 4, 3> {};
  class LeftKindField : public BitField<Kind, 7, 3> {};
  class StrengthField : public BitField<bool, 10, 1> {};
  // When fixed right arg is set, we don't need to store the right kind.
  // Thus the two fields can overlap.
  class HasFixedRightArgField : public BitField<bool, 11, 1> {};
  class FixedRightArgValueField : public BitField<int, 12, 4> {};
  class RightKindField : public BitField<Kind, 12, 3> {};

  Token::Value op_;
  bool strong_;
  Kind left_kind_;
  Kind right_kind_;
  Kind result_kind_;
  Maybe<int> fixed_right_arg_;
  Isolate* isolate_;
};


std::ostream& operator<<(std::ostream& os, const BinaryOpICState& s);


class CompareICState {
 public:
  // The type/state lattice is defined by the following inequations:
  //   UNINITIALIZED < ...
  //   ... < GENERIC
  //   SMI < NUMBER
  //   INTERNALIZED_STRING < STRING
  //   INTERNALIZED_STRING < UNIQUE_NAME
  //   KNOWN_RECEIVER < RECEIVER
  enum State {
    UNINITIALIZED,
    BOOLEAN,
    SMI,
    NUMBER,
    STRING,
    INTERNALIZED_STRING,
    UNIQUE_NAME,     // Symbol or InternalizedString
    RECEIVER,        // JSReceiver
    KNOWN_RECEIVER,  // JSReceiver with specific map (faster check)
    GENERIC
  };

  static Type* StateToType(Zone* zone, State state,
                           Handle<Map> map = Handle<Map>());

  static State NewInputState(State old_state, Handle<Object> value);

  static const char* GetStateName(CompareICState::State state);

  static State TargetState(State old_state, State old_left, State old_right,
                           Token::Value op, bool has_inlined_smi_code,
                           Handle<Object> x, Handle<Object> y);
};


class LoadICState final BASE_EMBEDDED {
 private:
  class TypeofModeBits : public BitField<TypeofMode, 0, 1> {};
  class LanguageModeBits
      : public BitField<LanguageMode, TypeofModeBits::kNext, 2> {};
  STATIC_ASSERT(static_cast<int>(INSIDE_TYPEOF) == 0);
  const ExtraICState state_;

 public:
  static const uint32_t kNextBitFieldOffset = LanguageModeBits::kNext;

  static const ExtraICState kStrongModeState = STRONG
                                               << LanguageModeBits::kShift;

  explicit LoadICState(ExtraICState extra_ic_state) : state_(extra_ic_state) {}

  explicit LoadICState(TypeofMode typeof_mode, LanguageMode language_mode)
      : state_(TypeofModeBits::encode(typeof_mode) |
               LanguageModeBits::encode(language_mode)) {}

  ExtraICState GetExtraICState() const { return state_; }

  TypeofMode typeof_mode() const { return TypeofModeBits::decode(state_); }

  LanguageMode language_mode() const {
    return LanguageModeBits::decode(state_);
  }

  static TypeofMode GetTypeofMode(ExtraICState state) {
    return LoadICState(state).typeof_mode();
  }

  static LanguageMode GetLanguageMode(ExtraICState state) {
    return LoadICState(state).language_mode();
  }
};


class StoreICState final BASE_EMBEDDED {
 public:
  explicit StoreICState(ExtraICState extra_ic_state) : state_(extra_ic_state) {}

  explicit StoreICState(LanguageMode mode)
      : state_(LanguageModeState::encode(mode)) {}

  ExtraICState GetExtraICState() const { return state_; }

  LanguageMode language_mode() const {
    return LanguageModeState::decode(state_);
  }

  static LanguageMode GetLanguageMode(ExtraICState state) {
    return StoreICState(state).language_mode();
  }

  class LanguageModeState : public BitField<LanguageMode, 1, 2> {};
  STATIC_ASSERT(i::LANGUAGE_END == 3);

  // For convenience, a statically declared encoding of strict mode extra
  // IC state.
  static const ExtraICState kStrictModeState = STRICT
                                               << LanguageModeState::kShift;

 private:
  const ExtraICState state_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_IC_STATE_H_
