// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-inline-array-builtins --opt
// Flags: --no-always-opt

// Early exit from every functions properly.
(() => {
  const a = [1, 2, 3, 4, 5];
  let result = 0;
  function earlyExit() {
    return a.every(v => {
      result += v;
      return v < 2;
    });
  }
  assertFalse(earlyExit());
  earlyExit();
  %OptimizeFunctionOnNextCall(earlyExit);
  assertFalse(earlyExit());
  assertEquals(9, result);
})();

// Soft-deopt plus early exit.
(() => {
  const a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
  let result = 0;
  function softyPlusEarlyExit(deopt) {
     return a.every(v => {
       result += v;
       if (v === 4 && deopt) {
         a.abc = 25;
       }
       return v < 8;
     });
  }
  assertFalse(softyPlusEarlyExit(false));
  softyPlusEarlyExit(false);
  %OptimizeFunctionOnNextCall(softyPlusEarlyExit);
  assertFalse(softyPlusEarlyExit(true));
  assertEquals(36*3, result);
})();

// Soft-deopt synced with early exit, which forces the lazy deoptimization
// continuation handler to exit.
(() => {
  const a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
  let called_values = [];
  function softyPlusEarlyExit(deopt) {
     called_values = [];
     return a.every(v => {
       called_values.push(v);
       if (v === 4 && deopt) {
         a.abc = 25;
         return false;
       }
       return v < 8;
     });
  }
  assertFalse(softyPlusEarlyExit(false));
  assertArrayEquals([1, 2, 3, 4, 5, 6, 7, 8], called_values);
  softyPlusEarlyExit(false);
  %OptimizeFunctionOnNextCall(softyPlusEarlyExit);
  assertFalse(softyPlusEarlyExit(true));
  assertArrayEquals([1, 2, 3, 4], called_values);
})();

// Unknown field access leads to soft-deopt unrelated to every, should still
// lead to correct result.
(() => {
  const a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
             20, 21, 22, 23, 24, 25];
  let result = 0;
  function eagerDeoptInCalled(deopt) {
    return a.every((v, i) => {
      if (i === 13 && deopt) {
        a.abc = 25;
      }
      result += v;
      return true;
    });
  }
  eagerDeoptInCalled();
  eagerDeoptInCalled();
  %OptimizeFunctionOnNextCall(eagerDeoptInCalled);
  eagerDeoptInCalled();
  assertTrue(eagerDeoptInCalled(true));
  eagerDeoptInCalled();
  assertEquals(1625, result);
})();

// Length change detected during loop, must cause properly handled eager deopt.
(() => {
  let called_values;
  function eagerDeoptInCalled(deopt) {
    const a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
    called_values = [];
    return a.every((v,i) => {
      called_values.push(v);
      a.length = (i === 5 && deopt) ? 8 : 10;
      return true;
    });
  }
  assertTrue(eagerDeoptInCalled());
  assertArrayEquals([1, 2, 3, 4, 5, 6, 7, 8, 9, 10], called_values);
  eagerDeoptInCalled();
  %OptimizeFunctionOnNextCall(eagerDeoptInCalled);
  assertTrue(eagerDeoptInCalled());
  assertTrue(eagerDeoptInCalled(true));
  assertArrayEquals([1, 2, 3, 4, 5, 6, 7, 8], called_values);
  eagerDeoptInCalled();
})();

// Lazy deopt from a callback that changes the input array. Deopt in a callback
// execution that returns true.
(() => {
  const a = [1, 2, 3, 4, 5];
  function lazyChanger(deopt) {
    return a.every((v, i) => {
      if (i === 3 && deopt) {
        a[3] = 100;
        %DeoptimizeNow();
       }
      return true;
    });
  }
  assertTrue(lazyChanger());
  lazyChanger();
  %OptimizeFunctionOnNextCall(lazyChanger);
  assertTrue(lazyChanger(true));
  assertTrue(lazyChanger());
})();

