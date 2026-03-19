// Flags: --experimental-ffi
'use strict';
const common = require('../common');
common.skipIfFFIMissing();

const test = require('node:test');
const ffi = require('node:ffi');
const { fixtureSymbols, libraryPath } = require('./ffi-test-common');

test('dlopen() returns DynamicLibrary with empty functions by default', (t) => {
  const { lib, functions } = ffi.dlopen(libraryPath);
  t.after(() => lib.close());

  t.assert.ok(lib instanceof ffi.DynamicLibrary);
  t.assert.strictEqual(lib.path, libraryPath);
  t.assert.strictEqual(typeof functions, 'object');
  t.assert.strictEqual(Object.keys(functions).length, 0);
});

test('dlopen() resolves requested signatures and metadata', (t) => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i32: fixtureSymbols.add_i32,
    add_f32: { returns: 'f32', arguments: ['f32', 'f32'] },
    add_u64: { return: 'u64', parameters: ['u64', 'u64'] },
  });
  t.after(() => lib.close());

  t.assert.ok(lib instanceof ffi.DynamicLibrary);
  t.assert.strictEqual(lib.path, libraryPath);
  t.assert.strictEqual(functions.add_i32(10, 32), 42);
  t.assert.strictEqual(functions.add_f32(1.25, 2.75), 4);
  t.assert.strictEqual(functions.add_u64(20n, 22n), 42n);
  t.assert.strictEqual(functions.add_i32.name, 'add_i32');
  t.assert.strictEqual(functions.add_i32.length, 0);
  t.assert.strictEqual(typeof functions.add_i32.pointer, 'bigint');
});

test('dlopen() throws on a missing library', (t) => {
  t.assert.throws(() => {
    ffi.dlopen('missing-library-for-ffi-tests.so');
  }, /dlopen failed:/);
});

test('DynamicLibrary() constructor throws without new', (t) => {
  t.assert.throws(() => {
    ffi.DynamicLibrary(libraryPath);
  }, /Class constructor DynamicLibrary cannot be invoked without 'new'/);
});

test('DynamicLibrary APIs resolve functions and symbols', (t) => {
  const lib = new ffi.DynamicLibrary(libraryPath);
  t.after(() => ffi.dlclose(lib));
  const addI32 = lib.getFunction('add_i32', fixtureSymbols.add_i32);
  const addU64 = lib.getFunction('add_u64', {
    returns: 'u64',
    arguments: ['u64', 'u64'],
  });
  const addI32Ptr = lib.getSymbol('add_i32');

  t.assert.strictEqual(addI32(20, 22), 42);
  t.assert.strictEqual(addU64(2n, 40n), 42n);
  t.assert.strictEqual(ffi.dlsym(lib, 'add_i32'), addI32Ptr);
  t.assert.strictEqual(addI32.pointer, addI32Ptr);

  const functions = lib.getFunctions({
    add_f32: { result: 'f32', parameters: ['f32', 'f32'] },
    add_i64: { return: 'i64', arguments: ['i64', 'i64'] },
  });

  t.assert.strictEqual(functions.add_f32(10, 32), 42);
  t.assert.strictEqual(functions.add_i64(21n, 21n), 42n);

  const symbols = lib.getSymbols();
  t.assert.strictEqual(symbols.add_i32, addI32Ptr);
  t.assert.strictEqual(symbols.add_u64, addU64.pointer);
  t.assert.strictEqual(lib.symbols.add_i32, addI32Ptr);
  t.assert.strictEqual(lib.functions.add_i64.pointer, functions.add_i64.pointer);
});

test('getFunction() caches signatures and rejects mismatches', (t) => {
  const lib = new ffi.DynamicLibrary(libraryPath);
  t.after(() => lib.close());
  const addI32 = lib.getFunction('add_i32', fixtureSymbols.add_i32);

  t.assert.strictEqual(
    lib.getFunction('add_i32', fixtureSymbols.add_i32).pointer,
    addI32.pointer,
  );

  t.assert.throws(() => {
    lib.getFunction('add_i32', { parameters: ['u32', 'u32'], result: 'u32' });
  }, /already requested with a different signature/);
});

test('library methods and function wrappers throw after close', (t) => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i32: fixtureSymbols.add_i32,
  });

  lib.close();

  t.assert.throws(() => functions.add_i32(1, 2), /Library is closed/);
  t.assert.throws(() => lib.getFunction('add_i32', fixtureSymbols.add_i32), /Library is closed/);
  t.assert.throws(() => lib.getSymbol('add_i32'), /Library is closed/);
});

test('getFunction() throws on missing symbols', (t) => {
  const lib = new ffi.DynamicLibrary(libraryPath);
  t.after(() => lib.close());

  t.assert.throws(() => {
    lib.getFunction('missing_symbol', { parameters: [], result: 'void' });
  }, /dlsym failed:/);
});

test('errors are thrown for invalid ffi function signatures', (t) => {
  const lib = new ffi.DynamicLibrary(libraryPath);
  t.after(() => lib.close());

  t.assert.throws(() => {
    lib.getFunctions('not an object');
  }, {
    name: 'TypeError',
    message: 'Functions signatures must be an object',
  });

  t.assert.throws(() => {
    lib.getFunctions([]);
  }, {
    name: 'TypeError',
    message: 'Functions signatures must be an object',
  });

  t.assert.throws(() => {
    lib.getFunctions([fixtureSymbols.add_i32]);
  }, {
    name: 'TypeError',
    message: 'Functions signatures must be an object',
  });

  t.assert.throws(() => {
    lib.getFunctions({ add_i32: 1 });
  }, {
    name: 'TypeError',
    message: 'Signature of function add_i32 must be an object',
  });

  t.assert.throws(() => {
    lib.getFunction('add_i32', {
      result: 'i32',
      return: 'i32',
      parameters: ['i32', 'i32'],
    });
  }, /must have either 'returns', 'return' or 'result' property/);

  t.assert.throws(() => {
    lib.getFunction('add_i32', {
      result: 'i32',
      parameters: ['i32', 'i32'],
      arguments: ['i32', 'i32'],
    });
  }, /must have either 'parameters' or 'arguments' property/);

  t.assert.throws(() => {
    lib.getFunction('add_i32', { result: 'bogus', parameters: [] });
  }, /Unsupported FFI type: bogus/);

  const hasTrapError = new Error('signature has trap');
  t.assert.throws(() => {
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
  t.assert.throws(() => {
    lib.getFunction('add_i32', {
      result: 'i32',
      get parameters() {
        throw getterError;
      },
    });
  }, getterError);
});
