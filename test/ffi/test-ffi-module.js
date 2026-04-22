// Flags: --experimental-ffi
'use strict';
const common = require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');

common.skipIfFFIMissing();

test('ffi cannot be loaded without node: prefix', () => {
  assert.throws(() => {
    require('ffi');
  }, {
    code: 'MODULE_NOT_FOUND',
    message: /Cannot find module 'ffi'/,
  });
});

test('ffi builtin is unavailable when disabled', () => {
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
});

test('ffi builtin is listed', () => {
  const { stdout, stderr, status, signal } = spawnSync(process.execPath, [
    '-p',
    'require("node:module").builtinModules.includes("node:ffi")',
  ], {
    encoding: 'utf8',
  });

  assert.strictEqual(stdout.trim(), 'true');
  assert.strictEqual(stderr, '');
  assert.strictEqual(status, 0);
  assert.strictEqual(signal, null);
});

test('ffi can be imported from ESM', () => {
  const { stdout, stderr, status, signal } = spawnSync(process.execPath, [
    '--experimental-ffi',
    '--input-type=module',
    '-e',
    'import * as ffi from "node:ffi"; console.log(typeof ffi.dlopen);',
  ], {
    encoding: 'utf8',
  });

  assert.strictEqual(stdout.trim(), 'function');
  assert.match(stderr, /ExperimentalWarning: FFI is an experimental feature/);
  assert.strictEqual(status, 0);
  assert.strictEqual(signal, null);
});

test('DynamicLibrary requires new', () => {
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
});

test('ffi exports expected API surface', () => {
  const ffi = require('node:ffi');
  const expected = [
    'DynamicLibrary',
    'dlclose',
    'dlopen',
    'dlsym',
    'exportArrayBuffer',
    'exportArrayBufferView',
    'exportBuffer',
    'exportString',
    'getFloat32',
    'getFloat64',
    'getInt16',
    'getInt32',
    'getInt64',
    'getInt8',
    'getRawPointer',
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
    'suffix',
    'toArrayBuffer',
    'toBuffer',
    'toString',
    'types',
  ];

  assert.deepStrictEqual(Object.keys(ffi).sort(), expected);
  assert.strictEqual(typeof ffi.DynamicLibrary, 'function');
  assert.strictEqual(typeof ffi.dlopen, 'function');
  assert.strictEqual(typeof ffi.dlclose, 'function');
  assert.strictEqual(typeof ffi.dlsym, 'function');
  assert.strictEqual(typeof ffi.exportArrayBuffer, 'function');
  assert.strictEqual(typeof ffi.exportArrayBufferView, 'function');
  assert.strictEqual(typeof ffi.exportString, 'function');
  assert.strictEqual(typeof ffi.exportBuffer, 'function');
  assert.strictEqual(typeof ffi.getRawPointer, 'function');
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
  assert.strictEqual(typeof ffi.types, 'object');
});

test('ffi.types exports canonical type constants', () => {
  const ffi = require('node:ffi');
  const expected = {
    __proto__: null,
    VOID: 'void',
    POINTER: 'pointer',
    BUFFER: 'buffer',
    ARRAY_BUFFER: 'arraybuffer',
    FUNCTION: 'function',
    BOOL: 'bool',
    CHAR: 'char',
    STRING: 'string',
    FLOAT: 'float',
    DOUBLE: 'double',
    INT_8: 'int8',
    UINT_8: 'uint8',
    INT_16: 'int16',
    UINT_16: 'uint16',
    INT_32: 'int32',
    UINT_32: 'uint32',
    INT_64: 'int64',
    UINT_64: 'uint64',
    FLOAT_32: 'float32',
    FLOAT_64: 'float64',
  };

  assert.deepStrictEqual(ffi.types, expected);
  assert.strictEqual(Object.isFrozen(ffi.types), true);
});
