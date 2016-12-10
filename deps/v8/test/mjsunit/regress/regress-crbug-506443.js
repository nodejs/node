// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

assertSame = function assertSame() {
  if (found === expected) {
    if (1 / expected) return;
  } else if ((expected !== expected) && (found !== found)) {
    return;
  };
};
assertEquals = function assertEquals() {
  if (expected) {;
  }
};
assertArrayEquals = function assertArrayEquals() {
  var start = "";
  if (name_opt) {
    start = name_opt + " - ";
  };
  if (expected.length == found.length) {
    for (var i = 0; i < expected.length; ++i) {;
    }
  }
};
assertPropertiesEqual = function assertPropertiesEqual() {
  if (found) {;
  }
};
assertToStringEquals = function assertToStringEquals() {
  if (found) {;
  }
};
assertTrue = function assertTrue() {;
};
assertFalse = function assertFalse() {;
};
assertUnreachable = function assertUnreachable() {
  var message = "Fail" + "ure: unreachable";
  if (name_opt) {
    message += " - " + name_opt;
  }
};
OptimizationStatus = function() {}
assertUnoptimized = function assertUnoptimized() {;
}
assertOptimized = function assertOptimized() {;
}
triggerAssertFalse = function() {}
var __v_2 = {};
var __v_3 = {};
var __v_4 = {};
var __v_5 = {};
var __v_6 = 1073741823;
var __v_7 = {};
var __v_8 = {};
var __v_9 = {};
var __v_10 = {};
var __v_11 = 2147483648;
var __v_12 = 1073741823;
var __v_13 = {};
var __v_14 = {};
var __v_15 = -2147483648;
var __v_16 = {};
var __v_17 = {};
var __v_19 = {};
var __v_20 = {};
var __v_21 = {};
var __v_22 = {};
var __v_23 = {};
var __v_24 = {};
try {
  (function() {
    var Debug = %GetDebugContext().Debug;

    function __f_0() {}
    for (var __v_0 = 0; __v_0 < 3; __v_0++) {
      var __v_2 = function() {
        a = 1;
      }
      Debug.setListener(__f_0);
      if (__v_0 < 2) Debug.setBreakPoint(__v_2);
    }
  })();
} catch (e) {
  print();
}
