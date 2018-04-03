// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function g(y) { assertEquals(y, 12); }

var X = 0;

function foo () {
  var cnt = 0;
  var l = -1;
  var x = 0;
  while (1) switch (l) {
      case -1:
        var y = x + 12;
        l = 0;
        break;
      case 0:
        if (cnt++ == 5) {
          %OptimizeOsr();
          l = 1;
        }
        break;
      case 1:
        // This case will contain deoptimization
        // because it has no type feedback.
        g(y);
        return;
    };
}

foo();
