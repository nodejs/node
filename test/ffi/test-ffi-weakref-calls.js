// Flags: --expose-gc
'use strict';
const { skipIfFFIMissing } = require('../common');
skipIfFFIMissing();

const { gcUntil } = require('../common/gc');
const test = require('node:test');
const ffi = require('node:ffi');
const { fixtureSymbols, libraryPath } = require('./ffi-test-common');

test('ffi function pointer callables retain their library through GC', async (t) => {
  let library = ffi.dlopen(libraryPath);
  const ref = new WeakRef(library.lib);
  const fn = library.lib.toFunction(library.lib.getSymbol('add_i32'), {
    arguments: ['i32', 'i32'], return: 'i32',
  });
  library = null;
  try {
    for (let index = 0; index < 5; index++) {
      await gcUntil('ffi function pointer retains library', () => true, 1);
      t.assert.notStrictEqual(ref.deref(), undefined);
      t.assert.strictEqual(fn(20, 22), 42);
    }
  } finally {
    ref.deref()?.close();
  }
  t.assert.throws(() => fn(20, 22), { code: 'ERR_FFI_LIBRARY_CLOSED' });
});

test('ffi function pointer registry does not retain callables', async (t) => {
  const { lib } = ffi.dlopen(libraryPath);
  try {
    const address = lib.getSymbol('add_i32');
    const refs = [];
    for (let index = 0; index < 100; index++) {
      let fn = lib.toFunction(address, {
        arguments: ['i32', 'i32'], return: 'i32',
      });
      refs.push(new WeakRef(fn));
      fn = null;
    }
    await gcUntil('ffi function pointer callables are collected',
                  () => refs.every((ref) => ref.deref() === undefined));
    const fn = lib.toFunction(address, {
      arguments: ['i32', 'i32'], return: 'i32',
    });
    t.assert.strictEqual(fn(20, 22), 42);
    lib.close();
    t.assert.throws(() => fn(20, 22), { code: 'ERR_FFI_LIBRARY_CLOSED' });
  } finally {
    lib.close();
  }
});

test('ffi pointer callable construction ignores throwing inherited setters', async (t) => {
  const { lib } = ffi.dlopen(libraryPath);
  t.after(() => lib.close());
  const address = lib.getSymbol('add_i32');
  const original = Object.getOwnPropertyDescriptor(Function.prototype, 'pointer');
  let fn;
  let setterCalls = 0;
  const failure = new Error('pointer setter failed');
  try {
    Object.defineProperty(Function.prototype, 'pointer', {
      configurable: true,
      get() { return undefined; },
      set() {
        setterCalls++;
        throw failure;
      },
    });
    fn = lib.toFunction(address, {
      arguments: ['i32', 'i32'], return: 'i32',
    });
    t.assert.strictEqual(setterCalls, 0);
    t.assert.strictEqual(Object.getOwnPropertyDescriptor(fn, 'pointer').value, address);
    t.assert.strictEqual(fn(20, 22), 42);
  } finally {
    if (original === undefined) {
      delete Function.prototype.pointer;
    } else {
      Object.defineProperty(Function.prototype, 'pointer', original);
    }
  }
  try {
    const ref = new WeakRef(fn);
    fn = null;
    await gcUntil('ffi pointer callable with inherited setter is collected',
                  () => ref.deref() === undefined);
  } finally {
    lib.close();
  }
});

test('ffi pointer callable construction ignores inherited setters that close the library', async (t) => {
  const { lib } = ffi.dlopen(libraryPath);
  t.after(() => lib.close());
  const address = lib.getSymbol('add_i32');
  const original = Object.getOwnPropertyDescriptor(Function.prototype, 'pointer');
  let fn;
  let setterCalls = 0;
  try {
    Object.defineProperty(Function.prototype, 'pointer', {
      configurable: true,
      get() { return undefined; },
      set() {
        setterCalls++;
        lib.close();
      },
    });
    fn = lib.toFunction(address, {
      arguments: ['i32', 'i32'], return: 'i32',
    });
    t.assert.strictEqual(setterCalls, 0);
    t.assert.strictEqual(Object.getOwnPropertyDescriptor(fn, 'pointer').value, address);
    t.assert.strictEqual(fn(20, 22), 42);
  } finally {
    if (original === undefined) {
      delete Function.prototype.pointer;
    } else {
      Object.defineProperty(Function.prototype, 'pointer', original);
    }
  }
  lib.close();
  t.assert.throws(() => fn(20, 22), { code: 'ERR_FFI_LIBRARY_CLOSED' });
  const ref = new WeakRef(fn);
  fn = null;
  await gcUntil('ffi pointer callable from closed library is collected',
                () => ref.deref() === undefined);
});

