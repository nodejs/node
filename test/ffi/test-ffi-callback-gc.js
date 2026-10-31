// Flags: --expose-gc
'use strict';
const { skipIfFFIMissing } = require('../common');
skipIfFFIMissing();

const { gcUntil } = require('../common/gc');
const test = require('node:test');
const ffi = require('node:ffi');
const { fixtureSymbols, libraryPath } = require('./ffi-test-common');

const signature = { arguments: ['i32'], return: 'i32' };

test('registered callbacks keep an unreachable library alive', async (t) => {
  // A separate handle provides the native function that invokes the callback.
  const { lib: keeper, functions: symbols } =
    ffi.dlopen(libraryPath, fixtureSymbols);
  t.after(() => keeper.close());

  let lib = new ffi.DynamicLibrary(libraryPath);
  const ref = new WeakRef(lib);
  const pointer = lib.registerCallback(signature, (x) => x * 2);
  t.assert.strictEqual(symbols.call_int_callback(pointer, 21), 42);

  lib = null;
  for (let i = 0; i < 5; i++) {
    await gcUntil('library with callbacks stays alive', () => true, 1);
    t.assert.strictEqual(typeof ref.deref(), 'object');
    t.assert.strictEqual(symbols.call_int_callback(pointer, 21), 42);
  }

  ref.deref().unregisterCallback(pointer);
});

test('library becomes collectable after its callbacks are unregistered', async () => {
  let lib = new ffi.DynamicLibrary(libraryPath);
  const ref = new WeakRef(lib);
  const first = lib.registerCallback(signature, (x) => x);
  const second = lib.registerCallback(signature, (x) => x);
  lib.unregisterCallback(first);
  lib.unregisterCallback(second);
  lib = null;

  await gcUntil('library is collected after unregisterCallback',
                () => ref.deref() === undefined);
});

test('library becomes collectable after close()', async () => {
  let lib = new ffi.DynamicLibrary(libraryPath);
  const ref = new WeakRef(lib);
  lib.registerCallback(signature, (x) => x);
  lib.close();
  lib = null;

  await gcUntil('library is collected after close()',
                () => ref.deref() === undefined);
});

test('unrefCallback() does not release the callback trampoline', async (t) => {
  const { lib: keeper, functions: symbols } =
    ffi.dlopen(libraryPath, fixtureSymbols);
  t.after(() => keeper.close());

  let lib = new ffi.DynamicLibrary(libraryPath);
  const ref = new WeakRef(lib);
  const pointer = lib.registerCallback(signature, () => 1);
  lib.unrefCallback(pointer);
  lib = null;

  for (let i = 0; i < 5; i++) {
    await gcUntil('library with unref callbacks stays alive', () => true, 1);
    t.assert.strictEqual(typeof ref.deref(), 'object');
  }
  // The JS function may be gone, but the trampoline must still be callable.
  symbols.call_int_callback(pointer, 21);

  ref.deref().unregisterCallback(pointer);
});

test('libraries with registered callbacks do not block teardown', async (t) => {
  const { Worker } = require('node:worker_threads');
  const { once } = require('node:events');
  const worker = new Worker(`
    const ffi = require('node:ffi');
    const lib = new ffi.DynamicLibrary(${JSON.stringify(libraryPath)});
    lib.registerCallback(${JSON.stringify(signature)}, (x) => x);
    globalThis.lib = lib;
  `, { eval: true });
  const [code] = await once(worker, 'exit');
  t.assert.strictEqual(code, 0);
});
