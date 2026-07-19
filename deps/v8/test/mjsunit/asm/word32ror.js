// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var stdlib = {};
var foreign = {};
var heap = new ArrayBuffer(64 * 1024);

var rol = (function Module(stdlib, foreign, heap) {
  "use asm";
  function rol(x, y) {
    x = x | 0;
    y = y | 0;
    return (x << y) | (x >>> (32 - y)) | 0;
  }
  return { rol: rol };
})(stdlib, foreign, heap).rol;

assertEquals(10, rol(10, 0));
assertEquals(2, rol(1, 1));
assertEquals(0x40000000, rol(1, 30));
assertEquals(-0x80000000, rol(1, 31));

var ror = (function Module(stdlib, foreign, heap) {
  "use asm";
  function ror(x, y) {
    x = x | 0;
    y = y | 0;
    return (x << (32 - y)) | (x >>> y) | 0;
  }
  return { ror: ror };
})(stdlib, foreign, heap).ror;

assertEquals(10, ror(10, 0));
assertEquals(-0x80000000, ror(1, 1));
assertEquals(0x40000000, ror(1, 2));
assertEquals(2, ror(1, 31));
