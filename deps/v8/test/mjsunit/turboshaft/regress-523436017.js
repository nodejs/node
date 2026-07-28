// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --shared-string-table
// Flags: --transition-strings-during-gc-with-stack --gc-global

// Warm Intl so CompareStringsOptionsFor → kTryFastPath for the default locale.
"warmup".localeCompare("warmup");

let g;
function f(a, b) {
  return a.localeCompare((g = {}, b));
}

%PrepareFunctionForOptimization(f);
// These string literals are internalized by the parser and will be the
// target of the ThinString we'll create later.
f("xxxxxxxxxxxxxxxx", "xxxxxxxxxxxxxxxx");
f("xxxxxxxxxxxxxxxx", "xxxxxxxxxxxxxxxx");
%OptimizeFunctionOnNextCall(f);
f("xxxxxxxxxxxxxxxx", "xxxxxxxxxxxxxxxx");

const b = "x".repeat(4);
const content = "x".repeat(16);
let a = %ShareObject(content);    // Shared, not internalized.
%ConstructInternalizedString(a);  // Added to StringForwardingTable.
// With --gc-global and --transition-strings-during-gc-with-stack, the
// failing allocation causes {a} to be turned into a ThinString.
%SimulateNewspaceFull();
const result = f(a, b);
assertEquals(1, result);

// Just to check that the expectation above is correct:
const baseline = ("x".repeat(16)).localeCompare("x".repeat(4));
assertEquals(1, baseline);
