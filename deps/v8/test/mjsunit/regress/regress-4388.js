// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-filter=test*

// Tests that TurboFan emits a dynamic hole-check for the temporal dead zone at
// a non-initializing assignments to a {let} variable.
function test_hole_check_for_let(a) {
  'use strict';
  { switch (a) {
      case 0: let x;
      case 1: x = 9;
    }
  }
}
assertDoesNotThrow("test_hole_check_for_let(0)");
assertThrows("test_hole_check_for_let(1)", ReferenceError);
%OptimizeFunctionOnNextCall(test_hole_check_for_let)
assertThrows("test_hole_check_for_let(1)", ReferenceError);

// Tests that TurboFan emits a dynamic hole-check for the temporal dead zone at
// a non-initializing assignments to a {const} variable.
function test_hole_check_for_const(a) {
  'use strict';
  { switch (a) {
      case 0: const x = 3;
      case 1: x = 2;
    }
  }
}
assertThrows("test_hole_check_for_const(0)", TypeError);
assertThrows("test_hole_check_for_const(1)", ReferenceError);
%OptimizeFunctionOnNextCall(test_hole_check_for_const)
assertThrows("test_hole_check_for_const(1)", ReferenceError);
