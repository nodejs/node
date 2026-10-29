// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let glob_str = "abcd";

function cmp(a) {
  // This comparison has InternalizedString feedback and Maglev will save the
  // CheckedInternalizedString node in the checked_value alternative.

  // Note that this needs to happen in a function that will get inlined, because
  // the CheckedInternalizedString will be stored directly in the
  // current_interpreter_frame_, which shadows its input in the current
  // function, but not in the caller.

  return a == "abcd";
}

function foo(a) {
  if (cmp(a)) {
    // This is a store to a constant field ==> it gets lowered to a simple
    // CheckValue(a), and saves the constant ("abcd") in the checked_value
    // alternative.
    glob_str = a;
  }
}

%PrepareFunctionForOptimization(cmp);
%PrepareFunctionForOptimization(foo);
foo("abcd");
foo("efgh");

%OptimizeFunctionOnNextCall(foo);
foo("abcd");
