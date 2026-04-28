// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

[1].push(1);

(function PushHoleBitPattern() {
  function g(src, dst, i) {
    dst[i] = src[i];
  }

  var b = new ArrayBuffer(8);
  var i32 = new Int32Array(b);
  i32[0] = 0xFFF7FFFF;
  i32[1] = 0xFFF7FFFF;
  var f64 = new Float64Array(b);

  var a = [,2.5];
  a.push(f64[0]);
  assertTrue(Number.isNaN(a[2]));
})();
