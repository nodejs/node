// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-turbolev
// Flags: --no-optimize-on-next-call-optimizes-to-maglev
// Flags: --concurrent-recompilation

let left = "a".repeat(100) + "b".repeat(100);
Object.defineProperty(globalThis, 'left_glob', {
  value: left,
  writable: false,
  configurable: false
});

function f() {
  return left_glob + left_glob + "c";
}

const expected = left + left + "c";

%PrepareFunctionForOptimization(f);
assertEquals(expected, f());

%BlockAt('NewConsString', 1000);
%OptimizeFunctionOnNextCall(f, "concurrent");
assertEquals(expected, f());

%WaitUntilBlocked('NewConsString', 1000);

// Internalize the string. This will mutate the string on the main thread
// and turn it into a ThinString, right while the background thread is
// stopped at NewConsString.
%InternalizeString(left);

%Resume('NewConsString');
%WaitForBackgroundOptimization();
assertEquals(expected, f());
