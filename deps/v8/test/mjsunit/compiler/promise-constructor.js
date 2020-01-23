// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// We have to patch mjsunit because normal assertion failures just throw
// exceptions which are swallowed in a then clause.
failWithMessage = (msg) => %AbortJS(msg);

// Don't crash.
(function() {
  function foo() {
    let resolve, reject, promise;
    promise = new Promise((a, b) => { resolve = a; reject = b; });

    return {resolve, reject, promise};
  }

  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();

// Check that when executor is non-callable, the constructor throws.
(function() {
  function foo() {
    return new Promise(1);
  }

  %PrepareFunctionForOptimization(foo);
  assertThrows(foo, TypeError);
  assertThrows(foo, TypeError);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(foo, TypeError);
})();

// Check that when the promise constructor throws because the executor is
// non-callable, the stack contains 'new Promise'.
(function() {
  function foo() {
    return new Promise(1);
  }

  %PrepareFunctionForOptimization(foo);
  let threw;
  try {
    threw = false;
    foo();
  } catch (e) {
    threw = true;
    assertContains('new Promise', e.stack);
  } finally {
    assertTrue(threw);
  }
  try {
    threw = false;
    foo();
  } catch (e) {
    threw = true;
    assertContains('new Promise', e.stack);
  } finally {
    assertTrue(threw);
  }

  %OptimizeFunctionOnNextCall(foo);
  try {
    threw = false;
    foo();
  } catch (e) {
    threw = true;
    assertContains('new Promise', e.stack);
  } finally {
    assertTrue(threw);
  }
})();

// Check that when executor throws, the promise is rejected.
(function() {
  function foo() {
    return new Promise((a, b) => { throw new Error(); });
  }
  %PrepareFunctionForOptimization(foo);

  function bar(i) {
    let error = null;
    foo().then(_ => error = 1, e => error = e);
    setTimeout(_ => assertInstanceof(error, Error));
    if (i == 1) %OptimizeFunctionOnNextCall(foo);
    if (i > 0) setTimeout(bar.bind(null, i - 1));
    }
  bar(3);
})();

// Check that when executor causes lazy deoptimization of the inlined
// constructor, we return the promise value and not the return value of the
// executor function itself.
(function() {
  function foo() {
    let p;
    try {
      p = new Promise((a, b) => { %DeoptimizeFunction(foo); });
    } catch (e) {
      // Nothing should throw
      assertUnreachable();
    }
    assertInstanceof(p, Promise);
  }

  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();

// The same as above, except that the executor function also creates a promise
// and both executor functions cause a lazy deopt of the calling function.
(function() {
  function executor(a, b) {
    %DeoptimizeFunction(foo);
    let p = new Promise((a, b) => { %DeoptimizeFunction(executor); });
  }
  function foo() {
    let p;
    try {
      p = new Promise(executor);
    } catch (e) {
      // Nothing should throw
      assertUnreachable();
    }
    assertInstanceof(p, Promise);
  }

  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();

// Check that when the executor causes lazy deoptimization of the inlined
// constructor, and then throws, the deopt continuation catches and then calls
// the reject function instead of propagating the exception.
(function() {
  function foo() {
    let p;
    try {
      p = new Promise((a, b) => {
        %DeoptimizeFunction(foo);
        throw new Error();
      });
    } catch (e) {
      // The promise constructor should catch the exception and reject the
      // promise instead.
      assertUnreachable();
    }
    assertInstanceof(p, Promise);
  }

  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();


// Check that when the promise constructor is marked for lazy deoptimization
// from below, but not immediatelly deoptimized, and then throws, the deopt continuation
// catches and calls the reject function instead of propagating the exception.
(function() {
  function foo() {
    let p;
    try {
      p = new Promise((resolve, reject) => { bar(); resolve()});
    } catch (e) {
       // The promise constructor should catch the exception and reject the
      // promise instead.
      assertUnreachable();
    }
    assertInstanceof(p, Promise);
  }

  function bar() {
    %DeoptimizeFunction(foo);
    throw new Error();
  }
  %NeverOptimizeFunction(bar);

  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();

// Test when the executor is not inlined.
(function() {
  let resolve, reject, promise;
  function bar(a, b) {
    resolve = a; reject = b;
    throw new Error();
  }
  function foo() {
    promise = new Promise(bar);
  }
  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %NeverOptimizeFunction(bar);
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();

// Test that the stack trace contains 'new Promise'
(function() {
  let resolve, reject, promise;
  function bar(a, b) {
    resolve = a; reject = b;
    let stack = new Error().stack;
    assertContains("new Promise", stack);
    throw new Error();
  }
  function foo() {
    promise = new Promise(bar);
  }
  %PrepareFunctionForOptimization(foo);
  foo();
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();
