'use strict';

const assert = require('node:assert');
const common = require('../common.js');
const { libraryPath, ensureFixtureLibrary } = require('./common.js');

// Measure the invocation (call) path for signatures that bypass V8 Fast API
// and use libffi through FFIFunction::Invoke(). On x86-64 System V with
// libffi >= 3.7, Invoke() reuses a precomputed call plan that avoids repeating
// argument-placement work on every call. This benchmark quantifies the
// per-call benefit.
//
// Signatures chosen to bypass both V8 Fast API and keep native work minimal:
// - call_int_callback (null): 'function' type forces the generic path; null
//   pointer triggers the early return in C so native computation is negligible.
//   From libffi's perspective this is a register-only plan (2 pointer-sized
//   args both fit in GP registers on x86-64 System V).
// - sum_8_i32: 8 GP args exceed the x86-64 Fast API register cap (6), forcing
//   the generic path. From libffi's perspective 6 args go in registers and 2
//   spill to the stack, exercising a stack-spilled plan.

const bench = common.createBenchmark(main, {
  n: [1e7],
  symbol: ['call_int_callback', 'sum_8_i32'],
}, {
  flags: ['--no-warnings'],
});

ensureFixtureLibrary();

function main({ n, symbol }) {
  const ffi = require('node:ffi');

  if (symbol === 'call_int_callback') {
    // 'function' type bypasses Fast API (IsFastCallEligible rejects it).
    // Pass 0n (null function pointer) so the native function returns -1
    // immediately without invoking any callback, keeping per-call overhead
    // dominated by the FFI call machinery itself.
    const { lib, functions } = ffi.dlopen(libraryPath, {
      call_int_callback: { return: 'i32', arguments: ['function', 'i32'] },
    });

    try {
      // Verify the null-pointer early return.
      assert.strictEqual(functions.call_int_callback(0n, 7), -1);

      bench.start();
      for (let i = 0; i < n; ++i)
        functions.call_int_callback(0n, 21);
      bench.end(n);
    } finally {
      lib.close();
    }
  } else {
    // 8 integer args exceed the x86-64 SysV GP register cap (6), which makes
    // CreateFastFFIMetadata reject the signature. Calls go through the
    // SharedBuffer or generic invoker into FFIFunction::Invoke().
    const { lib, functions } = ffi.dlopen(libraryPath, {
      sum_8_i32: {
        return: 'i32',
        arguments: [
          'i32', 'i32', 'i32', 'i32',
          'i32', 'i32', 'i32', 'i32',
        ],
      },
    });

    const fn = functions.sum_8_i32;

    assert.strictEqual(fn(1, 2, 3, 4, 5, 6, 7, 8), 36);

    bench.start();
    for (let i = 0; i < n; ++i)
      fn(1, 2, 3, 4, 5, 6, 7, 14);
    bench.end(n);

    lib.close();
  }
}
