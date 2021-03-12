// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The C++ style guide recommends using <re2> instead of <regex>. However, the
// former isn't available in V8.
#include <regex>  // NOLINT(build/c++11)
#include <vector>

#include "src/codegen/arm64/register-arm64.h"
#include "test/cctest/cctest.h"
#include "test/cctest/disasm-regex-helper.h"

namespace v8 {
namespace internal {

namespace {
// Poison register.
const int kPRegCode = kSpeculationPoisonRegister.code();
const std::string kPReg =  // NOLINT(runtime/string)
    "x" + std::to_string(kPRegCode);
}  // namespace

TEST(DisasmPoisonMonomorphicLoad) {
#ifdef ENABLE_DISASSEMBLER
  if (i::FLAG_always_opt || !i::FLAG_opt) return;
  // TODO(9684): Re-enable for TurboProp if necessary.
  if (i::FLAG_turboprop) return;

  i::FLAG_allow_natives_syntax = true;
  i::FLAG_untrusted_code_mitigations = true;
  i::FLAG_debug_code = false;

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
#if defined(V8_COMPRESS_POINTERS)
  std::vector<std::string> patterns_array = {
      "ldur <<Map:w[0-9]+>>, \\[<<Obj:x[0-9]+>>, #-1\\]",  // load map
      "ldr <<ExpMap:w[0-9]+>>, pc",                        // load expected map
      "cmp <<Map>>, <<ExpMap>>",                           // compare maps
      "b.ne",                                              // deopt if different
      "csel " + kPReg + ", xzr, " + kPReg + ", ne",        // update the poison
      "csdb",                                              // spec. barrier
      "ldur w<<Field:[0-9]+>>, \\[<<Obj>>, #[0-9]+\\]",    // load the field
      "and x<<Field>>, x<<Field>>, " + kPReg,              // apply the poison
  };
#else
  std::vector<std::string> patterns_array = {
      "ldur <<Map:x[0-9]+>>, \\[<<Obj:x[0-9]+>>, #-1\\]",  // load map
      "ldr <<ExpMap:x[0-9]+>>, pc",                        // load expected map
      "cmp <<Map>>, <<ExpMap>>",                           // compare maps
      "b.ne",                                              // deopt if different
      "csel " + kPReg + ", xzr, " + kPReg + ", ne",        // update the poison
      "csdb",                                              // spec. barrier
      "ldur <<Field:x[0-9]+>>, \\[<<Obj>>, #[0-9]+\\]",    // load the field
      "and <<Field>>, <<Field>>, " + kPReg,                // apply the poison
  };
#endif
  CHECK(CheckDisassemblyRegexPatterns("mono", patterns_array));
#endif  // ENABLE_DISASSEMBLER
}

TEST(DisasmPoisonPolymorphicLoad) {
#ifdef ENABLE_DISASSEMBLER
  if (i::FLAG_always_opt || !i::FLAG_opt) return;
  // TODO(9684): Re-enable for TurboProp if necessary.
  if (i::FLAG_turboprop) return;

  i::FLAG_allow_natives_syntax = true;
  i::FLAG_untrusted_code_mitigations = true;
  i::FLAG_debug_code = false;

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
#if defined(V8_COMPRESS_POINTERS)
  std::vector<std::string> patterns_array = {
      "ldur <<Map0:w[0-9]+>>, \\[<<Obj:x[0-9]+>>, #-1\\]",  // load map
      "ldr <<ExpMap:w[0-9]+>>, pc",                         // load map const #1
      "cmp <<Map0>>, <<ExpMap>>",                           // compare maps
      "b.eq",                                               // ? go to the load
      "csel " + kPReg + ", xzr, " + kPReg + ", eq",         // update the poison
      "csdb",                                               // spec. barrier
      "ldur <<Map1:w[0-9]+>>, \\[<<Obj>>, #-1\\]",          // load map
      "ldr <<ExpMap1:w[0-9]+>>, pc",                        // load map const #2
      "cmp <<Map1>>, <<ExpMap1>>",                          // compare maps
      "b.ne",                                            // deopt if different
      "csel " + kPReg + ", xzr, " + kPReg + ", ne",      // update the poison
      "csdb",                                            // spec. barrier
      "ldur w<<Field:[0-9]+>>, \\[<<Obj>>, #[0-9]+\\]",  // load the field
      "and x<<Field>>, x<<Field>>, " + kPReg,            // apply the poison
      "asr w[0-9]+, w<<Field>>, #1",                     // untag
      "b",                                               // goto merge point
      // Lcase1:
      "csel " + kPReg + ", xzr, " + kPReg + ", ne",      // update the poison
      "csdb",                                            // spec. barrier
      "ldur w<<BSt:[0-9]+>>, \\[<<Obj>>, #[0-9]+\\]",    // load backing store
                                                         // branchful decompress
      "add x<<BSt>>, x26, x<<BSt>>",                     // Add root to ref
      "and x<<BSt>>, x<<BSt>>, " + kPReg,                // apply the poison
      "ldur w<<Prop:[0-9]+>>, \\[x<<BSt>>, #[0-9]+\\]",  // load the property
      "and x<<Prop>>, x<<Prop>>, " + kPReg,              // apply the poison
                                                         // Ldone:
  };
#else
  std::vector<std::string> patterns_array = {
      "ldur <<Map0:x[0-9]+>>, \\[<<Obj:x[0-9]+>>, #-1\\]",  // load map
      "ldr <<ExpMap0:x[0-9]+>>, pc",                        // load map const #1
      "cmp <<Map0>>, <<ExpMap0>>",                          // compare maps
      "b.eq",                                               // ? go to the load
      "csel " + kPReg + ", xzr, " + kPReg + ", eq",         // update the poison
      "csdb",                                               // spec. barrier
      "ldur <<Map1:x[0-9]+>>, \\[<<Obj>>, #-1\\]",          // load map
      "ldr <<ExpMap1:x[0-9]+>>, pc",                        // load map const #2
      "cmp <<Map1>>, <<ExpMap1>>",                          // compare maps
      "b.ne",                                            // deopt if different
      "csel " + kPReg + ", xzr, " + kPReg + ", ne",      // update the poison
      "csdb",                                            // spec. barrier
      "ldur x<<Field:[0-9]+>>, \\[<<Obj>>, #[0-9]+\\]",  // load the field
      "and x<<Field>>, x<<Field>>, " + kPReg,            // apply the poison
#ifdef V8_31BIT_SMIS_ON_64BIT_ARCH
      "asr w<<Field>>, w<<Field>>, #1",                  // untag
#else
      "asr x[0-9]+, x<<Field>>, #32",                    // untag
#endif
      "b",                                               // goto merge point
      // Lcase1:
      "csel " + kPReg + ", xzr, " + kPReg + ", ne",     // update the poison
      "csdb",                                           // spec. barrier
      "ldur <<BSt:x[0-9]+>>, \\[<<Obj>>, #[0-9]+\\]",   // load backing store
      "and <<BSt>>, <<BSt>>, " + kPReg,                 // apply the poison
      "ldur <<Prop:x[0-9]+>>, \\[<<BSt>>, #[0-9]+\\]",  // load the property
      "and <<Prop>>, <<Prop>>, " + kPReg,               // apply the poison
                                                        // Ldone:
  };
#endif
  CHECK(CheckDisassemblyRegexPatterns("poly", patterns_array));
#endif  // ENABLE_DISASSEMBLER
}

TEST(DisasmPoisonMonomorphicLoadFloat64) {
#ifdef ENABLE_DISASSEMBLER
  if (i::FLAG_always_opt || !i::FLAG_opt) return;
  // TODO(9684): Re-enable for TurboProp if necessary.
  if (i::FLAG_turboprop) return;

  i::FLAG_allow_natives_syntax = true;
  i::FLAG_untrusted_code_mitigations = true;

  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());

