// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --allow-natives-syntax

function foo() {
  let rab = new ArrayBuffer(3, {maxByteLength: 5});
  let ta = new Int8Array(rab);
  ta[0] = 11;
  ta[1] = 22;
  ta[2] = 33;

  let it = ta.values();

  let r = it.next();
  assertEquals(false, r.done);
  assertEquals(11, r.value);

  rab.resize(0);
  r = it.next();
  assertEquals(true, r.done);
  assertEquals(undefined, r.value);

  rab.resize(5);
  r = it.next();
  assertEquals(true, r.done);
  assertEquals(undefined, r.value);
}

%PrepareFunctionForOptimization(foo);
foo();

%OptimizeFunctionOnNextCall(foo);
foo();
