// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var stdlib = {};
var foreign = {};
var heap = new ArrayBuffer(64 * 1024);

var rol = (function Module(stdlib, foreign, heap) {
  "use asm";
  function rol() {
    y = "a" > false;
    return y + (1 - y);
  }
  return { rol: rol };
})(stdlib, foreign, heap).rol;

assertEquals(1, rol());
