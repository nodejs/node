// Flags: --experimental-ffi
'use strict';
const { skipIfFFIMissing, isWindows, skip } = require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { test } = require('node:test');
const { fixtureSymbols, libraryPath } = require('./ffi-test-common');

skipIfFFIMissing();
if (isWindows) {
  skip('This test currently relies on POSIX APIs');
}

test('writing to readonly memory via buffer fails', () => {
  const symbols = JSON.stringify(fixtureSymbols);
  const libPath = JSON.stringify(libraryPath);
  const { stdout, status } = spawnSync(process.execPath, [
    '--experimental-ffi',
    '-p',
    `
    const ffi = require('node:ffi');
    const { functions } = ffi.dlopen(${libPath}, ${symbols})
    const p = functions.readonly_memory();
    const b = ffi.toBuffer(p, 4096, false);
    b[0] = 42;
    console.log('success');
    `,
  ]);
  assert.notStrictEqual(status, 0);
  assert.strictEqual(stdout.length, 0);
});
