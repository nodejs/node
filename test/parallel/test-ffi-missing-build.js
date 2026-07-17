'use strict';

const common = require('../common');

if (process.config.variables.node_use_ffi) {
  common.skip('requires a build without FFI support');
}

const assert = require('node:assert');
const { spawnSync } = require('node:child_process');

{
  const { stderr, status, signal } = spawnSync(process.execPath, [
    '--experimental-ffi',
    '-e',
    '',
  ], {
    encoding: 'utf8',
  });

  assert.match(stderr, /--experimental-ffi/);
  assert.notStrictEqual(status, 0);
  assert.strictEqual(signal, null);
}

{
  const { stderr, status, signal } = spawnSync(process.execPath, [
    '--no-experimental-ffi',
    '-e',
    '',
  ], {
    encoding: 'utf8',
  });

  assert.match(stderr, /--no-experimental-ffi/);
  assert.notStrictEqual(status, 0);
  assert.strictEqual(signal, null);
}

{
  const { stderr, status, signal } = spawnSync(process.execPath, [
    '--permission',
    '--allow-ffi',
    '-e',
    '',
  ], {
    encoding: 'utf8',
  });

  assert.match(stderr, /--allow-ffi/);
  assert.notStrictEqual(status, 0);
  assert.strictEqual(signal, null);
}

{
  const { stderr, status, signal } = spawnSync(process.execPath, [
    '-e',
    'require("node:ffi")',
  ], {
    encoding: 'utf8',
  });

  assert.match(stderr, /No such built-in module: node:ffi/);
  assert.notStrictEqual(status, 0);
  assert.strictEqual(signal, null);
}

{
  const { stdout, status, signal } = spawnSync(process.execPath, [
    '--permission',
    '--expose-internals',
    '-p',
    'JSON.stringify(require("internal/process/permission").availableFlags())',
  ], {
    encoding: 'utf8',
  });

  assert.strictEqual(status, 0);
  assert.strictEqual(signal, null);
  assert.ok(!JSON.parse(stdout).includes('--allow-ffi'));
}
