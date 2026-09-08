// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sparkplug --sparkplug-plus

function makeClass() {
  return class Foo {
    // Field with initializer so that we install class_fields_symbol on Foo
    // instances and load it during construction.
    x = 42;
  };
}

// Step 1: Initialize DerivedClass constructor with baseline code and
// monomorphic feedback. The Sparkplug+ code should be patched to the
// monomorphic handler.

// c0 has Map M0.
const c0 = makeClass();
%CompileBaseline(c0);
%PrepareFunctionForOptimization(c0);

let feedback = %GetFeedback(c0);
if (feedback !== undefined) {
  assertEquals(
    [
      ["LoadProperty", "UNINITIALIZED"],
      ["Call", "UNINITIALIZED"],
    ],
    feedback,
  );
}

// Run the constructor to collect feedback and execute patching.
new c0();

feedback = %GetFeedback(c0);
if (feedback !== undefined) {
  assertMatches(/^MONOMORPHIC/, feedback[0][1]);
  assertMatches(/^MONOMORPHIC/, feedback[1][1]);
}

// Step 2: Transition to polymorphic feedback via a deopt to the interpreter in Maglev.

// c1 has Map M1.
const c1 = makeClass();
%OptimizeMaglevOnNextCall(c1);
// Deopts to interpreter, transitions feedback to POLYMORPHIC [M0, M1].
// Since it's in the interpreter, the Sparkplug+ code is _not_ patched.
new c1();
assertUnoptimized(c1);

feedback = %GetFeedback(c1);
if (feedback !== undefined) {
  assertMatches(/^POLYMORPHIC/, feedback[0][1]);
  assertMatches(/^POLYMORPHIC/, feedback[1][1]);
}

// Step 3: Second call of c1 executes Sparkplug+ code again, which executes the
// monomorphic patched LoadIC on polymorphic feedback, which should miss. We then
// enter the LoadIC miss path despite having matching polymorphic feedback.
new c1();

// Despite the miss, we should still be polymorphic on this path.
feedback = %GetFeedback(c1);
if (feedback !== undefined) {
  assertMatches(/^POLYMORPHIC/, feedback[0][1]);
  assertMatches(/^POLYMORPHIC/, feedback[1][1]);
}
