// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var stdlib = this;
var buffer = new ArrayBuffer(64 * 1024);
var foreign = {}


var sext8 = (function Module(stdlib, foreign, heap) {
  "use asm";
  function sext8(i) {
    i = i|0;
    i = i << 24 >> 24;
    return i|0;
  }
  return { sext8: sext8 };
})(stdlib, foreign, buffer).sext8;

assertEquals(-128, sext8(128));
assertEquals(-1, sext8(-1));
assertEquals(-1, sext8(255));
assertEquals(0, sext8(0));
assertEquals(0, sext8(256));
assertEquals(42, sext8(42));
assertEquals(127, sext8(127));


var sext16 = (function Module(stdlib, foreign, heap) {
  "use asm";
  function sext16(i) {
    i = i|0;
    i = i << 16 >> 16;
    return i|0;
  }
  return { sext16: sext16 };
})(stdlib, foreign, buffer).sext16;

assertEquals(-32768, sext16(32768));
assertEquals(-1, sext16(-1));
assertEquals(-1, sext16(65535));
assertEquals(0, sext16(0));
assertEquals(0, sext16(65536));
assertEquals(128, sext16(128));
assertEquals(32767, sext16(32767));
