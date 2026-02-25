// Flags: --experimental-ffi
'use strict';
const { skipIfFFIMissing } = require('../common');
skipIfFFIMissing();
const ffi = require('node:ffi');
const { suite, test } = require('node:test');

// Load standard C library based on platform
let libPath;
if (process.platform === 'win32') {
  libPath = 'msvcrt.dll';
} else if (process.platform === 'darwin') {
  libPath = 'libc.dylib';
} else {
  libPath = 'libc.so.6';
}

suite('dlopen', () => {
  test('can open a library', (t) => {
    const lib = ffi.dlopen(libPath);
    t.assert.ok(lib);
  });

  test('can open a library with empty symbols', (t) => {
    const lib = ffi.dlopen(libPath, {});
    t.assert.ok(lib);
    t.assert.ok(lib.symbols);
  });

  test('throws when library cannot be found', (t) => {
    t.assert.throws(() => {
      ffi.dlopen('nonexistent_library_that_does_not_exist.so');
    }, Error);
  });

  test('can get a symbol from a loaded library', (t) => {
    const lib = ffi.dlopen(libPath, {
      strlen: { parameters: ['pointer'], result: 'u64' },
    });

    t.assert.ok(lib.symbols);
    t.assert.ok(lib.symbols.strlen);
  });

  test('throws when symbol is not found', (t) => {
    t.assert.throws(() => {
      ffi.dlopen(libPath, {
        nonexistent_function_symbol_zzz: { parameters: [], result: 'void' },
      });
    }, Error);
  });

  test('can close a library', (t) => {
    const lib = ffi.dlopen(libPath);
    lib.close();
  });

  test('can manually get symbol and create function', (t) => {
    const lib = ffi.dlopen(libPath);

    // Get symbol address manually
    const strlenSymbol = lib.getSymbol('strlen');
    t.assert.ok(strlenSymbol);

    // getSymbol should return a PointerObject (null prototype)
    t.assert.strictEqual(Object.getPrototypeOf(strlenSymbol), null);

    // PointerObject should not have .address property
    t.assert.strictEqual(strlenSymbol.address, undefined);

    // Can extract address using UnsafePointer.value()
    const address = ffi.UnsafePointer.value(strlenSymbol);
    t.assert.ok(typeof address === 'bigint');

    // Create UnsafeFnPointer manually using the pointer and definition
    const strlen = new ffi.UnsafeFnPointer(
      strlenSymbol,
      {
        parameters: ['pointer'],
        result: 'u64',
      },
    );

    // Invoke the function
    const str = new TextEncoder().encode('test\0');
    const len = strlen(str);
    t.assert.strictEqual(len, 4n);
  });
});
