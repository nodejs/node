// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --use-osr

var counter = 188;

function gen(w) {  // defeat compiler cache.
 var num = counter++;
  var Z = [ "", "", "", ];
  Z[w] = "%OptimizeOsr()";
  var src =
    "function f" + num + "(a,b,c) {" +
    "  var x = 0;" +
    "  var y = 0;" +
    "  var z = 0;" +
    "  while (a > 0) { " + Z[0] + "; x += 19; a--; var j=2; while(j--); }" +
    "  while (b > 0) { " + Z[1] + "; y += 23; b--; var j=2; while(j--); }" +
    "  while (c > 0) { " + Z[2] + "; z += 29; c--; var j=2; while(j--); }" +
    "  return x + y + z;" +
    "} f" + num;
  return eval(src);
}

function check(x,a,b,c) {
  for (var i = 0; i < 3; i++) {
    var f = gen(i);
    %PrepareFunctionForOptimization(f);
    assertEquals(x, f(a, b, c));
  }
}

check(213, 3,3,3);
check(365, 4,5,6);
check(6948, 99,98,97);
