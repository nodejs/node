// Flags: --experimental-ffi
'use strict';
const common = require('../common');
common.skipIfFFIMissing();
const assert = require('node:assert');
const { test } = require('node:test');
const ffi = require('node:ffi');
const { fixtureSymbols, libraryPath } = require('./ffi-test-common');

test('dlopen without definitions returns empty function map', () => {
  const { lib, functions } = ffi.dlopen(libraryPath);

  try {
    assert.ok(lib instanceof ffi.DynamicLibrary);
    assert.strictEqual(lib.path, libraryPath);
    assert.strictEqual(typeof functions, 'object');
    assert.strictEqual(Object.keys(functions).length, 0);
    assert.strictEqual(Object.getPrototypeOf(functions), null);
  } finally {
    lib.close();
  }
});

test('dlopen resolves symbols from the current process with null path', {
  skip: common.isWindows,
}, () => {
  const { lib, functions } = ffi.dlopen(null, {
    uv_os_getpid: { result: 'i32', parameters: [] },
  });

  try {
    assert.ok(lib instanceof ffi.DynamicLibrary);
    assert.strictEqual(functions.uv_os_getpid(), process.pid);
  } finally {
    lib.close();
  }
});

test('dlopen resolves functions from definitions', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i32: fixtureSymbols.add_i32,
    add_f32: { returns: 'f32', arguments: ['f32', 'f32'] },
    add_u64: { return: 'u64', parameters: ['u64', 'u64'] },
  });

  try {
    assert.ok(lib instanceof ffi.DynamicLibrary);
    assert.strictEqual(lib.path, libraryPath);
    assert.strictEqual(functions.add_i32(10, 32), 42);
    assert.strictEqual(functions.add_f32(1.25, 2.75), 4);
    assert.strictEqual(functions.add_u64(20n, 22n), 42n);
    assert.strictEqual(functions.add_i32.name, 'add_i32');
    // Shared-buffer wrapper sets `length` to the FFI signature's arity
    // (see `inheritMetadata` in lib/internal/ffi-shared-buffer.js). The raw
    // native function has length 0, but the wrapper exposes the parameter
    // count so `fn.length` is useful for introspection.
    assert.strictEqual(functions.add_i32.length, 2);
    assert.strictEqual(typeof functions.add_i32.pointer, 'bigint');
    assert.strictEqual(Object.getPrototypeOf(functions), null);
  } finally {
    lib.close();
  }
});

test('DynamicLibrary exposes functions and symbols', () => {
  const lib = new ffi.DynamicLibrary(libraryPath);

  try {
    const addI32 = lib.getFunction('add_i32', fixtureSymbols.add_i32);
    const addU64 = lib.getFunction('add_u64', {
      returns: 'u64',
      arguments: ['u64', 'u64'],
    });
    const addI32Ptr = lib.getSymbol('add_i32');

    assert.strictEqual(addI32(20, 22), 42);
    assert.strictEqual(addU64(2n, 40n), 42n);
    assert.strictEqual(ffi.dlsym(lib, 'add_i32'), addI32Ptr);
    assert.strictEqual(addI32.pointer, addI32Ptr);

    const functions = lib.getFunctions({
      add_f32: { result: 'f32', parameters: ['f32', 'f32'] },
      add_i64: { return: 'i64', arguments: ['i64', 'i64'] },
    });

    assert.strictEqual(functions.add_f32(10, 32), 42);
    assert.strictEqual(functions.add_i64(21n, 21n), 42n);

    const symbols = lib.getSymbols();
    assert.strictEqual(Object.getPrototypeOf(functions), null);
    assert.strictEqual(Object.getPrototypeOf(symbols), null);
    assert.strictEqual(symbols.add_i32, addI32Ptr);
    assert.strictEqual(symbols.add_u64, addU64.pointer);
    assert.strictEqual(lib.symbols.add_i32, addI32Ptr);
    assert.strictEqual(lib.functions.add_i64.pointer, functions.add_i64.pointer);
  } finally {
    ffi.dlclose(lib);
  }
});

test('getFunction caches signatures consistently', () => {
  const lib = new ffi.DynamicLibrary(libraryPath);

  try {
    const addI32 = lib.getFunction('add_i32', fixtureSymbols.add_i32);

    assert.strictEqual(
      lib.getFunction('add_i32', fixtureSymbols.add_i32).pointer,
      addI32.pointer,
    );

    assert.throws(() => {
      lib.getFunction('add_i32', { parameters: ['u32', 'u32'], result: 'u32' });
    }, /already requested with a different signature/);
  } finally {
    lib.close();
  }
});

