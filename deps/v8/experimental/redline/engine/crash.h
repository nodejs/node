// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_ENGINE_CRASH_H_
#define V8_EXPERIMENTAL_REDLINE_ENGINE_CRASH_H_

#include <cstdint>
#include <string>
#include <vector>

#include "experimental/redline/exec/execution.h"

namespace redline {

// A stable summary of a crash, derived from what the target printed. Two
// crashes with the same signature are the same bug for the purpose of
// deduplication (the saved-crash budget, the per-signature statistics and
// the minimizer's oracle, which must not wander off into a different bug).
//
// Recognized formats (in this order):
//  - V8 fatal errors:  "# Fatal error in ../../src/foo.cc, line 123" plus
//    the following "Check failed: ..." / "Debug check failed: ..." line, if
//    any. Signature: "fatal foo.cc:123 <check>".
//  - Sanitizer reports: "ERROR: AddressSanitizer: heap-use-after-free ..."
//    plus the function of the first frame. Signature: "asan <kind> <fn>".
//  - Signals: "Received signal 11 SEGV_MAPERR ..." plus the first three
//    frames of the C stack trace that name a function (frames without a
//    symbol, the stack-trace machinery itself and libc are skipped).
//    Signature: "sig11 SEGV_MAPERR fn1 < fn2 < fn3".
//    A stack overflow faults wherever the stack runs out, so its first
//    frames differ from crash to crash; when the trace is dominated by
//    functions that repeat (see RecursionCycle), those name the crash
//    instead: "sig11 SEGV_MAPERR stack-overflow fnA | fnB | fnC".
//  - Anything else: "sig<N>" (or "exit<code>") alone.
struct CrashSignature {
  std::string text;
  uint64_t hash = 0;

  static CrashSignature FromExecution(const Execution& execution);
  // Also exposed for tests: builds the signature from the raw stderr text.
  static CrashSignature FromOutput(int signal, int exit_code,
                                   const std::string& stderr_text);

  bool operator==(const CrashSignature& other) const {
    return hash == other.hash;
  }
  bool operator!=(const CrashSignature& other) const {
    return hash != other.hash;
  }
};

// Demangles an Itanium C++ symbol and strips the parameter list, so that
// "_ZN2v88internal6Parser17GetBigIntAsSymbolEv" becomes
// "v8::internal::Parser::GetBigIntAsSymbol". Non-mangled input is returned
// unchanged (minus a parameter list, if it has one).
std::string ShortFunctionName(const std::string& symbol);

// The frames of a V8 "==== C stack trace ====" block (or a sanitizer stack)
// that name a function, innermost first, already shortened.
std::vector<std::string> CrashFrames(const std::string& stderr_text);

// A function counts as part of a recursion when it appears at least this
// many times among the frames of one trace.
inline constexpr size_t kMinRecursionRepeats = 8;
inline constexpr size_t kMaxRecursionCycleFunctions = 4;

// The functions of a recursion that fills the trace: those appearing at least
// kMinRecursionRepeats times, sorted by name (so the result does not depend
// on where the stack ran out), at most kMaxRecursionCycleFunctions of them.
// Empty unless they make up at least half of the frames.
std::vector<std::string> RecursionCycle(const std::vector<std::string>& frames);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_ENGINE_CRASH_H_