// Lazy deopt from a callback that will always return true and no element is
// found. Verifies the lazy-after-callback continuation builtin.
(() => {
  const a = [1, 2, 3, 4, 5];
  function lazyChanger(deopt) {
    return a.every((v, i) => {
      if (i === 3 && deopt) {
        %DeoptimizeNow();
       }
      return true;
    });
  }
  assertTrue(lazyChanger());
  lazyChanger();
  %OptimizeFunctionOnNextCall(lazyChanger);
  assertTrue(lazyChanger(true));
  assertTrue(lazyChanger());
})();

// Lazy deopt from a callback that changes the input array. Deopt in a callback
// execution that returns true.
(() => {
  const a = [1, 2, 3, 4, 5];
  function lazyChanger(deopt) {
    return a.every((v, i) => {
      if (i === 2 && deopt) {
        a[3] = 100;
        %DeoptimizeNow();
       }
      return true;
    });
  }
  assertTrue(lazyChanger());
  lazyChanger();
  %OptimizeFunctionOnNextCall(lazyChanger);
  assertTrue(lazyChanger(true));
  assertTrue(lazyChanger());
})();

// Escape analyzed array
(() => {
  let result = 0;
  function eagerDeoptInCalled(deopt) {
    const a_noescape = [0, 1, 2, 3, 4, 5];
    a_noescape.every((v, i) => {
      result += v | 0;
      if (i === 13 && deopt) {
        a_noescape.length = 25;
      }
      return true;
    });
  }
  eagerDeoptInCalled();
  eagerDeoptInCalled();
  %OptimizeFunctionOnNextCall(eagerDeoptInCalled);
  eagerDeoptInCalled();
  eagerDeoptInCalled(true);
  eagerDeoptInCalled();
  assertEquals(75, result);
})();

