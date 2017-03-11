'use strict';

// Tests that --pending-deprecation and NODE_PENDING_DEPRECATION both
// set the process.binding('config').pendingDeprecation flag that is
// used to determine if pending deprecation messages should be shown.
// The test is performed by launching two child processes that run
// this same test script with different arguments. If those exit with
// code 0, then the test passes. If they don't, it fails.

const common = require('../common');

const assert = require('assert');
const config = process.binding('config');
const fork = require('child_process').fork;

function message(name) {
  return `${name} did not set the process.binding('config').` +
         'pendingDeprecation flag.';
}

switch (process.argv[2]) {
  case 'env':
  case 'switch':
    assert.strictEqual(config.pendingDeprecation, true);
    break;
  default:
    // Verify that the flag is off by default.
    assert.strictEqual(config.pendingDeprecation, undefined);

    // Test the --pending-deprecation command line switch.
    fork(__filename, ['switch'], {
      execArgv: ['--pending-deprecation'],
      silent: true
    }).on('exit', common.mustCall((code) => {
      assert.strictEqual(code, 0, message('--pending-deprecation'));
    }));

    // Test the NODE_PENDING_DEPRECATION environment var.
    fork(__filename, ['env'], {
      env: {NODE_PENDING_DEPRECATION: 1},
      silent: true
    }).on('exit', common.mustCall((code) => {
      assert.strictEqual(code, 0, message('NODE_PENDING_DEPRECATION'));
    }));
}
