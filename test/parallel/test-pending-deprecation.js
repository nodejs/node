'use strict';

// Flags: --expose-internals
// Tests that --pending-deprecation and NODE_PENDING_DEPRECATION both set the
// `require('internal/options').getOptionValue('--pending-deprecation')`
// flag that is used to determine if pending deprecation messages should be
// shown. The test is performed by launching two child processes that run
// this same test script with different arguments. If those exit with
// code 0, then the test passes. If they don't, it fails.

// TODO(joyeecheung): instead of testing internals, test the actual features
// pending deprecations.

const common = require('../common');

const assert = require('assert');
const fork = require('child_process').fork;
const { getOptionValue } = require('internal/options');

function message(name) {
  return `${name} did not affect getOptionValue('--pending-deprecation')`;
}

switch (process.argv[2]) {
  case 'env':
  case 'switch':
    assert.strictEqual(
      getOptionValue('--pending-deprecation'),
      true
    );
    break;
  default:
    // Verify that the flag is off by default.
    const envvar = process.env.NODE_PENDING_DEPRECATION;
    assert.strictEqual(
      getOptionValue('--pending-deprecation'),
      !!(envvar && envvar[0] === '1')
    );

    // Test the --pending-deprecation command line switch.
    fork(__filename, ['switch'], {
      execArgv: ['--pending-deprecation', '--expose-internals'],
      silent: true
    }).on('exit', common.mustCall((code) => {
      assert.strictEqual(code, 0, message('--pending-deprecation'));
    }));

    // Test the --pending_deprecation command line switch.
    fork(__filename, ['switch'], {
      execArgv: ['--pending_deprecation', '--expose-internals'],
      silent: true
    }).on('exit', common.mustCall((code) => {
      assert.strictEqual(code, 0, message('--pending_deprecation'));
    }));

    // Test the NODE_PENDING_DEPRECATION environment var.
    fork(__filename, ['env'], {
      env: { ...process.env, NODE_PENDING_DEPRECATION: 1 },
      execArgv: ['--expose-internals'],
      silent: true
    }).on('exit', common.mustCall((code) => {
      assert.strictEqual(code, 0, message('NODE_PENDING_DEPRECATION'));
    }));
}
