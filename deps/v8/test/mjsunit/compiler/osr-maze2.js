// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --use-osr

function bar() {
  var sum = 11;
  var i = 35;
  while (i-- > 31) {
    LOOP1();
    j = 9;
    while (j-- > 7) {
      LOOP2();
      sum = sum + j * 5;
      var k = 7;
      while (k-- > 5) {
        LOOP3();
        sum = sum + j * 5;
      }
    }
  }
  while (i-- > 29) {
    LOOP4();
    while (j-- > 3) {
      LOOP5();
      var k = 10;
      while (k-- > 8) {
        LOOP6();
        sum = sum + k * 11;
      }
    }
    while (j-- > 1) {
      LOOP7();
      var k = 8;
      while (k-- > 6) {
        LOOP8();
        var m = 9;
        while (m-- > 6) {
          LOOP9();
          sum = sum + k * 13;
        }
      }
    }
  }
  return sum;
}

function gen(i) {
  var body = bar.toString();
  body = body.replace(new RegExp("bar"), "bar" + i);
  for (var j = 1; j < 10; j++) {
    var r = new RegExp("LOOP" + j + "\\(\\);");
    if (i == j) body = body.replace(r, "%OptimizeOsr(); %PrepareFunctionForOptimization(bar" + i +");");
    else body = body.replace(r, "");
  }
  return eval("(" + body + ")");
}

for (var i = 1; i < 10; i++) {
  var f = gen(i);
  %PrepareFunctionForOptimization(f);
  assertEquals(1979, f());
}
