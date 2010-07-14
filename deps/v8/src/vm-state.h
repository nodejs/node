// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_VM_STATE_H_
#define V8_VM_STATE_H_

namespace v8 {
namespace internal {

class VMState BASE_EMBEDDED {
#ifdef ENABLE_VMSTATE_TRACKING
 public:
  inline VMState(StateTag state);
  inline ~VMState();

  StateTag state() { return state_; }
  void set_external_callback(Address external_callback) {
    external_callback_ = external_callback;
  }

  // Used for debug asserts.
  static bool is_outermost_external() {
    return current_state_ == 0;
  }

  static StateTag current_state() {
    VMState* state = reinterpret_cast<VMState*>(current_state_);
    return state ? state->state() : EXTERNAL;
  }

  static Address external_callback() {
    VMState* state = reinterpret_cast<VMState*>(current_state_);
    return state ? state->external_callback_ : NULL;
  }

 private:
  bool disabled_;
  StateTag state_;
  VMState* previous_;
  Address external_callback_;

  // A stack of VM states.
  static AtomicWord current_state_;
#else
 public:
  explicit VMState(StateTag state) {}
#endif
};

} }  // namespace v8::internal


#endif  // V8_VM_STATE_H_