  CompileRun(
      "function mono(o) { return o.x; }"
      "%PrepareFunctionForOptimization(mono);"
      "mono({ x : 1.1 });"
      "mono({ x : 1.1 });"
      "%OptimizeFunctionOnNextCall(mono);"
      "mono({ x : 1.1 });");

  // Matches that the property access sequence is instrumented with
  // poisoning.
#if defined(V8_COMPRESS_POINTERS)
  std::vector<std::string> patterns_array = {
      "ldur <<Map:w[0-9]+>>, \\[<<Obj:x[0-9]+>>, #-1\\]",  // load map
      "ldr <<ExpMap:w[0-9]+>>, pc",                        // load expected map
      "cmp <<Map>>, <<ExpMap>>",                           // compare maps
      "b.ne",                                              // deopt if differ
      "csel " + kPReg + ", xzr, " + kPReg + ", ne",        // update the poison
      "csdb",                                              // spec. barrier
      "ldur w<<F1:[0-9]+>>, \\[<<Obj>>, #11\\]",           // load heap number
      "add x<<F1>>, x26, x<<F1>>",                         // Decompress ref
      "and x<<F1>>, x<<F1>>, " + kPReg,                    // apply the poison
      "add <<Addr:x[0-9]+>>, x<<F1>>, #0x[0-9a-f]+",       // addr. calculation
      "and <<Addr>>, <<Addr>>, " + kPReg,                  // apply the poison
      "ldr d[0-9]+, \\[<<Addr>>\\]",                       // load Float64
  };
#else
  std::vector<std::string> patterns_array = {
      "ldur <<Map:x[0-9]+>>, \\[<<Obj:x[0-9]+>>, #-1\\]",  // load map
      "ldr <<ExpMap:x[0-9]+>>, pc",                        // load expected map
      "cmp <<Map>>, <<ExpMap>>",                           // compare maps
      "b.ne",                                              // deopt if differ
      "csel " + kPReg + ", xzr, " + kPReg + ", ne",        // update the poison
      "csdb",                                              // spec. barrier
      "ldur <<F1:x[0-9]+>>, \\[<<Obj>>, #23\\]",           // load heap number
      "and <<F1>>, <<F1>>, " + kPReg,                      // apply the poison
      "add <<Addr:x[0-9]+>>, <<F1>>, #0x7",                // addr. calculation
      "and <<Addr>>, <<Addr>>, " + kPReg,                  // apply the poison
      "ldr d[0-9]+, \\[<<Addr>>\\]",                       // load Float64
  };
#endif
  CHECK(CheckDisassemblyRegexPatterns("mono", patterns_array));
#endif  // ENABLE_DISASSEMBLER
}

}  // namespace internal
}  // namespace v8
