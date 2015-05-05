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
  static void Clear(Isolate* isolate, Address address,
                    ConstantPoolArray* constant_pool);
};


class CallICState FINAL BASE_EMBEDDED {
 public:
  explicit CallICState(ExtraICState extra_ic_state);

  enum CallType { METHOD, FUNCTION };

  CallICState(int argc, CallType call_type)
      : argc_(argc), call_type_(call_type) {}

  ExtraICState GetExtraICState() const;

  static void GenerateAheadOfTime(Isolate*,
                                  void (*Generate)(Isolate*,
                                                   const CallICState&));

  int arg_count() const { return argc_; }
  CallType call_type() const { return call_type_; }

  bool CallAsMethod() const { return call_type_ == METHOD; }

 private:
  class ArgcBits : public BitField<int, 0, Code::kArgumentsBits> {};
  class CallTypeBits : public BitField<CallType, Code::kArgumentsBits, 1> {};

  const int argc_;
  const CallType call_type_;
};


std::ostream& operator<<(std::ostream& os, const CallICState& s);


class BinaryOpICState FINAL BASE_EMBEDDED {
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

  Type* GetLeftType(Zone* zone) const { return KindToType(left_kind_, zone); }
  Type* GetRightType(Zone* zone) const { return KindToType(right_kind_, zone); }
  Type* GetResultType(Zone* zone) const;

  void Update(Handle<Object> left, Handle<Object> right, Handle<Object> result);

  Isolate* isolate() const { return isolate_; }

 private:
  friend std::ostream& operator<<(std::ostream& os, const BinaryOpICState& s);

  enum Kind { NONE, SMI, INT32, NUMBER, STRING, GENERIC };

  Kind UpdateKind(Handle<Object> object, Kind kind) const;

  static const char* KindToString(Kind kind);
  static Type* KindToType(Kind kind, Zone* zone);
  static bool KindMaybeSmi(Kind kind) {
    return (kind >= SMI && kind <= NUMBER) || kind == GENERIC;
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
  //   KNOWN_OBJECT < OBJECT
  enum State {
    UNINITIALIZED,
    SMI,
    NUMBER,
    STRING,
    INTERNALIZED_STRING,
    UNIQUE_NAME,   // Symbol or InternalizedString
    OBJECT,        // JSObject
    KNOWN_OBJECT,  // JSObject with specific map (faster check)
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


class LoadICState FINAL BASE_EMBEDDED {
 public:
  explicit LoadICState(ExtraICState extra_ic_state) : state_(extra_ic_state) {}

  explicit LoadICState(ContextualMode mode)
      : state_(ContextualModeBits::encode(mode)) {}

  ExtraICState GetExtraICState() const { return state_; }

  ContextualMode contextual_mode() const {
    return ContextualModeBits::decode(state_);
  }

  static ContextualMode GetContextualMode(ExtraICState state) {
    return LoadICState(state).contextual_mode();
  }

 private:
  class ContextualModeBits : public BitField<ContextualMode, 0, 1> {};
  STATIC_ASSERT(static_cast<int>(NOT_CONTEXTUAL) == 0);

  const ExtraICState state_;
};
}
}

#endif  // V8_IC_STATE_H_
