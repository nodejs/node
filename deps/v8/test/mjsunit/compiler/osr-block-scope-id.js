// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --use-osr

"use strict";

function foo() {
  var result = new Array();
  var out;
  {
    let sum = 0;
    for (var i = 0; i < 10; i++) {
      {
        let x = i;
        if (i == 5) %OptimizeOsr();
        sum += i;
        result.push(function() { return x; });
      }
    }
    out = sum;
  }
  result.push(out);
  return result;
}


function check() {
  var r = foo();
  assertEquals(45, r.pop());
  for (var i = 9; i >= 0; i--) {
    assertEquals(i, r.pop()());
  }
  assertEquals(0, r.length);
}

check();
check();
check();
