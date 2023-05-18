// Flags: --experimental-permission --allow-env=* --allow-fs-read=* --allow-child-process
'use strict';

require('../common');
const { spawnSync } = require('node:child_process');
const { debuglog } = require('node:util');
const { strictEqual } = require('node:assert');
const { describe, it } = require('node:test');

const debug = debuglog('test');

function runTest(args, options = {}) {
  const { status, stdout, stderr } = spawnSync(
    process.execPath,
    ['--experimental-permission', '--allow-fs-read=*', ...args],
    options
  );
  debug('status:', status);
  if (status) debug('stdout:', stdout?.toString().split('\n'));
  if (status) debug('stderr:', stderr?.toString().split('\n'));
  return { status, stdout, stderr };
}

describe('permission: "env" access on worker thread', () => {
  it('worker_threads', () => {
    const { status } = runTest([
      '--allow-worker',
      '-e',
      `
      const assert = require('node:assert');
      const { Worker, SHARE_ENV } = require('node:worker_threads');
      const w = new Worker('process.env.UNDEFINED', { eval: true });
      w.on('error', () => {});
      w.on('exit', (code) => assert.strictEqual(code, 1));
      `,
    ]);
    strictEqual(status, 0);
  });

  it('worker_threads with SHARE_ENV', () => {
    const { status } = runTest([
      '--allow-worker',
      '-e',
      `
      const assert = require('node:assert');
      const { Worker, SHARE_ENV } = require('node:worker_threads');
      const w = new Worker('process.env.UNDEFINED', { eval: true, env: SHARE_ENV });
      w.on('error', () => {});
      w.on('exit', (code) => assert.strictEqual(code, 1));
      `,
    ]);
    strictEqual(status, 0);
  });

  it('worker_threads with DENIED_IN_MAIN_THREAD', () => {
    const error = JSON.stringify({
      code: 'ERR_ACCESS_DENIED',
      permission: 'Environment',
    });

    const { status } = runTest([
      '--allow-worker',
      '--allow-env=*,-DENIED_IN_MAIN_THREAD',
      '-e',
      `
      const { throws, strictEqual } = require('node:assert');
      const { Worker, SHARE_ENV } = require('node:worker_threads');
      const w = new Worker('process.env.DENIED_IN_MAIN_THREAD', {
        eval: true,
        env: { DENIED_IN_MAIN_THREAD: 1 },
      });
      w.on('exit', (code) => strictEqual(code, 0));
      throws(() => {
        process.env['DENIED_IN_MAIN_THREAD']
      }, ${error});
      `,
    ]);
    strictEqual(status, 0);
  });
});
