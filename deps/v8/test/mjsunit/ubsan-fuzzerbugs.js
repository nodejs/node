// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// crbug.com/923466
__v_5 = [ -1073741825, -2147483648];
__v_5.sort();

// crbug.com/923642
new RegExp("(abcd){2148473648,}", "");

// crbug.com/923626
new Date(2146399200000).toString();
new Date(2146940400000).toString();
new Date(2147481600000).toString();
new Date(2148022800000).toString();

// crbug.com/927212
assertThrows(() => (2n).toString(-2147483657), RangeError);

// crbug.com/927894
var typed_array = new Uint8Array(16);
typed_array.fill(0, -1.7976931348623157e+308);

// crbug.com/927996
var float_array = new Float32Array(1);
float_array[0] = 1e51;

// crbug.com/930086
(function() {
  try {
    // Build up a 536870910-character string (just under 2**30).
    var string = "ff";
    var long_string = "0x";
    for (var i = 2; i < 29; i++) {
      string = string + string;
      long_string += string;
    }
    assertThrows(() => BigInt(long_string), SyntaxError);
  } catch (e) {
    /* 32-bit architectures have a lower string length limit. */
  }
})();

// crbug.com/932679
(function() {
  const buffer = new DataView(new ArrayBuffer(2));
  function __f_14159(buffer) {
    try { return buffer.getUint16(Infinity, true); } catch(e) { return 0; }
  }
  __f_14159(buffer);
  %OptimizeFunctionOnNextCall(__f_14159);
  __f_14159(buffer);
})();

// crbug.com/937652
(function() {
  function f() {
    for (var i = 0; i < 1; i++) {
      var shift = 1;
      for (var j = 0; j < 2; ++j) {
        if (shift == shift) shift = 0;
        var x = 1;
        print((x << shift | x >>> 32 - shift));
      }
    }
  }
  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  f();
})();
