// Flags: --experimental-permission --allow-env --allow-fs-read=* --allow-child-process
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

describe('permission: enumerate', () => {
  const error = JSON.stringify({
    code: 'ERR_ACCESS_DENIED',
    permission: 'Environment',
  });

  it('enumerate with --allow-env', () => {
    const { status } = runTest([
      '--allow-env',
      '-e',
      `
      // doesNotThrow
      Object.keys(process.env);
      `,
    ]);
    strictEqual(status, 0);
  });

  it('enumerate with --allow-env=ALLOWED_ONLY', () => {
    const { status } = runTest(
      [
        '--allow-env=ALLOWED_ONLY',
        '-e',
        `
        const { throws } = require('node:assert');
        throws(() => {
          Object.keys(process.env);
        }, ${error});
        `,
      ],
      {
        env: {
          ...process.env,
          ALLOWED_ONLY: 0,
        },
      }
    );
    strictEqual(status, 0);
  });
});

describe('permission: structuredClone', () => {
  const error = JSON.stringify({
    code: 'ERR_ACCESS_DENIED',
    permission: 'Environment',
  });

  it('structuredClone process.env with --allow-env', () => {
    const { status } = runTest([
      '--allow-env',
      '-e',
      `
      // doesNotThrow
      structuredClone(process.env);
      `,
    ]);
    strictEqual(status, 0);
  });

  it('structuredClone process.env with --allow-env=ALLOWED_ONLY', () => {
    const { status } = runTest(
      [
        '--allow-env=ALLOWED_ONLY',
        '-e',
        `
        const { throws } = require('node:assert');
        throws(() => {
          structuredClone(process.env);
        }, ${error});
        `,
      ],
      {
        env: {
          ...process.env,
          ALLOWED_ONLY: 0,
        },
      }
    );
    strictEqual(status, 0);
  });
});
