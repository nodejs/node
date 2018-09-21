// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function baz(a, b) {
  for (var i = 0; i < a.length; i++) {
    if (a[i], b[i]) return false;
  }
}
function bar(expected, found) {
  if (!baz(found, expected)) {
  }
};
bar([{}, 6, NaN], [1.8, , NaN]);
function foo() {
  var a = [1,2,3,4];
  bar(a.length, a.length);
}
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
