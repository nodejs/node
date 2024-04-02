// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --print-ast --no-stress-background-compile

// Ensures that the --print-ast flag doesn't crash.
function foo(a) {
  var b = 2;
  return function bar() { return a + b; };
}

foo()();
