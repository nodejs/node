// Flags: --permission --allow-ffi --allow-child-process --experimental-ffi --allow-fs-read=* --allow-fs-write=*
'use strict';

const { skipIfFFIMissing } = require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');
const { fixtureSymbols, libraryPath } = require('./ffi-test-common');

skipIfFFIMissing();

const ffi = require('node:ffi');

test('allow-ffi enables ffi APIs and permission flag discovery', () => {
  assert.ok(process.permission.has('ffi'));
  assert.strictEqual(typeof ffi.DynamicLibrary, 'function');
  assert.strictEqual(ffi.toString(0n), null);

  const { stdout, status, signal } = spawnSync(process.execPath, [
    '--permission',
    '--expose-internals',
    '--experimental-ffi',
    '-p',
    'JSON.stringify(require("internal/process/permission").availableFlags())',
  ], {
    encoding: 'utf8',
  });

  assert.strictEqual(status, 0);
  assert.strictEqual(signal, null);
  assert.ok(JSON.parse(stdout).includes('--allow-ffi'));
});

test('allow-ffi permits library and memory operations', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i32: fixtureSymbols.add_i32,
    allocate_memory: fixtureSymbols.allocate_memory,
    deallocate_memory: fixtureSymbols.deallocate_memory,
  });

  try {
    assert.strictEqual(functions.add_i32(19, 23), 42);
    assert.strictEqual(ffi.dlsym(lib, 'add_i32'), functions.add_i32.pointer);

    const ptr = functions.allocate_memory(8n);
    try {
      ffi.exportString('allow', ptr, 8);
      assert.strictEqual(ffi.toString(ptr), 'allow');
      assert.deepStrictEqual([...ffi.toBuffer(ptr, 6)], [...Buffer.from('allow\0')]);
    } finally {
      functions.deallocate_memory(ptr);
    }
  } finally {
    lib.close();
  }
});
