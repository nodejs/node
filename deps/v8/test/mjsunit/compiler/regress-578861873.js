// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const proto = { a: 1 };
Object.setPrototypeOf(Array.prototype, proto);
Object.defineProperty(Array.prototype, "concat", { enumerable: true, value: 1 });
%ToFastProperties(proto);

function tag(x) {
  return x;
}

function foo(b) {
  if (b) {
    tag`hello`;
  }
  proto.a = 1;
}

%PrepareFunctionForOptimization(foo);
foo(false);
foo(false);
%OptimizeFunctionOnNextCall(foo);
foo(true);
assertEquals(1, proto.a);