// Lazy deopt from runtime call from inlined callback function.
(() => {
  const a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
             20, 21, 22, 23, 24, 25];
  let result = 0;
  function lazyDeopt(deopt) {
    a.every((v, i) => {
      result += i;
      if (i === 13 && deopt) {
        %DeoptimizeNow();
      }
      return true;
    });
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
(() => {
  const a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
             20, 21, 22, 23, 24, 25];
  let result = 0;
  function lazyDeopt(deopt) {
    function callback(v, i) {
      result += i;
      if (i === 13 && deopt) {
        %DeoptimizeNow();
      }
      return true;
    }
    %NeverOptimizeFunction(callback);
    a.every(callback);
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
  lazyDeopt(true);
  lazyDeopt();
  assertEquals(1500, result);
})();

// Call to a.every is done inside a try-catch block and the callback function
// being called actually throws.
(() => {
  const a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
             20, 21, 22, 23, 24, 25];
  let caught = false;
  function lazyDeopt(deopt) {
    try {
      a.every((v, i) => {
        if (i === 1 && deopt) {
          throw("a");
        }
        return true;
      });
    } catch (e) {
      caught = true;
    }
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
  assertDoesNotThrow(() => lazyDeopt(true));
  assertTrue(caught);
  lazyDeopt();
})();

// Call to a.every is done inside a try-catch block and the callback function
// being called actually throws, but the callback is not inlined.
(() => {
  let a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
  let caught = false;
  function lazyDeopt(deopt) {
    function callback(v, i) {
      if (i === 1 && deopt) {
        throw("a");
      }
      return true;
    }
    %NeverOptimizeFunction(callback);
    try {
      a.every(callback);
    } catch (e) {
      caught = true;
    }
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
  assertDoesNotThrow(() => lazyDeopt(true));
  assertTrue(caught);
  lazyDeopt();
})();

// Call to a.every is done inside a try-catch block and the callback function
// being called throws into a deoptimized caller function.
(function TestThrowIntoDeoptimizedOuter() {
  const a = [1, 2, 3, 4];
  function lazyDeopt(deopt) {
    function callback(v, i) {
      if (i === 1 && deopt) {
        %DeoptimizeFunction(lazyDeopt);
        throw "some exception";
      }
      return true;
    }
    %NeverOptimizeFunction(callback);
    let result = 0;
    try {
      result = a.every(callback);
    } catch (e) {
      assertEquals("some exception", e);
      result = "nope";
    }
    return result;
  }
  assertEquals(true, lazyDeopt(false));
  assertEquals(true, lazyDeopt(false));
  assertEquals("nope", lazyDeopt(true));
  assertEquals("nope", lazyDeopt(true));
  %OptimizeFunctionOnNextCall(lazyDeopt);
  assertEquals(true, lazyDeopt(false));
  assertEquals("nope", lazyDeopt(true));
})();

// An error generated inside the callback includes every in it's
// stack trace.
(() => {
  const re = /Array\.every/;
  function lazyDeopt(deopt) {
    const b = [1, 2, 3];
    let result = 0;
    b.every((v, i) => {
      result += v;
      if (i === 1) {
        const e = new Error();
        assertTrue(re.exec(e.stack) !== null);
      }
      return true;
    });
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
})();

// An error generated inside a non-inlined callback function also
// includes every in it's stack trace.
(() => {
  const re = /Array\.every/;
  function lazyDeopt(deopt) {
    const b = [1, 2, 3];
    let did_assert_error = false;
    let result = 0;
    function callback(v, i) {
      result += v;
      if (i === 1) {
        const e = new Error();
        assertTrue(re.exec(e.stack) !== null);
        did_assert_error = true;
      }
      return true;
    }
    %NeverOptimizeFunction(callback);
    b.every(callback);
    return did_assert_error;
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  assertTrue(lazyDeopt());
})();

// An error generated inside a recently deoptimized callback function
// includes every in it's stack trace.
(() => {
  const re = /Array\.every/;
  function lazyDeopt(deopt) {
    const b = [1, 2, 3];
    let did_assert_error = false;
    let result = 0;
    b.every((v, i) => {
      result += v;
      if (i === 1) {
        %DeoptimizeNow();
      } else if (i === 2) {
        const e = new Error();
        assertTrue(re.exec(e.stack) !== null);
        did_assert_error = true;
      }
      return true;
    });
    return did_assert_error;
  }
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  assertTrue(lazyDeopt());
})();

// Verify that various exception edges are handled appropriately.
// The thrown Error object should always indicate it was created from
// an every call stack.
(() => {
  const re = /Array\.every/;
  const a = [1, 2, 3];
  let result = 0;
  function lazyDeopt() {
    a.every((v, i) => {
      result += i;
      if (i === 1) {
        %DeoptimizeFunction(lazyDeopt);
        throw new Error();
      }
      return true;
    });
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
    a.every(v => {
      callback_values.push(v);
      return true;
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
    a.every(v => {
      callback_values.push(v);
      return true;
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
    return a.every(x => true, side_effect(a, b));
  }

  let a = [1, 2, 3];
  unreliable(a, false);
  unreliable(a, false);
  %OptimizeFunctionOnNextCall(unreliable);
  unreliable(a, false);
  // Now actually do change the map.
  unreliable(a, true);
})();

// Handle callback is not callable.
(() => {
  const a = [1, 2, 3, 4, 5];
  function notCallable() {
    return a.every(undefined);
  }

  assertThrows(notCallable, TypeError);
  try { notCallable(); } catch(e) { }
  %OptimizeFunctionOnNextCall(notCallable);
  assertThrows(notCallable, TypeError);
})();

// Messing with the Array prototype causes deoptimization.
(() => {
  const a = [1, 2, 3];
  let result = 0;
  function prototypeChanged() {
    a.every((v, i) => {
      result += v;
      return true;
    });
  }
  prototypeChanged();
  prototypeChanged();
  %OptimizeFunctionOnNextCall(prototypeChanged);
  prototypeChanged();
  a.constructor = {};
  prototypeChanged();
  assertUnoptimized(prototypeChanged);
  assertEquals(24, result);
})();
