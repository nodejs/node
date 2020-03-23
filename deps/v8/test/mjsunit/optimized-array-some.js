// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-inline-array-builtins --opt
// Flags: --no-always-opt --no-lazy-feedback-allocation

// TODO(v8:10195): Fix these tests s.t. we assert deoptimization occurs when
// expected (e.g. in a %DeoptimizeNow call), then remove
// --no-lazy-feedback-allocation.

// Early exit from some functions properly.
(() => {
  const a = [1, 2, 3, 4, 5];
  let result = 0;
  function earlyExit() {
    return a.some(v => {
      result += v;
      return v > 2;
    });
  }
  %PrepareFunctionForOptimization(earlyExit);
  assertTrue(earlyExit());
  earlyExit();
  %OptimizeFunctionOnNextCall(earlyExit);
  assertTrue(earlyExit());
  assertEquals(18, result);
})();

// Soft-deopt plus early exit.
(() => {
  const a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
  let result = 0;
  function softyPlusEarlyExit(deopt) {
     return a.some(v => {
       result += v;
       if (v === 4 && deopt) {
         a.abc = 25;
       }
       return v > 7;
     });
  }
  %PrepareFunctionForOptimization(softyPlusEarlyExit);
  assertTrue(softyPlusEarlyExit(false));
  softyPlusEarlyExit(false);
  %OptimizeFunctionOnNextCall(softyPlusEarlyExit);
  assertTrue(softyPlusEarlyExit(true));
  assertEquals(36*3, result);
})();

// Soft-deopt synced with early exit, which forces the lazy deoptimization
// continuation handler to exit.
(() => {
  const a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
  let called_values = [];
  function softyPlusEarlyExit(deopt) {
     called_values = [];
     return a.some(v => {
       called_values.push(v);
       if (v === 4 && deopt) {
         a.abc = 25;
         return true;
       }
       return v > 7;
     });
  }
  %PrepareFunctionForOptimization(softyPlusEarlyExit);
  assertTrue(softyPlusEarlyExit(false));
  assertArrayEquals([1, 2, 3, 4, 5, 6, 7, 8], called_values);
  softyPlusEarlyExit(false);
  %OptimizeFunctionOnNextCall(softyPlusEarlyExit);
  assertTrue(softyPlusEarlyExit(true));
  assertArrayEquals([1, 2, 3, 4], called_values);
})();

// Unknown field access leads to soft-deopt unrelated to some, should still
// lead to correct result.
(() => {
  const a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
             20, 21, 22, 23, 24, 25];
  let result = 0;
  function eagerDeoptInCalled(deopt) {
    return a.some((v, i) => {
      if (i === 13 && deopt) {
        a.abc = 25;
      }
      result += v;
      return false;
    });
  }
  %PrepareFunctionForOptimization(eagerDeoptInCalled);
  eagerDeoptInCalled();
  eagerDeoptInCalled();
  %OptimizeFunctionOnNextCall(eagerDeoptInCalled);
  eagerDeoptInCalled();
  assertFalse(eagerDeoptInCalled(true));
  eagerDeoptInCalled();
  assertEquals(1625, result);
})();

// Length change detected during loop, must cause properly handled eager deopt.
(() => {
  let called_values;
  function eagerDeoptInCalled(deopt) {
    const a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
    called_values = [];
    return a.some((v,i) => {
      called_values.push(v);
      a.length = (i === 5 && deopt) ? 8 : 10;
      return false;
    });
  }
  %PrepareFunctionForOptimization(eagerDeoptInCalled);
  assertFalse(eagerDeoptInCalled());
  assertArrayEquals([1, 2, 3, 4, 5, 6, 7, 8, 9, 10], called_values);
  eagerDeoptInCalled();
  %OptimizeFunctionOnNextCall(eagerDeoptInCalled);
  assertFalse(eagerDeoptInCalled());
  assertFalse(eagerDeoptInCalled(true));
  assertArrayEquals([1, 2, 3, 4, 5, 6, 7, 8], called_values);
  eagerDeoptInCalled();
})();

// Lazy deopt from a callback that changes the input array. Deopt in a callback
// execution that returns true.
(() => {
  const a = [1, 2, 3, 4, 5];
  function lazyChanger(deopt) {
    return a.some((v, i) => {
      if (i === 3 && deopt) {
        a[3] = 100;
        %DeoptimizeNow();
       }
      return false;
    });
  }
  %PrepareFunctionForOptimization(lazyChanger);
  assertFalse(lazyChanger());
  lazyChanger();
  %OptimizeFunctionOnNextCall(lazyChanger);
  assertFalse(lazyChanger(true));
  assertFalse(lazyChanger());
})();

