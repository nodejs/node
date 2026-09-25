// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --allow-natives-syntax

function f() {
  var s = "äϠ�𝌆";
  var i = s[Symbol.iterator]();
  assertEquals("ä", i.next().value);
  assertEquals("Ϡ", i.next().value);
  assertEquals("�", i.next().value);
  assertEquals("𝌆", i.next().value);
  assertSame(undefined, i.next().value);
};
%PrepareFunctionForOptimization(f);
f();
f();
%OptimizeFunctionOnNextCall(f);
f();
