// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function baz(f) {
  f();
}
function goo() {}
baz(goo);
baz(goo);

function bar(p) {
  if (p == 0) baz(1);
};
%PrepareFunctionForOptimization(bar);
bar(1);
bar(1);
%OptimizeFunctionOnNextCall(bar);
bar(1);
