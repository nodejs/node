// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_STATE_H_
#define V8_IC_STATE_H_

#include "src/macro-assembler.h"
#include "src/parsing/token.h"

namespace v8 {
namespace internal {

const int kMaxKeyedPolymorphism = 4;


class ICUtility : public AllStatic {
 public:
  // Clear the inline cache to initial state.
  static void Clear(Isolate* isolate, Address address, Address constant_pool);
};


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
