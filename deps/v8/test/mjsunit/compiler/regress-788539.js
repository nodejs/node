// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-verify

// This test creates a FrameState node with a DeadValue parent framestate.
// Ensure that deadness is propagated along FrameState edges.

function f1() {
  return this;
}

function f2(x, value, type) {
  x instanceof type
}

function f3(a) {
  a.x = 0;
  if (a.x === 0) {
    a[1] = 0.1;
  }
  class B {
  }
  class C extends B {
    bar() {
      return super.foo()
    }
  }
  B.prototype.foo = f1;
  f2(new C().bar.call(), Object(), String);
}

f3(new Array(1));
f3(new Array(1));
%OptimizeFunctionOnNextCall(f3);
f3(new Array(1));
