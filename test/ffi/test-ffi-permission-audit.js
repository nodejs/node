'use strict';

const { skipIfFFIMissing } = require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');

skipIfFFIMissing();

test('permission audit does not deny ffi access', () => {
  const { stdout, stderr, status, signal } = spawnSync(process.execPath, [
    '--permission-audit',
    '--experimental-ffi',
    '-e',
    'const ffi = require("node:ffi"); ' +
      'console.log(JSON.stringify({ ' +
      'hasPermission: process.permission.has("ffi"), ' +
      'dlopen: typeof ffi.dlopen ' +
      '}));',
  ], {
    encoding: 'utf8',
  });

  assert.match(stderr, /ExperimentalWarning: FFI is an experimental feature/);
  assert.strictEqual(status, 0);
  assert.strictEqual(signal, null);
  assert.deepStrictEqual(JSON.parse(stdout), {
    hasPermission: false,
    dlopen: 'function',
  });
});
