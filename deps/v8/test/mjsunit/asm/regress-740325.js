// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

assertTrue = function assertTrue() { }
assertFalse = function assertFalse() { }

__v_3 = [];
__v_2 = [];
__v_0 = 0;
__v_2.__defineGetter__(0, function() {
  if (__v_0++ > 2) return;
  gc();
  __v_3.concat(__v_2);
});
__v_2[0];


function __f_2() {
}

(function __f_1() {
  print("1...");
  function __f_5(stdlib, imports) {
    "use asm";
    var __f_2 = imports.__f_2;
    function __f_3(a) {
      a = a | 0;
    }
    return { __f_3:__f_3 };
  }
  var __v_2 = __f_5(this, { __f_2:__f_2 });
;
})();

(function __f_10() {
  print("2...");
  function __f_5() {
    "use asm";
    function __f_3(a) {
    }
  }
  var __v_2 = __f_5();
  assertFalse();
})();

(function __f_11() {
  print("3...");
  let m = (function __f_6() {
    function __f_5() {
      "use asm";
      function __f_3() {
      }
      return { __f_3:__f_3 };
    }
    var __v_2 = __f_5( { __f_2:__f_2 });
  });
  for (var i = 0; i < 30; i++) {
    print("  i = " + i);
    var x = m();
    for (var j = 0; j < 200; j++) {
      try {
        __f_5;
      } catch (e) {
      }
    }
    x;
  }
})();
