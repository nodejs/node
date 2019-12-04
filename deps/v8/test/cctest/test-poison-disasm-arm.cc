// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The C++ style guide recommends using <re2> instead of <regex>. However, the
// former isn't available in V8.
#include <regex>  // NOLINT(build/c++11)
#include <vector>

#include "src/codegen/arm/register-arm.h"
#include "test/cctest/cctest.h"
#include "test/cctest/disasm-regex-helper.h"

namespace v8 {
namespace internal {

namespace {
// Poison register.
const int kPRegCode = kSpeculationPoisonRegister.code();
const std::string kPReg =  // NOLINT(runtime/string)
    "r" + std::to_string(kPRegCode);
}  // namespace

TEST(DisasmPoisonMonomorphicLoad) {
#ifdef ENABLE_DISASSEMBLER
  if (i::FLAG_always_opt || !i::FLAG_opt) return;

  i::FLAG_allow_natives_syntax = true;
  i::FLAG_untrusted_code_mitigations = true;

  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  CompileRun(
      "function mono(o) { return o.x; };"
      "%PrepareFunctionForOptimization(mono);"
      "mono({ x : 1 });"
      "mono({ x : 1 });"
      "%OptimizeFunctionOnNextCall(mono);"
      "mono({ x : 1 });");

  // Matches that the property access sequence is instrumented with
  // poisoning.
  std::vector<std::string> patterns_array = {
      "ldr <<Map:r[0-9]+>>, \\[<<Obj:r[0-9]+>>, #-1\\]",   // load map
      "ldr <<ExpMap:r[0-9]+>>, \\[pc, #",                  // load expected map
      "cmp <<Map>>, <<ExpMap>>",                           // compare maps
      "bne",                                               // deopt if different
      "eorne " + kPReg + ", " + kPReg + ", " + kPReg,      // update the poison
      "csdb",                                              // spec. barrier
      "ldr <<Field:r[0-9]+>>, \\[<<Obj>>, #\\+[0-9]+\\]",  // load the field
      "and <<Field>>, <<Field>>, " + kPReg,                // apply the poison
  };
  CHECK(CheckDisassemblyRegexPatterns("mono", patterns_array));
#endif  // ENABLE_DISASSEMBLER
}

TEST(DisasmPoisonPolymorphicLoad) {
#ifdef ENABLE_DISASSEMBLER
  if (i::FLAG_always_opt || !i::FLAG_opt) return;

  i::FLAG_allow_natives_syntax = true;
  i::FLAG_untrusted_code_mitigations = true;

  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  CompileRun(
      "function poly(o) { return o.x + 1; };"
      "let o1 = { x : 1 };"
      "let o2 = { y : 1 };"
      "o2.x = 2;"
      "%PrepareFunctionForOptimization(poly);"
      "poly(o2);"
      "poly(o1);"
      "poly(o2);"
      "%OptimizeFunctionOnNextCall(poly);"
      "poly(o1);");

  // Matches that the property access sequence is instrumented with
  // poisoning.
  std::vector<std::string> patterns_array = {
      "ldr <<Map0:r[0-9]+>>, \\[<<Obj:r[0-9]+>>, #-1\\]",  // load map
      "ldr <<ExpMap0:r[0-9]+>>, \\[pc",                    // load map const #1
      "cmp <<Map0>>, <<ExpMap0>>",                         // compare maps
      "beq",                                               // ? go to the load
      "eoreq " + kPReg + ", " + kPReg + ", " + kPReg,      // update the poison
      "csdb",                                              // spec. barrier
      "ldr <<Map1:r[0-9]+>>, \\[<<Obj>>, #-1\\]",          // load map
      "ldr <<ExpMap1:r[0-9]+>>, \\[pc",                    // load map const #2
      "cmp <<Map1>>, <<ExpMap1>>",                         // compare maps
      "bne",                                               // deopt if different
      "eorne " + kPReg + ", " + kPReg + ", " + kPReg,      // update the poison
      "csdb",                                              // spec. barrier
      "ldr <<Field:r[0-9]+>>, \\[<<Obj>>, #\\+[0-9]+\\]",  // load the field
      "and <<Field>>, <<Field>>, " + kPReg,                // apply the poison
      "mov r[0-9]+, <<Field>>, asr #1",                    // untag
      "b",                                                 // goto merge point
      // Lcase1:
      "eorne " + kPReg + ", " + kPReg + ", " + kPReg,     // update the poison
      "csdb",                                             // spec. barrier
      "ldr <<BSt:r[0-9]+>>, \\[<<Obj>>, #\\+[0-9]+\\]",   // load backing store
      "and <<BSt>>, <<BSt>>, " + kPReg,                   // apply the poison
      "ldr <<Prop:r[0-9]+>>, \\[<<Obj>>, #\\+[0-9]+\\]",  // load the property
      "and <<Prop>>, <<Prop>>, " + kPReg,                 // apply the poison
                                                          // Ldone:
  };
  CHECK(CheckDisassemblyRegexPatterns("poly", patterns_array));
#endif  // ENABLE_DISASSEMBLER
}

}  // namespace internal
}  // namespace v8
