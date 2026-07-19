// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(r) {
  return r.finally();
}

const resolution = Promise.resolve();
foo(resolution);

function bar() {
  try {
    foo(undefined);
  } catch (e) {}
}

%PrepareFunctionForOptimization(bar);
bar();
bar();
%OptimizeFunctionOnNextCall(bar);
bar();
