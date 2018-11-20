// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var stdlib = {};
var foreign = {};
var heap = new ArrayBuffer(64 * 1024);

function Uint32Div(divisor) {
  var name = "div_";
  name += divisor;
  var m = eval("function Module(stdlib, foreign, heap) {\n"
      + " \"use asm\";\n"
      + " function " + name + "(dividend) {\n"
      + "  return ((dividend >>> 0) / " + divisor + ") >>> 0;\n"
      + " }\n"
      + " return { f: " + name + "}\n"
      + "}; Module");
  return m(stdlib, foreign, heap).f;
}

var divisors = [0, 1, 3, 4, 10, 42, 64, 100, 1024, 2147483647, 4294967295];
for (var i in divisors) {
  var divisor = divisors[i];
  var div = Uint32Div(divisor);
  for (var dividend = 0; dividend < 4294967296; dividend += 3999773) {
    assertEquals((dividend / divisor) >>> 0, div(dividend));
  }
}

var div = (function(stdlib, foreign, heap) {
  "use asm";
  function div(dividend, divisor) {
    return (dividend >>> 0) / (divisor >>> 0) | 0;
  }
  return {div: div};
})(stdlib, foreign, heap).div;

for (var i in divisors) {
  var divisor =  divisors[i];
  for (var dividend = 0; dividend < 4294967296; dividend += 3999773) {
    assertEquals((dividend >>> 0) / (divisor >>> 0) | 0,
                 div(dividend, divisor));
  }
}
