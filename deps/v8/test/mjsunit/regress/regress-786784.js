// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  function g(arg) { return arg; }
  // The closure contains a call IC slot.
  return function() { return g(42); };
}

const a = Realm.create();
const b = Realm.create();

// Create two closures in different contexts sharing the same
// SharedFunctionInfo (shared due to code caching).
const x = Realm.eval(a, f.toString() + " f()");
const y = Realm.eval(b, f.toString() + " f()");

// Run the first closure to create SFI::code.
x();

// At this point, SFI::code is set and `x` has a feedback vector (`y` does not).

// Enabling block code coverage deoptimizes all functions and triggers the
// buggy code path in which we'd unconditionally replace JSFunction::code with
// its SFI::code (but skip feedback vector setup).
%DebugToggleBlockCoverage(true);

// Still no feedback vector set on `y` but it now contains code. Run it to
// trigger the crash when attempting to write into the non-existent feedback
// vector.
y();
