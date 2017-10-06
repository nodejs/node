// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/ic-state.h"

#include "src/feedback-vector.h"
#include "src/ic/ic.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

// static
void ICUtility::Clear(Isolate* isolate, Address address,
                      Address constant_pool) {
  IC::Clear(isolate, address, constant_pool);
}


const char* CompareICState::GetStateName(State state) {
  switch (state) {
    case UNINITIALIZED:
      return "UNINITIALIZED";
    case BOOLEAN:
      return "BOOLEAN";
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
    case RECEIVER:
      return "RECEIVER";
    case KNOWN_RECEIVER:
      return "KNOWN_RECEIVER";
    case GENERIC:
      return "GENERIC";
  }
  UNREACHABLE();
}


CompareICState::State CompareICState::NewInputState(State old_state,
                                                    Handle<Object> value) {
  switch (old_state) {
    case UNINITIALIZED:
      if (value->IsBoolean()) return BOOLEAN;
      if (value->IsSmi()) return SMI;
      if (value->IsHeapNumber()) return NUMBER;
      if (value->IsInternalizedString()) return INTERNALIZED_STRING;
      if (value->IsString()) return STRING;
      if (value->IsSymbol()) return UNIQUE_NAME;
      if (value->IsJSReceiver() && !value->IsUndetectable()) {
        return RECEIVER;
      }
      break;
    case BOOLEAN:
      if (value->IsBoolean()) return BOOLEAN;
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
    case RECEIVER:
      if (value->IsJSReceiver() && !value->IsUndetectable()) {
        return RECEIVER;
      }
      break;
    case GENERIC:
      break;
    case KNOWN_RECEIVER:
      UNREACHABLE();
      break;
  }
  return GENERIC;
}


// static
CompareICState::State CompareICState::TargetState(
    Isolate* isolate, State old_state, State old_left, State old_right,
    Token::Value op, bool has_inlined_smi_code, Handle<Object> x,
    Handle<Object> y) {
  switch (old_state) {
    case UNINITIALIZED:
      if (x->IsBoolean() && y->IsBoolean()) return BOOLEAN;
      if (x->IsSmi() && y->IsSmi()) return SMI;
      if (x->IsNumber() && y->IsNumber()) return NUMBER;
      if (Token::IsOrderedRelationalCompareOp(op)) {
        // Ordered comparisons treat undefined as NaN, so the
        // NUMBER stub will do the right thing.
        if ((x->IsNumber() && y->IsUndefined(isolate)) ||
            (y->IsNumber() && x->IsUndefined(isolate))) {
          return NUMBER;
        }
      }
      if (x->IsInternalizedString() && y->IsInternalizedString()) {
        // We compare internalized strings as plain ones if we need to determine
        // the order in a non-equality compare.
        return Token::IsEqualityOp(op) ? INTERNALIZED_STRING : STRING;
      }
      if (x->IsString() && y->IsString()) return STRING;
      if (x->IsJSReceiver() && y->IsJSReceiver()) {
        if (x->IsUndetectable() || y->IsUndetectable()) {
          return GENERIC;
        }
        if (Handle<JSReceiver>::cast(x)->map() ==
            Handle<JSReceiver>::cast(y)->map()) {
          return KNOWN_RECEIVER;
        } else {
          return Token::IsEqualityOp(op) ? RECEIVER : GENERIC;
        }
      }
      if (!Token::IsEqualityOp(op)) return GENERIC;
      if (x->IsUniqueName() && y->IsUniqueName()) return UNIQUE_NAME;
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
    case KNOWN_RECEIVER:
      if (x->IsJSReceiver() && y->IsJSReceiver()) {
        return Token::IsEqualityOp(op) ? RECEIVER : GENERIC;
      }
      return GENERIC;
    case BOOLEAN:
    case STRING:
    case UNIQUE_NAME:
    case RECEIVER:
    case GENERIC:
      return GENERIC;
  }
  UNREACHABLE();
}

}  // namespace internal
}  // namespace v8
