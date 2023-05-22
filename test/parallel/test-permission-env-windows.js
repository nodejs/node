// Flags: --experimental-permission --allow-env --allow-fs-read=* --allow-child-process
'use strict';

const common = require('../common');
const { spawnSync } = require('node:child_process');
const { debuglog } = require('node:util');
const { strictEqual } = require('node:assert');
const { describe, it } = require('node:test');

if (!common.isWindows) {
  common.skip('process.env on Windows test');
}

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

describe('permission: reference', () => {
  const error = JSON.stringify({
    code: 'ERR_ACCESS_DENIED',
    permission: 'Environment',
  });

  it('reference on Windows is case-insensitive', () => {
    const { status } = runTest([
      '--allow-env=FoO',
      '-e',
      `
      const { throws, strictEqual } = require('node:assert');

      strictEqual(process.permission.has('env', 'FOO'), true);
      strictEqual(process.permission.has('env', 'foo'), true);
      strictEqual(process.permission.has('env', 'FoO'), true);

      // doesNotThrow
      process.env.FOO;

      // doesNotThrow
      process.env.foo;

      // doesNotThrow
      process.env.FoO;

      throws(() => {
        process.env.OTHERS;
      }, ${error});
      `,
    ]);
    strictEqual(status, 0);
  });
});
