// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode

"use strong";

function foo(param, fooCount, barCount) {
  if (param === 0)
    return {'foo': fooCount, 'bar': barCount};
  return bar(param - 1, fooCount + 1, barCount);
}

function bar(param, fooCount, barCount) {
  if (param === 0)
    return {'foo': fooCount, 'bar': barCount};
  return foo(param - 1, fooCount, barCount + 1);
}

(function TestMutuallyRecursiveFunctions() {
  let obj = foo(10, 0, 0);
  assertEquals(obj.foo, 5);
  assertEquals(obj.bar, 5);
})();
