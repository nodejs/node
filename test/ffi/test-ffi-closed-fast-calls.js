// Flags: --experimental-ffi --allow-natives-syntax
'use strict';

// Regression test for calls that reach a generated fast-call trampoline after
// the owning library has been closed.
//
// `DynamicLibrary::Close()` sets `FFIFunction::closed` and clears
// `FFIFunction::ptr`, and both C++ invokers check them. Neither check can run on
// the fast-call path: the trampoline is created with the resolved symbol address
// and V8 embeds the trampoline address in optimized code, so a call site that
// has tiered up jumps straight into the unloaded library.

const common = require('../common');

common.skipIfFFIMissing();

const assert = require('node:assert');
const { test } = require('node:test');
const ffi = require('node:ffi');
const { cString, fixtureSymbols, libraryPath } = require('./ffi-test-common');

// Every function that has a trampoline is exposed as a JavaScript wrapper, and
// that wrapper holds the fast-call site, so it is the function that has to be
// optimized to put the trampoline in play. Optimizing only an outer caller does
// not reach it, and neither does calling after `close()`: a call site that
// always throws never tiers up, which is why a closed library used to survive a
// single call and then crash under sustained use.
function optimizeFastCall(fn, args) {
  assert.doesNotMatch(fn.toString(), /\[native code\]/,
                      'FFI functions with a fast-call trampoline must be ' +
                      'exposed as JavaScript wrappers');
  eval('%PrepareFunctionForOptimization(fn)');
  fn(...args);
  fn(...args);
  eval('%OptimizeFunctionOnNextCall(fn)');
  fn(...args);
}

// The guard must hold for repeated calls, not just the first one.
function assertAlwaysClosed(fn, args) {
  assert.throws(() => fn(...args), { code: 'ERR_FFI_LIBRARY_CLOSED' });

  for (let i = 0; i < 1000; i++) {
    let code;
    try {
      fn(...args);
    } catch (err) {
      code = err.code;
    }
    assert.strictEqual(code, 'ERR_FFI_LIBRARY_CLOSED');
  }
}

test('closed library throws from optimized scalar fast calls', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, fixtureSymbols);
  const multiply = functions.multiply_f64;

  optimizeFastCall(multiply, [2, 3]);
  assert.strictEqual(multiply(2, 3), 6);

  lib.close();
  assertAlwaysClosed(multiply, [2, 3]);
});

test('closed library throws from optimized integer fast calls', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, fixtureSymbols);
  const add = functions.add_i32;

  optimizeFastCall(add, [20, 22]);
  assert.strictEqual(add(20, 22), 42);

  lib.close();
  assertAlwaysClosed(add, [20, 22]);
});

test('closed library throws from optimized pointer fast calls', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, fixtureSymbols);
  const strlen = functions.safe_strlen;
  const buffer = cString('closed');

  optimizeFastCall(strlen, [buffer]);
  assert.strictEqual(strlen(buffer), 6);

  lib.close();
  assertAlwaysClosed(strlen, [buffer]);
});

test('closed library throws from optimized multi-argument fast calls', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, fixtureSymbols);
  const sum = functions.sum_five_f64;

  optimizeFastCall(sum, [1, 2, 3, 4, 5]);
  assert.strictEqual(sum(1, 2, 3, 4, 5), 15);

  lib.close();
  assertAlwaysClosed(sum, [1, 2, 3, 4, 5]);
});

test('closed library throws for functions from getFunction()', () => {
  const lib = new ffi.DynamicLibrary(libraryPath);
  const multiply = lib.getFunction('multiply_f64', fixtureSymbols.multiply_f64);

  optimizeFastCall(multiply, [2, 3]);
  assert.strictEqual(multiply(2, 3), 6);

  lib.close();
  assertAlwaysClosed(multiply, [2, 3]);
});

test('disposing a library invalidates retained fast wrappers', () => {
  let multiply;
  {
    using handle = ffi.dlopen(libraryPath, fixtureSymbols);
    multiply = handle.functions.multiply_f64;
    optimizeFastCall(multiply, [2, 3]);
  }

  assertAlwaysClosed(multiply, [2, 3]);
});
