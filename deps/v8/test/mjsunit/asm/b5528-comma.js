// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(stdlib, env, heap) {
  "use asm";

  var a = new stdlib.Int32Array(heap);
  var b = new stdlib.Float32Array(heap);
  var fround = stdlib.Math.fround;
  var value = env.value|0;

  function foo() {
    var x = fround(0.0);
    x = (a[0]=value|0,fround(b[0]));
    return fround(x);
  }

  return { foo: foo };
}

var buffer = new ArrayBuffer(32);
assertEquals(0.0, Module(this, {value: 0x00000000}, buffer).foo());
assertEquals(-0.0, Module(this, {value: 0x80000000}, buffer).foo());
assertEquals(5.0, Module(this, {value: 0x40a00000}, buffer).foo());
assertEquals(-5.0, Module(this, {value: 0xc0a00000}, buffer).foo());
assertEquals(129.375, Module(this, {value: 0x43016000}, buffer).foo());
assertEquals(-129.375, Module(this, {value: 0xc3016000}, buffer).foo());
assertEquals(Infinity, Module(this, {value: 0x7f800000}, buffer).foo());
assertEquals(-Infinity, Module(this, {value: 0xff800000}, buffer).foo());
assertEquals(NaN, Module(this, {value: 0x7fffffff}, buffer).foo());
