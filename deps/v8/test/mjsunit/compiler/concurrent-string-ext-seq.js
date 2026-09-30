// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests that sequential strings are protected from being externalized on the
// main thread while a background compilation thread is reading them.

// Flags: --allow-natives-syntax --expose-externalize-string
// Flags: --concurrent-recompilation --no-jitless
// Flags: --no-disable-optimizing-compilers

function TestRace(str, f, ext, syncPointName) {
  // 1. JIT-compile the externalization function to minimize JS overhead during
  // the race.
  %PrepareFunctionForOptimization(ext);
  const warmup1 = str + "foo";
  %InternalizeString(warmup1);
  ext(warmup1);
  %OptimizeFunctionOnNextCall(ext);
  const warmup2 = str + "bar";
  %InternalizeString(warmup2);
  ext(warmup2);

  // 2. Kick off background compilation and pause it right before string
  // concatenation.
  %BlockAt("ConcurrentConcatenateStrings", 10000);
  %PrepareFunctionForOptimization(f);
  // Warm up with the string addition path.
  f(true);
  %OptimizeFunctionOnNextCall(f, "concurrent");
  // Trigger the compilation; do not use the string addition here since an
  // allocation might trigger a GC, which would deadlock with the sync point.
  f(false);

  // Calling assertTrue here would trigger JS allocations and interrupt checks,
  // causing a deadlock with a GC.
  let res0 = %WaitUntilBlocked("ConcurrentConcatenateStrings", 10000);

  // 3. Move the background thread forward to pause during the actual string
  // memory read.
  %BlockAt(syncPointName, 10000);
  let res1 = %Resume("ConcurrentConcatenateStrings");
  let res2 = %WaitUntilBlocked(syncPointName, 10000);

  // 4. Unblock the background thread and immediately try to externalize on the
  // main thread. The background thread is reading the string, and the main
  // thread will safely block until the background read completes.
  let res3 = %Resume(syncPointName);
  ext(str);

  // Safe to assert now that the background thread is free.
  assertTrue(res0);
  assertTrue(res1);
  assertTrue(res2);
  assertTrue(res3);
}

// Base length must be >= ConsString::kMinLength to form a ConsString, and
// concatenated length <= kConstantStringFlattenMaxSize to flatten directly.
const kBaseLen = 42;

const str1 = "A".repeat(kBaseLen);
%InternalizeString(str1);
function f1(do_string_op) { if (do_string_op) return str1 + "x"; return 0; }
function externalize1(s) { externalizeString(s); }
TestRace(str1, f1, externalize1, "StringWriteToFlatSeqOneByteString");

const str2 = "\u1234".repeat(kBaseLen);
%InternalizeString(str2);
function f2(do_string_op) { if (do_string_op) return str2 + "x"; return 0; }
function externalize2(s) { externalizeString(s); }
TestRace(str2, f2, externalize2, "StringWriteToFlatSeqTwoByteString");
