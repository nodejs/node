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

describe('permission: "env" access', () => {
  const error = JSON.stringify({
    code: 'ERR_ACCESS_DENIED',
    permission: 'Environment',
  });

  it('get', () => {
    const { status } = runTest([
      '-e',
      `
      const { throws } = require('node:assert');
      throws(() => {
        process.env.UNDEFINED;
      }, ${error});`,
    ]);
    strictEqual(status, 0);
  });

  it('set', () => {
    const { status } = runTest([
      '-e',
      `
      const { throws } = require('node:assert');
      throws(() => {
        process.env.UNDEFINED = 0;
      }, ${error});`,
    ]);
    strictEqual(status, 0);
  });

  it('query', () => {
    const { status } = runTest([
      '-e',
      `
      const { throws } = require('node:assert');
      throws(() => {
        'UNDEFINED' in process.env;
      }, ${error});`,
    ]);
    strictEqual(status, 0);
  });

  it('delete', () => {
    const { status } = runTest([
      '-e',
      `
      const { throws } = require('node:assert');
      throws(() => {
        delete process.env.UNDEFINED;
      }, ${error});`,
    ]);
    strictEqual(status, 0);
  });

  it('enumerate', () => {
    const { status } = runTest([
      '-e',
      `
      const { throws } = require('node:assert');
      throws(() => {
        Object.keys(process.env);
      }, ${error});`,
    ]);
    strictEqual(status, 0);
  });

  it('structuredClone', () => {
    const { status } = runTest([
      '-e',
      `
      const { throws } = require('node:assert');
      throws(() => {
        structuredClone(process.env);
      }, ${error});`,
    ]);
    strictEqual(status, 0);
  });
});

describe('permission: "env" access with --allow-env=*', () => {
  it('get', () => {
    const { status } = runTest([
      '--allow-env=*',
      '-e',
      `
      // doesNotThrow
      process.env.UNDEFINED;
      `,
    ]);
    strictEqual(status, 0);
  });

  it('set', () => {
    const { status } = runTest([
      '--allow-env=*',
      '-e',
      `
      // doesNotThrow
      process.env.UNDEFINED = 0;
      `,
    ]);
    strictEqual(status, 0);
  });

  it('query', () => {
    const { status } = runTest([
      '--allow-env=*',
      '-e',
      `
      // doesNotThrow
      'UNDEFINED' in process.env;
      `,
    ]);
    strictEqual(status, 0);
  });

  it('delete', () => {
    const { status } = runTest([
      '--allow-env=*',
      '-e',
      `
      // doesNotThrow
      delete process.env.UNDEFINED;
      `,
    ]);
    strictEqual(status, 0);
  });

  it('enumerate', () => {
    const { status } = runTest([
      '--allow-env=*',
      '-e',
      `
      // doesNotThrow
      Object.keys(process.env);
      `,
    ]);
    strictEqual(status, 0);
  });

  it('structuredClone', () => {
    const { status } = runTest([
      '--allow-env=*',
      '-e',
      `
      // doesNotThrow
      structuredClone(process.env);
      `,
    ]);
    strictEqual(status, 0);
  });
});
