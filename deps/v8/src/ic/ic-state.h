// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_STATE_H_
#define V8_IC_STATE_H_

#include "src/macro-assembler.h"
#include "src/parsing/token.h"

namespace v8 {
namespace internal {

class AstType;

const int kMaxKeyedPolymorphism = 4;


class ICUtility : public AllStatic {
 public:
  // Clear the inline cache to initial state.
  static void Clear(Isolate* isolate, Address address, Address constant_pool);
};


class BinaryOpICState final BASE_EMBEDDED {
 public:
  BinaryOpICState(Isolate* isolate, ExtraICState extra_ic_state);
  BinaryOpICState(Isolate* isolate, Token::Value op)
      : op_(op),
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
  std::string ToString() const;

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

  // Returns true if the IC should enable the inline smi code (i.e. if either
  // parameter may be a smi).
  bool UseInlinedSmiCode() const {
    return KindMaybeSmi(left_kind_) || KindMaybeSmi(right_kind_);
  }

  static const int FIRST_TOKEN = Token::BIT_OR;
  static const int LAST_TOKEN = Token::MOD;

  Token::Value op() const { return op_; }
  Maybe<int> fixed_right_arg() const { return fixed_right_arg_; }

  AstType* GetLeftType() const { return KindToType(left_kind_); }
  AstType* GetRightType() const { return KindToType(right_kind_); }
  AstType* GetResultType() const;

  void Update(Handle<Object> left, Handle<Object> right, Handle<Object> result);

  Isolate* isolate() const { return isolate_; }

  enum Kind { NONE, SMI, INT32, NUMBER, STRING, GENERIC };
  Kind kind() const {
    return KindGeneralize(KindGeneralize(left_kind_, right_kind_),
                          result_kind_);
  }

 private:
  friend std::ostream& operator<<(std::ostream& os, const BinaryOpICState& s);

  Kind UpdateKind(Handle<Object> object, Kind kind) const;

  static const char* KindToString(Kind kind);
  static AstType* KindToType(Kind kind);
  static bool KindMaybeSmi(Kind kind) {
    return (kind >= SMI && kind <= NUMBER) || kind == GENERIC;
  }
  static bool KindLessGeneralThan(Kind kind1, Kind kind2) {
    if (kind1 == NONE) return true;
    if (kind1 == kind2) return true;
    if (kind2 == GENERIC) return true;
    if (kind2 == STRING) return false;
    return kind1 <= kind2;
  }
  static Kind KindGeneralize(Kind kind1, Kind kind2) {
    if (KindLessGeneralThan(kind1, kind2)) return kind2;
    if (KindLessGeneralThan(kind2, kind1)) return kind1;
    return GENERIC;
  }

  // We truncate the last bit of the token.
  STATIC_ASSERT(LAST_TOKEN - FIRST_TOKEN < (1 << 4));
  class OpField : public BitField<int, 0, 4> {};
  class ResultKindField : public BitField<Kind, 4, 3> {};
  class LeftKindField : public BitField<Kind, 7, 3> {};
  // When fixed right arg is set, we don't need to store the right kind.
  // Thus the two fields can overlap.
  class HasFixedRightArgField : public BitField<bool, 10, 1> {};
  class FixedRightArgValueField : public BitField<int, 11, 4> {};
  class RightKindField : public BitField<Kind, 11, 3> {};

  Token::Value op_;
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

  static AstType* StateToType(Zone* zone, State state,
                              Handle<Map> map = Handle<Map>());

  static State NewInputState(State old_state, Handle<Object> value);

  static const char* GetStateName(CompareICState::State state);

  static State TargetState(Isolate* isolate, State old_state, State old_left,
                           State old_right, Token::Value op,
                           bool has_inlined_smi_code, Handle<Object> x,
                           Handle<Object> y);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_IC_STATE_H_
