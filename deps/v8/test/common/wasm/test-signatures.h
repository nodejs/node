// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TEST_SIGNATURES_H
#define TEST_SIGNATURES_H

#include "src/signature.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

typedef Signature<LocalType> FunctionSig;

// A helper class with many useful signatures in order to simplify tests.
class TestSignatures {
 public:
  TestSignatures()
      : sig_i_v(1, 0, kIntTypes4),
        sig_i_i(1, 1, kIntTypes4),
        sig_i_ii(1, 2, kIntTypes4),
        sig_i_iii(1, 3, kIntTypes4),
        sig_i_f(1, 1, kIntFloatTypes4),
        sig_i_ff(1, 2, kIntFloatTypes4),
        sig_i_d(1, 1, kIntDoubleTypes4),
        sig_i_dd(1, 2, kIntDoubleTypes4),
        sig_l_v(1, 0, kLongTypes4),
        sig_l_l(1, 1, kLongTypes4),
        sig_l_ll(1, 2, kLongTypes4),
        sig_i_ll(1, 2, kIntLongTypes4),
        sig_f_f(1, 1, kFloatTypes4),
        sig_f_ff(1, 2, kFloatTypes4),
        sig_d_d(1, 1, kDoubleTypes4),
        sig_d_dd(1, 2, kDoubleTypes4),
        sig_v_v(0, 0, kIntTypes4),
        sig_v_i(0, 1, kIntTypes4),
        sig_v_ii(0, 2, kIntTypes4),
        sig_v_iii(0, 3, kIntTypes4),
        sig_s_i(1, 1, kSimd128IntTypes4) {
    // I used C++ and you won't believe what happened next....
    for (int i = 0; i < 4; i++) kIntTypes4[i] = kAstI32;
    for (int i = 0; i < 4; i++) kLongTypes4[i] = kAstI64;
    for (int i = 0; i < 4; i++) kFloatTypes4[i] = kAstF32;
    for (int i = 0; i < 4; i++) kDoubleTypes4[i] = kAstF64;
    for (int i = 0; i < 4; i++) kIntLongTypes4[i] = kAstI64;
    for (int i = 0; i < 4; i++) kIntFloatTypes4[i] = kAstF32;
    for (int i = 0; i < 4; i++) kIntDoubleTypes4[i] = kAstF64;
    for (int i = 0; i < 4; i++) kSimd128IntTypes4[i] = kAstS128;
    kIntLongTypes4[0] = kAstI32;
    kIntFloatTypes4[0] = kAstI32;
    kIntDoubleTypes4[0] = kAstI32;
    kSimd128IntTypes4[1] = kAstI32;
  }

  FunctionSig* i_v() { return &sig_i_v; }
  FunctionSig* i_i() { return &sig_i_i; }
  FunctionSig* i_ii() { return &sig_i_ii; }
  FunctionSig* i_iii() { return &sig_i_iii; }

  FunctionSig* i_f() { return &sig_i_f; }
  FunctionSig* i_ff() { return &sig_i_ff; }
  FunctionSig* i_d() { return &sig_i_d; }
  FunctionSig* i_dd() { return &sig_i_dd; }

  FunctionSig* l_v() { return &sig_l_v; }
  FunctionSig* l_l() { return &sig_l_l; }
  FunctionSig* l_ll() { return &sig_l_ll; }
  FunctionSig* i_ll() { return &sig_i_ll; }

  FunctionSig* f_f() { return &sig_f_f; }
  FunctionSig* f_ff() { return &sig_f_ff; }
  FunctionSig* d_d() { return &sig_d_d; }
  FunctionSig* d_dd() { return &sig_d_dd; }

  FunctionSig* v_v() { return &sig_v_v; }
  FunctionSig* v_i() { return &sig_v_i; }
  FunctionSig* v_ii() { return &sig_v_ii; }
  FunctionSig* v_iii() { return &sig_v_iii; }
  FunctionSig* s_i() { return &sig_s_i; }

  FunctionSig* many(Zone* zone, LocalType ret, LocalType param, int count) {
    FunctionSig::Builder builder(zone, ret == kAstStmt ? 0 : 1, count);
    if (ret != kAstStmt) builder.AddReturn(ret);
    for (int i = 0; i < count; i++) {
      builder.AddParam(param);
    }
    return builder.Build();
  }

 private:
  LocalType kIntTypes4[4];
  LocalType kLongTypes4[4];
  LocalType kFloatTypes4[4];
  LocalType kDoubleTypes4[4];
  LocalType kIntLongTypes4[4];
  LocalType kIntFloatTypes4[4];
  LocalType kIntDoubleTypes4[4];
  LocalType kSimd128IntTypes4[4];

  FunctionSig sig_i_v;
  FunctionSig sig_i_i;
  FunctionSig sig_i_ii;
  FunctionSig sig_i_iii;

  FunctionSig sig_i_f;
  FunctionSig sig_i_ff;
  FunctionSig sig_i_d;
  FunctionSig sig_i_dd;

  FunctionSig sig_l_v;
  FunctionSig sig_l_l;
  FunctionSig sig_l_ll;
  FunctionSig sig_i_ll;

  FunctionSig sig_f_f;
  FunctionSig sig_f_ff;
  FunctionSig sig_d_d;
  FunctionSig sig_d_dd;

  FunctionSig sig_v_v;
  FunctionSig sig_v_i;
  FunctionSig sig_v_ii;
  FunctionSig sig_v_iii;
  FunctionSig sig_s_i;
};
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // TEST_SIGNATURES_H