test('ffi unrefCallback releases callback function', async (t) => {
  const { lib, functions: symbols } = ffi.dlopen(libraryPath, fixtureSymbols);
  t.after(() => lib.close());

  let callback = () => 1;
  const ref = new WeakRef(callback);
  const pointer = lib.registerCallback(
    { arguments: ['i32'], return: 'i32' },
    callback,
  );

  lib.unrefCallback(pointer);
  callback = null;

  await gcUntil('ffi unrefCallback releases callback function', () => {
    return ref.deref() === undefined;
  });

  t.assert.strictEqual(symbols.call_int_callback(pointer, 21), 0);

  lib.unregisterCallback(pointer);
});

test('ffi unrefCallback zero-fills narrow callback return', async (t) => {
  const { lib, functions: symbols } = ffi.dlopen(libraryPath, fixtureSymbols);
  t.after(() => lib.close());

  let callback = () => 1;
  const ref = new WeakRef(callback);
  const pointer = lib.registerCallback(
    { arguments: ['i8'], return: 'i8' },
    callback,
  );

  lib.unrefCallback(pointer);
  callback = null;

  await gcUntil('ffi unrefCallback zero-fills narrow callback return', () => {
    return ref.deref() === undefined;
  });

  t.assert.strictEqual(symbols.call_int8_callback(pointer, 21), 0);
  lib.unregisterCallback(pointer);
});

test('ffi refCallback retains callback function', async (t) => {
  const { lib } = ffi.dlopen(libraryPath, fixtureSymbols);
  t.after(() => lib.close());

  let callback = () => 1;
  const ref = new WeakRef(callback);
  const pointer = lib.registerCallback({ return: 'i32' }, callback);

  lib.unrefCallback(pointer);
  lib.refCallback(pointer);
  callback = null;

  for (let i = 0; i < 5; i++) {
    await gcUntil('ffi refCallback retains callback function', () => true, 1);
    t.assert.strictEqual(typeof ref.deref(), 'function');
  }

  lib.unregisterCallback(pointer);
});

test('callback ref/unref throw after callback function is collected', async (t) => {
  const { lib } = ffi.dlopen(libraryPath, fixtureSymbols);
  t.after(() => lib.close());

  let callback = () => 1;
  const ref = new WeakRef(callback);
  const pointer = lib.registerCallback(
    { arguments: ['i32'], return: 'i32' },
    callback,
  );

  lib.unrefCallback(pointer);
  callback = null;

  await gcUntil(
    'callback ref/unref throw after callback function is collected',
    () => ref.deref() === undefined,
  );

  t.assert.throws(() => lib.unrefCallback(pointer), {
    code: 'ERR_INVALID_ARG_VALUE',
    message: /Callback not found/,
  });
  t.assert.throws(() => lib.refCallback(pointer), {
    code: 'ERR_INVALID_ARG_VALUE',
    message: /Callback not found/,
  });

  lib.unregisterCallback(pointer);
});

test('callback ref/unref/unregister throw when library is closed', (t) => {
  const { lib } = ffi.dlopen(libraryPath, fixtureSymbols);
  const callback = lib.registerCallback(() => {});

  lib.close();

  t.assert.throws(() => lib.unregisterCallback(callback), /Library is closed/);
  t.assert.throws(() => lib.refCallback(callback), /Library is closed/);
  t.assert.throws(() => lib.unrefCallback(callback), /Library is closed/);
});
