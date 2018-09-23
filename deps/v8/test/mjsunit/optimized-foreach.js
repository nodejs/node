// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --turbo-inline-array-builtins

var a = [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,0,0];
var b = [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25];
var c = [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25];

// Unknown field access leads to soft-deopt unrelated to forEach, should still
// lead to correct result.
(function() {
  var result = 0;
  var eagerDeoptInCalled = function(deopt) {
    var sum = function(v,i,o) {
      result += v;
      if (i == 13 && deopt) {
        a.abc = 25;
      }
    }
    a.forEach(sum);
  }
  eagerDeoptInCalled();
  eagerDeoptInCalled();
  %OptimizeFunctionOnNextCall(eagerDeoptInCalled);
  eagerDeoptInCalled();
  eagerDeoptInCalled(true);
  eagerDeoptInCalled();
  assertEquals(1500, result);
})();

// Length change detected during loop, must cause properly handled eager deopt.
(function() {
  var result = 0;
  var eagerDeoptInCalled = function(deopt) {
    var sum = function(v,i,o) {
      result += v;
      a.length = (i == 13 && deopt) ? 25 : 27;
    }
    a.forEach(sum);
  }
  eagerDeoptInCalled();
  eagerDeoptInCalled();
  %OptimizeFunctionOnNextCall(eagerDeoptInCalled);
  eagerDeoptInCalled();
  eagerDeoptInCalled(true);
  eagerDeoptInCalled();
  assertEquals(1500, result);
})();

// Escape analyzed array
(function() {
  var result = 0;
  var eagerDeoptInCalled = function(deopt) {
    var a_noescape = [0,1,2,3,4,5];
    var sum = function(v,i,o) {
      result += v;
      if (i == 13 && deopt) {
        a_noescape.length = 25;
      }
    }
    a_noescape.forEach(sum);
  }
  eagerDeoptInCalled();
  eagerDeoptInCalled();
  %OptimizeFunctionOnNextCall(eagerDeoptInCalled);
  eagerDeoptInCalled();
  eagerDeoptInCalled(true);
  eagerDeoptInCalled();
  assertEquals(75, result);
})();

