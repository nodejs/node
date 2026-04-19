// Flags: --experimental-ffi --expose-gc
'use strict';
const { skipIfFFIMissing } = require('../common');
skipIfFFIMissing();

const { gcUntil } = require('../common/gc');
const test = require('node:test');
const ffi = require('node:ffi');
const { fixtureSymbols, libraryPath } = require('./ffi-test-common');

test('ffi unrefCallback releases callback function', async (t) => {
  const { lib, functions: symbols } = ffi.dlopen(libraryPath, fixtureSymbols);
  t.after(() => lib.close());

  let callback = () => 1;
  const ref = new WeakRef(callback);
  const pointer = lib.registerCallback(
    { parameters: ['i32'], result: 'i32' },
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

test('ffi refCallback retains callback function', async (t) => {
  const { lib } = ffi.dlopen(libraryPath, fixtureSymbols);
  t.after(() => lib.close());

  let callback = () => 1;
  const ref = new WeakRef(callback);
  const pointer = lib.registerCallback({ result: 'i32' }, callback);

  lib.unrefCallback(pointer);
  lib.refCallback(pointer);
  callback = null;

  for (let i = 0; i < 5; i++) {
    await gcUntil('ffi refCallback retains callback function', () => true, 1);
    t.assert.strictEqual(typeof ref.deref(), 'function');
  }

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
