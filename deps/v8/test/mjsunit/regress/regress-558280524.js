// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --proto-assign-seq-lazy-func-opt

// Arrow function without prototype property.
{
  let f = () => {};
  function C() {}
  C.prototype = f;
  function test(C) {
    C.prototype.prototype = function () { return 1; };
    C.prototype.m1 = function () { return 2; };
  }
  test(C);
  %HeapObjectVerify(f);
  assertEquals(1, f.prototype());
  assertEquals(2, f.m1());
  %HeapObjectVerify(f);
}

// Concise method without prototype property.
{
  let obj = {
    m() {}
  };
  function C() {}
  C.prototype = obj.m;
  function test(C) {
    C.prototype.prototype = function () { return 3; };
    C.prototype.m1 = function () { return 4; };
  }
  test(C);
  %HeapObjectVerify(obj.m);
  assertEquals(3, obj.m.prototype());
  assertEquals(4, obj.m.m1());
  %HeapObjectVerify(obj.m);
}

// Async function without prototype property.
{
  let f = async () => {};
  function C() {}
  C.prototype = f;
  function test(C) {
    C.prototype.prototype = function () { return 5; };
    C.prototype.m1 = function () { return 6; };
  }
  test(C);
  %HeapObjectVerify(f);
  assertEquals(5, f.prototype());
  assertEquals(6, f.m1());
  %HeapObjectVerify(f);
}