// Escape analyzed array where sum function isn't inlined, forcing a lazy deopt
// with GC that relies on the stashed-away return result fro the lazy deopt
// being properly stored in a place on the stack that gets GC'ed.
(function() {
  var result = 0;
  var lazyDeopt = function(deopt) {
    var b = [1,2,3];
    var sum = function(v,i,o) {
      result += i;
      if (i == 1 && deopt) {
        %DeoptimizeFunction(lazyDeopt);
      }
      gc(); gc();
    };
    %NeverOptimizeFunction(sum);
    b.forEach(sum);
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
  lazyDeopt(true);
  lazyDeopt();
})();

// Lazy deopt from runtime call from inlined callback function.
(function() {
  var result = 0;
  var lazyDeopt = function(deopt) {
    var sum = function(v,i,o) {
      result += i;
      if (i == 13 && deopt) {
          %DeoptimizeNow();
      }
    }
    b.forEach(sum);
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
  lazyDeopt(true);
  lazyDeopt();
  assertEquals(1500, result);
})();

// Lazy deopt from runtime call from non-inline callback function.
(function() {
  var result = 0;
  var lazyDeopt = function(deopt) {
    var sum = function(v,i,o) {
      result += i;
      if (i == 13 && deopt) {
          %DeoptimizeNow();
      }
    };
    %NeverOptimizeFunction(sum);
    b.forEach(sum);
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
  lazyDeopt(true);
  lazyDeopt();
  assertEquals(1500, result);
})();

(function() {
  var result = 0;
  var lazyDeopt = function(deopt) {
    var sum = function(v,i,o) {
      result += i;
      if (i == 13 && deopt) {
          %DeoptimizeNow();
          gc();
          gc();
          gc();
      }
    }
    c.forEach(sum);
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
  lazyDeopt(true);
  lazyDeopt();
  assertEquals(1500, result);
})();

// Call to a.forEach is done inside a try-catch block and the callback function
// being called actually throws.
(function() {
  var caught = false;
  var result = 0;
  var lazyDeopt = function(deopt) {
    var sum = function(v,i,o) {
      result += i;
      if (i == 1 && deopt) {
        throw("a");
      }
    }
    try {
      c.forEach(sum);
    } catch (e) {
      caught = true;
    }
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
  assertDoesNotThrow(lazyDeopt.bind(this, true));
  assertTrue(caught);
  lazyDeopt();
})();

// Call to a.forEach is done inside a try-catch block and the callback function
// being called actually throws, but the callback is not inlined.
(function() {
  var caught = false;
  var result = 0;
  var lazyDeopt = function(deopt) {
    var sum = function(v,i,o) {
      result += i;
      if (i == 1 && deopt) {
        throw("a");
      }
    };
    %NeverOptimizeFunction(sum);
    try {
      c.forEach(sum);
    } catch (e) {
      caught = true;
    }
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
  assertDoesNotThrow(lazyDeopt.bind(this, true));
  assertTrue(caught);
  lazyDeopt();
})();

// Call to a.forEach is done inside a try-catch block and the callback function
// being called throws into a deoptimized caller function.
(function TestThrowIntoDeoptimizedOuter() {
  var a = [1,2,3,4];
  var lazyDeopt = function(deopt) {
    var sum = function(v,i,o) {
      result += v;
      if (i == 1 && deopt) {
        %DeoptimizeFunction(lazyDeopt);
        throw "some exception";
      }
    };
    %NeverOptimizeFunction(sum);
    var result = 0;
    try {
      a.forEach(sum);
    } catch (e) {
      assertEquals("some exception", e)
      result += 100;
    }
    return result;
  }
  assertEquals(10, lazyDeopt(false));
  assertEquals(10, lazyDeopt(false));
  assertEquals(103, lazyDeopt(true));
  assertEquals(103, lazyDeopt(true));
  %OptimizeFunctionOnNextCall(lazyDeopt);
  assertEquals(10, lazyDeopt(false));
  assertEquals(103, lazyDeopt(true));
})();

(function() {
  var re = /Array\.forEach/;
  var lazyDeopt = function foobar(deopt) {
    var b = [1,2,3];
    var result = 0;
    var sum = function(v,i,o) {
      result += v;
      if (i == 1) {
        var e = new Error();
        print(e.stack);
        assertTrue(re.exec(e.stack) !== null);
      }
    };
    var o = [1,2,3];
    b.forEach(sum);
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
})();

(function() {
  var re = /Array\.forEach/;
  var lazyDeopt = function(deopt) {
    var b = [1,2,3];
    var result = 0;
    var sum = function(v,i,o) {
      result += v;
      if (i == 1) {
        var e = new Error();
        assertTrue(re.exec(e.stack) !== null);
      }
    };
    %NeverOptimizeFunction(sum);
    var o = [1,2,3];
    b.forEach(sum);
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
})();

(function() {
  var re = /Array\.forEach/;
  var lazyDeopt = function(deopt) {
    var b = [1,2,3];
    var result = 0;
    var sum = function(v,i,o) {
      result += v;
      if (i == 1) {
        %DeoptimizeNow();
      } else if (i == 2) {
        var e = new Error();
        assertTrue(re.exec(e.stack) !== null);
      }
    };
    var o = [1,2,3];
    b.forEach(sum);
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
})();

(function() {
  var re = /Array\.forEach/;
  var a = [1,2,3];
  var result = 0;
  var lazyDeopt = function() {
    var sum = function(v,i,o) {
      result += i;
      if (i == 1) {
        %DeoptimizeFunction(lazyDeopt);
        throw new Error();
      }
    };
    a.forEach(sum);
  }
  assertThrows(() => lazyDeopt());
  assertThrows(() => lazyDeopt());
  try {
    lazyDeopt();
  } catch (e) {
    assertTrue(re.exec(e.stack) !== null);
  }
  %OptimizeFunctionOnNextCall(lazyDeopt);
  try {
    lazyDeopt();
  } catch (e) {
    assertTrue(re.exec(e.stack) !== null);
  }
})();

// Verify holes are skipped.
(() => {
  const a = [1, 2, , 3, 4];
  function withHoles() {
    const callback_values = [];
    a.forEach(v => {
      callback_values.push(v);
    });
    return callback_values;
  }
  withHoles();
  withHoles();
  %OptimizeFunctionOnNextCall(withHoles);
  assertArrayEquals([1, 2, 3, 4], withHoles());
})();

(() => {
  const a = [1.5, 2.5, , 3.5, 4.5];
  function withHoles() {
    const callback_values = [];
    a.forEach(v => {
      callback_values.push(v);
    });
    return callback_values;
  }
  withHoles();
  withHoles();
  %OptimizeFunctionOnNextCall(withHoles);
  assertArrayEquals([1.5, 2.5, 3.5, 4.5], withHoles());
})();

// Ensure that we handle side-effects between load and call.
(() => {
  function side_effect(a, b) { if (b) a.foo = 3; return a; }
  %NeverOptimizeFunction(side_effect);

  function unreliable(a, b) {
    let sum = 0;
    return a.forEach(x => sum += x, side_effect(a, b));
  }

  let a = [1, 2, 3];
  unreliable(a, false);
  unreliable(a, false);
  %OptimizeFunctionOnNextCall(unreliable);
  unreliable(a, false);
  // Now actually do change the map.
  unreliable(a, true);
})();
