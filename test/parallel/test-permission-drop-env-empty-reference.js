'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const { spawnSync } = require('child_process');

// An empty reference must not drop every environment variable.
const { status, stdout, stderr } = spawnSync(process.execPath, [
  '--permission',
  '--allow-env=A,B',
  '-e',
  `
    const assert = require('assert');
    assert.throws(() => process.permission.drop('env', ''), {
      code: 'ERR_INVALID_ARG_VALUE',
    });
    assert.throws(() => process.permission.drop('env', new Uint8Array(0)), {
      code: 'ERR_INVALID_ARG_VALUE',
    });
    assert.strictEqual(process.env.A, '1');
    assert.strictEqual(process.env.B, '2');
  `,
], {
  env: { ...process.env, A: '1', B: '2' },
  encoding: 'utf8',
});

assert.strictEqual(status, 0, `stdout: ${stdout}\nstderr: ${stderr}`);
