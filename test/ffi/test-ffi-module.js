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

  test('shape of exports', async (t) => {
    const expectedExports = [
      'DynamicLibrary',
      'dlclose',
      'dlopen',
      'dlsym',
      'exportBuffer',
      'exportString',
      'getFloat32',
      'getFloat64',
      'getInt16',
      'getInt32',
      'getInt64',
      'getInt8',
      'getUint16',
      'getUint32',
      'getUint64',
      'getUint8',
      'setFloat32',
      'setFloat64',
      'setInt16',
      'setInt32',
      'setInt64',
      'setInt8',
      'setUint16',
      'setUint32',
      'setUint64',
      'setUint8',
      'toArrayBuffer',
      'toBuffer',
      'toString',
    ].sort();

    t.assert.deepStrictEqual(Object.keys(ffi).sort(), expectedExports);

    for (let i = 0; i < expectedExports.length; i++) {
      t.assert.strictEqual(typeof ffi[expectedExports[i]], 'function');
    }
  });
});
