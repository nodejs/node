// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan --no-always-turbofan

function strictEquals(a, b) {
  return a === b;
}

%PrepareFunctionForOptimization(strictEquals);

// Add the ReceiverOrNullOrUndefined feedback.
strictEquals({}, null);

%OptimizeFunctionOnNextCall(strictEquals);

const normalObject = {};
const undetectable = %GetUndetectable();

assertTrue(strictEquals(null, null));
assertFalse(strictEquals(null, undefined));
assertFalse(strictEquals(null, normalObject));
assertFalse(strictEquals(null, undetectable));

assertFalse(strictEquals(undefined, null));
assertTrue(strictEquals(undefined, undefined));
assertFalse(strictEquals(undefined, normalObject));
assertFalse(strictEquals(undefined, undetectable));

assertFalse(strictEquals(normalObject, null));
assertFalse(strictEquals(normalObject, undefined));
assertTrue(strictEquals(normalObject, normalObject));
assertFalse(strictEquals(normalObject, {}));
assertFalse(strictEquals(normalObject, undetectable));

assertFalse(strictEquals(undetectable, null));
assertFalse(strictEquals(undetectable, undefined));
assertFalse(strictEquals(undetectable, normalObject));
assertTrue(strictEquals(undetectable, undetectable));

assertOptimized(strictEquals);

// Passing an unexpected object deopts.
assertFalse(strictEquals({}, ""));
assertUnoptimized(strictEquals);
