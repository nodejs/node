// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --use-osr --allow-natives-syntax

var global_counter = 0;

function thrower() {
  var x = global_counter++;
  if (x == 5)  %OptimizeOsr(1);
  if (x == 10) throw "terminate";
}

%NeverOptimizeFunction(thrower);  // Don't want to inline the thrower.
%NeverOptimizeFunction(test);     // Don't want to inline the func into test.

function test(func) {
  for (var i = 0; i < 3; i++) {
    global_counter = 0;
    assertThrows(func);
  }
}

function n1() {
  while (true) thrower();
}

function n2() {
  while (true) while (true) thrower();
}

function n3() {
  while (true) while (true) while (true) thrower();
}

function n4() {
  while (true) while (true) while (true) while (true) thrower();
}

function b1(a) {
  while (true) {
    thrower();
    if (a) break
  }
}


function b2(a) {
  while (true) {
    while (true) {
      thrower();
      if (a) break
    }
  }
}


function b3(a) {
  while (true) {
    while (true) {
      while (true) {
        thrower();
        if (a) break
      }
      if (a) break
    }
  }
}


test(n1);
test(n2);
test(n3);
test(n4);
test(b1);
test(b2);
test(b3);
