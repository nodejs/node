// Flags: --experimental-ffi
'use strict';
const { spawnPromisified, skipIfFFIMissing } = require('../common');
skipIfFFIMissing();
const ffi = require('node:ffi');
const { suite, test } = require('node:test');

suite('accessing the node:ffi module', () => {
  test('cannot be accessed without the node: scheme', (t) => {
    t.assert.throws(() => {
      require('ffi');
    }, {
      code: 'MODULE_NOT_FOUND',
      message: /Cannot find module 'ffi'/,
    });
  });

  test('can be disabled with --no-experimental-ffi flag', async (t) => {
    const {
      stdout,
      stderr,
      code,
      signal,
    } = await spawnPromisified(process.execPath, [
      '--no-experimental-ffi',
      '-e',
      'require("node:ffi")',
    ]);

    t.assert.strictEqual(stdout, '');
    t.assert.match(stderr, /No such built-in module: node:ffi/);
    t.assert.notStrictEqual(code, 0);
    t.assert.strictEqual(signal, null);
  });

  test('exports dlopen factory', (t) => {
    t.assert.strictEqual(typeof ffi.dlopen, 'function');
  });

  test('does not export DynamicLibrary', (t) => {
    t.assert.strictEqual(ffi.DynamicLibrary, undefined);
  });

  test('exports UnsafeFnPointer constructor', (t) => {
    t.assert.strictEqual(typeof ffi.UnsafeFnPointer, 'function');
  });

  test('exports UnsafeCallback constructor', (t) => {
    t.assert.strictEqual(typeof ffi.UnsafeCallback, 'function');
  });

  test('exports UnsafePointer constructor', (t) => {
    t.assert.strictEqual(typeof ffi.UnsafePointer, 'function');
  });

  test('does not export Struct class', (t) => {
    t.assert.strictEqual(ffi.Struct, undefined);
  });
});
