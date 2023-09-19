// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --turbo-inline-array-builtins
// Flags: --turbofan --no-always-turbofan

// Unknown field access leads to eager-deopt unrelated to reduceright, should
// still lead to correct result.
(() => {
  const a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
  // For this particular eager deopt point to work, we need to dodge
  // TurboFan's soft-deopts through a non-inlined and non-optimized function
  // call to foo().
  function foo(o, deopt) {
    if (deopt) {
      o.abc = 3;
    }
  }
  %NeverOptimizeFunction(foo);
  function eagerDeoptInCalled(deopt) {
    return a.reduceRight((r, v, i, o) => {
      if (i === 7) {
        foo(a, deopt);
      }
      return r + "S";
    }, "H");
  };
  %PrepareFunctionForOptimization(eagerDeoptInCalled);
  eagerDeoptInCalled();
  eagerDeoptInCalled();
  %OptimizeFunctionOnNextCall(eagerDeoptInCalled);
  eagerDeoptInCalled();
  assertEquals("HSSSSSSSSSS", eagerDeoptInCalled(true));
})();

// Make sure we gracefully handle the case of an empty array in
// optimized code.
(function() {
var nothingThere = function(only_holes) {
  var a = [1, 2, , 3];  // holey smi array.
  if (only_holes) {
    a = [
      ,
      ,
      ,
    ];  // also a holey smi array.
  }
  return a.reduceRight((r, v, i, o) => r + v);
};
;
%PrepareFunctionForOptimization(nothingThere);
nothingThere();
nothingThere();
%OptimizeFunctionOnNextCall(nothingThere);
assertThrows(() => nothingThere(true));
})();

// An error generated inside the callback includes reduce in it's
// stack trace.
(function() {
var re = /Array\.reduceRight/;
var alwaysThrows = function() {
  var b = [
    ,
    ,
    ,
  ];
  var result = 0;
  var callback = function(r, v, i, o) {
    return r + v;
  };
  b.reduceRight(callback);
};
;
%PrepareFunctionForOptimization(alwaysThrows);
try {
  alwaysThrows();
} catch (e) {
  assertTrue(re.exec(e.stack) !== null);
}
try {
  alwaysThrows();
} catch (e) {
}
try {
  alwaysThrows();
} catch (e) {
}
%OptimizeFunctionOnNextCall(alwaysThrows);
try {
  alwaysThrows();
} catch (e) {
  assertTrue(re.exec(e.stack) !== null);
}
})();
