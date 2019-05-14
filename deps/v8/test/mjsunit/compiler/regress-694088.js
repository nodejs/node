// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function is_little_endian() {
  var buffer = new ArrayBuffer(4);
  HEAP32 = new Int32Array(buffer);
  HEAPU8 = new Uint8Array(buffer);
  HEAP32[0] = 255;
  if (HEAPU8[0] === 255 && HEAPU8[3] === 0)
    return true;
  else
    return false;
}

(function () {
  var buffer = new ArrayBuffer(8);
  var i32 = new Int32Array(buffer);
  var f64 = new Float64Array(buffer);

  function foo() {
    f64[0] = 1;
    var x = f64[0];
    if (is_little_endian())
      return i32[0];
    else
      return i32[1];
  }
  assertEquals(0, foo());
})();

(function () {
  var buffer = new ArrayBuffer(8);
  var i16 = new Int16Array(buffer);
  var i32 = new Int32Array(buffer);

  function foo() {
    i32[0] = 0x20001;
    var x = i32[0];
    if (is_little_endian())
      return i16[0];
    else
      return i16[1];
  }
  assertEquals(1, foo());
})();