test('closed libraries reject subsequent operations', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i32: fixtureSymbols.add_i32,
  });

  lib.close();

  assert.throws(() => functions.add_i32(1, 2), /Library is closed/);
  assert.throws(() => lib.getFunction('add_i32', fixtureSymbols.add_i32), /Library is closed/);
  assert.throws(() => lib.getSymbol('add_i32'), /Library is closed/);
});

test('dynamic library APIs validate failures and bad signatures', () => {
  assert.throws(() => {
    ffi.dlopen('missing-library-for-ffi-tests.so');
  }, /dlopen failed:/);

  assert.throws(() => {
    ffi.dlopen(`${libraryPath}\0suffix`);
  }, /Library path must not contain null bytes/);

  assert.throws(() => {
    ffi.dlopen(libraryPath, {
      add_i32: fixtureSymbols.add_i32,
      missing_symbol: { result: 'void', parameters: [] },
    });
  }, /dlsym failed:/);

  {
    const { lib, functions } = ffi.dlopen(libraryPath, {
      add_i32: fixtureSymbols.add_i32,
    });
    try {
      assert.strictEqual(functions.add_i32(20, 22), 42);
    } finally {
      lib.close();
    }
  }

  const lib = new ffi.DynamicLibrary(libraryPath);

  try {
    assert.throws(() => {
      lib.getFunction('missing_symbol', { result: 'void', parameters: [] });
    }, /dlsym failed:/);

    assert.throws(() => {
      lib.getFunction('add_i32\0suffix', fixtureSymbols.add_i32);
    }, /Function name must not contain null bytes/);

    assert.throws(() => {
      lib.getSymbol('add_i32\0suffix');
    }, /Symbol name must not contain null bytes/);

    assert.throws(() => {
      lib.getFunctions({
        add_i32: fixtureSymbols.add_i32,
        missing_symbol: { result: 'void', parameters: [] },
      });
    }, /dlsym failed:/);

    assert.strictEqual(lib.getFunction('add_i32', {
      result: 'pointer',
      parameters: ['pointer'],
    }).pointer, lib.getSymbol('add_i32'));

    assert.throws(() => {
      lib.getFunction('add_i32', { result: 'i32\0bad', parameters: [] });
    }, /Return value type of function add_i32 must not contain null bytes/);

    assert.throws(() => {
      lib.getFunction('add_i32', { result: 'i32', parameters: ['i32\0bad'] });
    }, /Argument 0 of function add_i32 must not contain null bytes/);

    assert.throws(() => {
      lib.getFunctions('not an object');
    }, {
      name: 'TypeError',
      message: 'Functions signatures must be an object',
    });

    assert.throws(() => {
      lib.getFunctions([]);
    }, {
      name: 'TypeError',
      message: 'Functions signatures must be an object',
    });

    assert.throws(() => {
      lib.getFunctions([fixtureSymbols.add_i32]);
    }, {
      name: 'TypeError',
      message: 'Functions signatures must be an object',
    });

    assert.throws(() => {
      lib.getFunctions({ add_i32: 1 });
    }, {
      name: 'TypeError',
      message: 'Signature of function add_i32 must be an object',
    });

    assert.throws(() => {
      lib.getFunction('add_i32', {
        result: 'i32',
        return: 'i32',
        parameters: ['i32', 'i32'],
      });
    }, /must have either 'returns', 'return' or 'result' property/);

    assert.throws(() => {
      lib.getFunction('add_i32', {
        result: 'i32',
        parameters: ['i32', 'i32'],
        arguments: ['i32', 'i32'],
      });
    }, /must have either 'parameters' or 'arguments' property/);

    assert.throws(() => {
      lib.getFunction('add_i32', { result: 'bogus', parameters: [] });
    }, /Unsupported FFI type: bogus/);

    const hasTrapError = new Error('signature has trap');
    assert.throws(() => {
      lib.getFunction('add_i32', new Proxy({}, {
        has(target, key) {
          if (key === 'result') {
            throw hasTrapError;
          }
          return Reflect.has(target, key);
        },
      }));
    }, hasTrapError);

    const getterError = new Error('signature getter');
    assert.throws(() => {
      lib.getFunction('add_i32', {
        result: 'i32',
        get parameters() {
          throw getterError;
        },
      });
    }, getterError);
  } finally {
    lib.close();
  }
});
