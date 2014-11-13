// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var stdlib = {};
var foreign = {};
var heap = new ArrayBuffer(64 * 1024);

function Int32Mod(divisor) {
  var name = "mod_";
  if (divisor < 0) {
    name += "minus_";
  }
  name += Math.abs(divisor);
  var m = eval("function Module(stdlib, foreign, heap) {\n"
      + " \"use asm\";\n"
      + " function " + name + "(dividend) {\n"
      + "  return ((dividend | 0) % " + divisor + ") | 0;\n"
      + " }\n"
      + " return { f: " + name + "}\n"
      + "}; Module");
  return m(stdlib, foreign, heap).f;
}

var divisors = [-2147483648, -32 * 1024, -1000, -16, -7, -2, -1,
                1, 3, 4, 10, 64, 100, 1024, 2147483647];
for (var i in divisors) {
  var divisor = divisors[i];
  var mod = Int32Mod(divisor);
  for (var dividend = -2147483648; dividend < 2147483648; dividend += 3999773) {
    assertEquals((dividend % divisor) | 0, mod(dividend));
  }
}
