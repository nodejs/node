// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --use-osr

"use strict";

function test(expected, func) {
  assertEquals(expected, func());
  assertEquals(expected, func());
  assertEquals(expected, func());
}

function foo() {
  var result = 0;
  {
    let x = 0;
    var temp_x = x;
    var first = 1;
    outer: while (true) {
      let x = temp_x;
      if (first == 1) first = 0;
      else x = x + 1 | 0;
      var flag = 1;
      for (; flag == 1; (flag = 0, temp_x = x)) {
        if (x < 2) {
          result = x; %OptimizeOsr();
        } else {
          break outer;
        }
      }
      if (flag == 1) break;
    }
  }
  return result;
}
%PrepareFunctionForOptimization(foo);

test(1, foo);


function smo() {
  var result = 0;
  {
    let x = 11;
    outer: while (true) {
      let y = x;
      for (var i = 0; i < 5; i++) {
        %OptimizeOsr();
        if (i) break outer;
        else result = y;
      }
    }
  }
  return result;
}
%PrepareFunctionForOptimization(smo);

test(11, smo);