// Lazy deopt from a callback that will always return false and no element is
// found. Verifies the lazy-after-callback continuation builtin.
(() => {
  const a = [1, 2, 3, 4, 5];
  function lazyChanger(deopt) {
    return a.some((v, i) => {
      if (i === 3 && deopt) {
        %DeoptimizeNow();
       }
      return false;
    });
  }
  %PrepareFunctionForOptimization(lazyChanger);
  assertFalse(lazyChanger());
  lazyChanger();
  %OptimizeFunctionOnNextCall(lazyChanger);
  assertFalse(lazyChanger(true));
  assertFalse(lazyChanger());
})();

// Lazy deopt from a callback that changes the input array. Deopt in a callback
// execution that returns false.
(() => {
  const a = [1, 2, 3, 4, 5];
  function lazyChanger(deopt) {
    return a.every((v, i) => {
      if (i === 2 && deopt) {
        a[3] = 100;
        %DeoptimizeNow();
       }
      return false;
    });
  }
  %PrepareFunctionForOptimization(lazyChanger);
  assertFalse(lazyChanger());
  lazyChanger();
  %OptimizeFunctionOnNextCall(lazyChanger);
  assertFalse(lazyChanger(true));
  assertFalse(lazyChanger());
})();

// Escape analyzed array
(() => {
  let result = 0;
  function eagerDeoptInCalled(deopt) {
    const a_noescape = [0, 1, 2, 3, 4, 5];
    a_noescape.some((v, i) => {
      result += v | 0;
      if (i === 13 && deopt) {
        a_noescape.length = 25;
      }
      return false;
    });
  }
  %PrepareFunctionForOptimization(eagerDeoptInCalled);
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
    a.some((v, i) => {
      result += i;
      if (i === 13 && deopt) {
        %DeoptimizeNow();
      }
      return false;
    });
  }
  %PrepareFunctionForOptimization(lazyDeopt);
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
      return false;
    }
    %NeverOptimizeFunction(callback);
    a.some(callback);
  }
  %PrepareFunctionForOptimization(lazyDeopt);
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
  lazyDeopt(true);
  lazyDeopt();
  assertEquals(1500, result);
})();

// Call to a.some is done inside a try-catch block and the callback function
// being called actually throws.
(() => {
  const a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
             20, 21, 22, 23, 24, 25];
  let caught = false;
  function lazyDeopt(deopt) {
    try {
      a.some((v, i) => {
        if (i === 1 && deopt) {
          throw("a");
        }
        return false;
      });
    } catch (e) {
      caught = true;
    }
  }
  %PrepareFunctionForOptimization(lazyDeopt);
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
  assertDoesNotThrow(() => lazyDeopt(true));
  assertTrue(caught);
  lazyDeopt();
})();

// Call to a.some is done inside a try-catch block and the callback function
// being called actually throws, but the callback is not inlined.
(() => {
  let a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
  let caught = false;
  function lazyDeopt(deopt) {
    function callback(v, i) {
      if (i === 1 && deopt) {
        throw("a");
      }
      return false;
    }
    %NeverOptimizeFunction(callback);
    try {
      a.some(callback);
    } catch (e) {
      caught = true;
    }
  }
  %PrepareFunctionForOptimization(lazyDeopt);
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
  assertDoesNotThrow(() => lazyDeopt(true));
  assertTrue(caught);
  lazyDeopt();
})();

// Call to a.some is done inside a try-catch block and the callback function
// being called throws into a deoptimized caller function.
(function TestThrowIntoDeoptimizedOuter() {
  const a = [1, 2, 3, 4];
  function lazyDeopt(deopt) {
    function callback(v, i) {
      if (i === 1 && deopt) {
        %DeoptimizeFunction(lazyDeopt);
        throw "some exception";
      }
      return false;
    }
    %NeverOptimizeFunction(callback);
    let result = 0;
    try {
      result = a.some(callback);
    } catch (e) {
      assertEquals("some exception", e);
      result = "nope";
    }
    return result;
  }
  %PrepareFunctionForOptimization(lazyDeopt);
  assertEquals(false, lazyDeopt(false));
  assertEquals(false, lazyDeopt(false));
  assertEquals("nope", lazyDeopt(true));
  assertEquals("nope", lazyDeopt(true));
  %OptimizeFunctionOnNextCall(lazyDeopt);
  assertEquals(false, lazyDeopt(false));
  assertEquals("nope", lazyDeopt(true));
})();

