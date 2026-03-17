// Flags: --experimental-ffi
'use strict';
const common = require('../common');
common.skipIfFFIMissing();
const {
  ok,
  strictEqual,
  throws,
} = require('node:assert');
const ffi = require('node:ffi');
const { fixtureSymbols, libraryPath } = require('./ffi-test-common');

{
  const { lib, functions } = ffi.dlopen(libraryPath);

  ok(lib instanceof ffi.DynamicLibrary);
  strictEqual(lib.path, libraryPath);
  strictEqual(typeof functions, 'object');
  strictEqual(Object.keys(functions).length, 0);

  lib.close();
}

{
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i32: fixtureSymbols.add_i32,
    add_f32: { returns: 'f32', arguments: ['f32', 'f32'] },
    add_u64: { return: 'u64', parameters: ['u64', 'u64'] },
  });

  ok(lib instanceof ffi.DynamicLibrary);
  strictEqual(lib.path, libraryPath);
  strictEqual(functions.add_i32(10, 32), 42);
  strictEqual(functions.add_f32(1.25, 2.75), 4);
  strictEqual(functions.add_u64(20n, 22n), 42n);
  strictEqual(functions.add_i32.name, 'add_i32');
  strictEqual(functions.add_i32.length, 0);
  strictEqual(typeof functions.add_i32.pointer, 'bigint');

  lib.close();
}

{
  const lib = new ffi.DynamicLibrary(libraryPath);
  const addI32 = lib.getFunction('add_i32', fixtureSymbols.add_i32);
  const addU64 = lib.getFunction('add_u64', {
    returns: 'u64',
    arguments: ['u64', 'u64'],
  });
  const addI32Ptr = lib.getSymbol('add_i32');

  strictEqual(addI32(20, 22), 42);
  strictEqual(addU64(2n, 40n), 42n);
  strictEqual(ffi.dlsym(lib, 'add_i32'), addI32Ptr);
  strictEqual(addI32.pointer, addI32Ptr);

  const functions = lib.getFunctions({
    add_f32: { result: 'f32', parameters: ['f32', 'f32'] },
    add_i64: { return: 'i64', arguments: ['i64', 'i64'] },
  });

  strictEqual(functions.add_f32(10, 32), 42);
  strictEqual(functions.add_i64(21n, 21n), 42n);

  const symbols = lib.getSymbols();
  strictEqual(symbols.add_i32, addI32Ptr);
  strictEqual(symbols.add_u64, addU64.pointer);
  strictEqual(lib.symbols.add_i32, addI32Ptr);
  strictEqual(lib.functions.add_i64.pointer, functions.add_i64.pointer);

  ffi.dlclose(lib);
}

{
  const lib = new ffi.DynamicLibrary(libraryPath);
  const addI32 = lib.getFunction('add_i32', fixtureSymbols.add_i32);

  strictEqual(
    lib.getFunction('add_i32', fixtureSymbols.add_i32).pointer,
    addI32.pointer,
  );

  throws(() => {
    lib.getFunction('add_i32', { parameters: ['u32', 'u32'], result: 'u32' });
  }, /already requested with a different signature/);

  lib.close();
}

{
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i32: fixtureSymbols.add_i32,
  });

  lib.close();

  throws(() => functions.add_i32(1, 2), /Library is closed/);
  throws(() => lib.getFunction('add_i32', fixtureSymbols.add_i32), /Library is closed/);
  throws(() => lib.getSymbol('add_i32'), /Library is closed/);
}

{
  throws(() => {
    ffi.dlopen('missing-library-for-ffi-tests.so');
  }, /dlopen failed:/);

  const lib = new ffi.DynamicLibrary(libraryPath);

  throws(() => {
    lib.getFunction('missing_symbol', { result: 'void', parameters: [] });
  }, /dlsym failed:/);

  throws(() => {
    lib.getFunctions('not an object');
  }, {
    name: 'TypeError',
    message: 'Functions signatures must be an object',
  });

  throws(() => {
    lib.getFunctions([]);
  }, {
    name: 'TypeError',
    message: 'Functions signatures must be an object',
  });

  throws(() => {
    lib.getFunctions([fixtureSymbols.add_i32]);
  }, {
    name: 'TypeError',
    message: 'Functions signatures must be an object',
  });

  throws(() => {
    lib.getFunctions({ add_i32: 1 });
  }, {
    name: 'TypeError',
    message: 'Signature of function add_i32 must be an object',
  });

  throws(() => {
    lib.getFunction('add_i32', {
      result: 'i32',
      return: 'i32',
      parameters: ['i32', 'i32'],
    });
  }, /must have either 'returns', 'return' or 'result' property/);

  throws(() => {
    lib.getFunction('add_i32', {
      result: 'i32',
      parameters: ['i32', 'i32'],
      arguments: ['i32', 'i32'],
    });
  }, /must have either 'parameters' or 'arguments' property/);

  throws(() => {
    lib.getFunction('add_i32', { result: 'bogus', parameters: [] });
  }, /Unsupported FFI type: bogus/);

  const hasTrapError = new Error('signature has trap');
  throws(() => {
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
  throws(() => {
    lib.getFunction('add_i32', {
      result: 'i32',
      get parameters() {
        throw getterError;
      },
    });
  }, getterError);

  lib.close();
}
