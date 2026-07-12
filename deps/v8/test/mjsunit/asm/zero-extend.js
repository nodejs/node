// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var stdlib = this;
var buffer = new ArrayBuffer(64 * 1024);
var foreign = {}


var zext8 = (function Module(stdlib, foreign, heap) {
  "use asm";
  function zext8(i) {
    i = i | 0;
    return (i & 0xff) | 0;
  }
  return { zext8: zext8 };
})(stdlib, foreign, buffer).zext8;

assertEquals(0, zext8(0));
assertEquals(0, zext8(0x100));
assertEquals(0xff, zext8(-1));
assertEquals(0xff, zext8(0xff));


var zext16 = (function Module(stdlib, foreign, heap) {
  "use asm";
  function zext16(i) {
    i = i | 0;
    return (i & 0xffff) | 0;
  }
  return { zext16: zext16 };
})(stdlib, foreign, buffer).zext16;

assertEquals(0, zext16(0));
assertEquals(0, zext16(0x10000));
assertEquals(0xffff, zext16(-1));
assertEquals(0xffff, zext16(0xffff));
