// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_POINTER_AUTHENTICATION_DUMMY_H_
#define V8_EXECUTION_POINTER_AUTHENTICATION_DUMMY_H_

#include "src/execution/pointer-authentication.h"

#include "include/v8.h"
#include "src/base/macros.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

// Dummy implementation of the PointerAuthentication class methods, to be used
// when CFI is not enabled.

// Load return address from {pc_address} and return it.
V8_INLINE Address PointerAuthentication::AuthenticatePC(
    Address* pc_address, unsigned offset_from_sp) {
  USE(offset_from_sp);
  return *pc_address;
}

// Return {pc} unmodified.
V8_INLINE Address PointerAuthentication::StripPAC(Address pc) { return pc; }

// Store {new_pc} to {pc_address} without signing.
V8_INLINE void PointerAuthentication::ReplacePC(Address* pc_address,
                                                Address new_pc,
                                                int offset_from_sp) {
  USE(offset_from_sp);
  *pc_address = new_pc;
}

// Return {pc} unmodified.
V8_INLINE Address PointerAuthentication::SignAndCheckPC(Address pc,
                                                        Address sp) {
  USE(sp);
  return pc;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_POINTER_AUTHENTICATION_DUMMY_H_
