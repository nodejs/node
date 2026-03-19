// Flags: --experimental-ffi
'use strict';
const common = require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');

common.skipIfFFIMissing();

const ffi = require('node:ffi');

{
  assert.throws(() => {
    require('ffi');
  }, {
    code: 'MODULE_NOT_FOUND',
    message: /Cannot find module 'ffi'/,
  });
}

{
  const { stdout, stderr, status, signal } = spawnSync(process.execPath, [
    '--no-experimental-ffi',
    '-e',
    'require("node:ffi")',
  ], {
    encoding: 'utf8',
  });

  assert.strictEqual(stdout, '');
  assert.match(stderr, /No such built-in module: node:ffi/);
  assert.notStrictEqual(status, 0);
  assert.strictEqual(signal, null);
}

{
  const { stdout, stderr, status, signal } = spawnSync(process.execPath, [
    '--experimental-ffi',
    '-e',
    'const ffi = require("node:ffi"); ffi.DynamicLibrary("missing");',
  ], {
    encoding: 'utf8',
  });

  assert.strictEqual(stdout, '');
  assert.match(stderr, /Class constructor DynamicLibrary cannot be invoked without 'new'/);
  assert.notStrictEqual(status, 0);
  assert.strictEqual(signal, null);
}

{
  const expected = [
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
  ];

  assert.deepStrictEqual(Object.keys(ffi).sort(), expected);
  assert.strictEqual(typeof ffi.DynamicLibrary, 'function');
  assert.strictEqual(typeof ffi.dlopen, 'function');
  assert.strictEqual(typeof ffi.dlclose, 'function');
  assert.strictEqual(typeof ffi.dlsym, 'function');
  assert.strictEqual(typeof ffi.exportString, 'function');
  assert.strictEqual(typeof ffi.exportBuffer, 'function');
  assert.strictEqual(typeof ffi.getInt8, 'function');
  assert.strictEqual(typeof ffi.getUint8, 'function');
  assert.strictEqual(typeof ffi.getInt16, 'function');
  assert.strictEqual(typeof ffi.getUint16, 'function');
  assert.strictEqual(typeof ffi.getInt32, 'function');
  assert.strictEqual(typeof ffi.getUint32, 'function');
  assert.strictEqual(typeof ffi.getInt64, 'function');
  assert.strictEqual(typeof ffi.getUint64, 'function');
  assert.strictEqual(typeof ffi.getFloat32, 'function');
  assert.strictEqual(typeof ffi.getFloat64, 'function');
  assert.strictEqual(typeof ffi.setInt8, 'function');
  assert.strictEqual(typeof ffi.setUint8, 'function');
  assert.strictEqual(typeof ffi.setInt16, 'function');
  assert.strictEqual(typeof ffi.setUint16, 'function');
  assert.strictEqual(typeof ffi.setInt32, 'function');
  assert.strictEqual(typeof ffi.setUint32, 'function');
  assert.strictEqual(typeof ffi.setInt64, 'function');
  assert.strictEqual(typeof ffi.setUint64, 'function');
  assert.strictEqual(typeof ffi.setFloat32, 'function');
  assert.strictEqual(typeof ffi.setFloat64, 'function');
  assert.strictEqual(typeof ffi.toString, 'function');
  assert.strictEqual(typeof ffi.toBuffer, 'function');
  assert.strictEqual(typeof ffi.toArrayBuffer, 'function');
}