// An error generated inside the callback includes some in it's
// stack trace.
(() => {
  const re = /Array\.some/;
  function lazyDeopt(deopt) {
    const b = [1, 2, 3];
    let result = 0;
    b.some((v, i) => {
      result += v;
      if (i === 1) {
        const e = new Error();
        assertTrue(re.exec(e.stack) !== null);
      }
      return false;
    });
  }
  %PrepareFunctionForOptimization(lazyDeopt);
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  lazyDeopt();
})();

// An error generated inside a non-inlined callback function also
// includes some in it's stack trace.
(() => {
  const re = /Array\.some/;
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
      return false;
    }
    %NeverOptimizeFunction(callback);
    b.some(callback);
    return did_assert_error;
  }
  %PrepareFunctionForOptimization(lazyDeopt);
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  assertTrue(lazyDeopt());
})();

// An error generated inside a recently deoptimized callback function
// includes some in it's stack trace.
(() => {
  const re = /Array\.some/;
  function lazyDeopt(deopt) {
    const b = [1, 2, 3];
    let did_assert_error = false;
    let result = 0;
    b.some((v, i) => {
      result += v;
      if (i === 1) {
        %DeoptimizeNow();
      } else if (i === 2) {
        const e = new Error();
        assertTrue(re.exec(e.stack) !== null);
        did_assert_error = true;
      }
      return false;
    });
    return did_assert_error;
  }
  %PrepareFunctionForOptimization(lazyDeopt);
  lazyDeopt();
  lazyDeopt();
  %OptimizeFunctionOnNextCall(lazyDeopt);
  assertTrue(lazyDeopt());
})();

// Verify that various exception edges are handled appropriately.
// The thrown Error object should always indicate it was created from
// a some call stack.
(() => {
  const re = /Array\.some/;
  const a = [1, 2, 3];
  let result = 0;
  function lazyDeopt() {
    a.some((v, i) => {
      result += i;
      if (i === 1) {
        %DeoptimizeFunction(lazyDeopt);
        throw new Error();
      }
      return false;
    });
  }
  %PrepareFunctionForOptimization(lazyDeopt);
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

// Messing with the Array prototype causes deoptimization.
(() => {
  const a = [1, 2, 3];
  let result = 0;
  function prototypeChanged() {
    a.some((v, i) => {
      result += v;
      return false;
    });
  }
  %PrepareFunctionForOptimization(prototypeChanged);
  prototypeChanged();
  prototypeChanged();
  %OptimizeFunctionOnNextCall(prototypeChanged);
  prototypeChanged();
  a.constructor = {};
  prototypeChanged();
  assertUnoptimized(prototypeChanged);
  assertEquals(24, result);
})();

// Verify holes are skipped.
(() => {
  const a = [1, 2, , 3, 4];
  function withHoles() {
    const callback_values = [];
    a.some(v => {
      callback_values.push(v);
      return false;
    });
    return callback_values;
  }
  %PrepareFunctionForOptimization(withHoles);
  withHoles();
  withHoles();
  %OptimizeFunctionOnNextCall(withHoles);
  assertArrayEquals([1, 2, 3, 4], withHoles());
})();

(() => {
  const a = [1.5, 2.5, , 3.5, 4.5];
  function withHoles() {
    const callback_values = [];
    a.some(v => {
      callback_values.push(v);
      return false;
    });
    return callback_values;
  }
  %PrepareFunctionForOptimization(withHoles);
  withHoles();
  withHoles();
  %OptimizeFunctionOnNextCall(withHoles);
  assertArrayEquals([1.5, 2.5, 3.5, 4.5], withHoles());
})();

// Handle callback is not callable.
(() => {
  const a = [1, 2, 3, 4, 5];
  function notCallable() {
    return a.some(undefined);
  }
  %PrepareFunctionForOptimization(notCallable);

  assertThrows(notCallable, TypeError);
  try { notCallable(); } catch(e) { }
  %OptimizeFunctionOnNextCall(notCallable);
  assertThrows(notCallable, TypeError);
})();
