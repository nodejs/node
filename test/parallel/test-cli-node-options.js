'use strict';
const common = require('../common');
if (process.config.variables.node_without_node_options)
  common.skip('missing NODE_OPTIONS support');

// Test options specified by env variable.

const assert = require('assert');
const exec = require('child_process').execFile;

common.refreshTmpDir();
process.chdir(common.tmpDir);

disallow('--version');
disallow('-v');
disallow('--help');
disallow('-h');
disallow('--eval');
disallow('-e');
disallow('--print');
disallow('-p');
disallow('-pe');
disallow('--check');
disallow('-c');
disallow('--interactive');
disallow('-i');
disallow('--v8-options');
disallow('--');
disallow('--no_warnings'); // Node options don't allow '_' instead of '-'.

function disallow(opt) {
  const options = { env: { NODE_OPTIONS: opt } };
  exec(process.execPath, options, common.mustCall(function(err) {
    const message = err.message.split(/\r?\n/)[1];
    const expect = `${process.execPath}: ${opt} is not allowed in NODE_OPTIONS`;

    assert.strictEqual(err.code, 9);
    assert.strictEqual(message, expect);
  }));
}

const printA = require.resolve('../fixtures/printA.js');

expect(`-r ${printA}`, 'A\nB\n');
expect('--no-deprecation', 'B\n');
expect('--no-warnings', 'B\n');
expect('--trace-warnings', 'B\n');
expect('--redirect-warnings=_', 'B\n');
expect('--trace-deprecation', 'B\n');
expect('--trace-sync-io', 'B\n');
expect('--trace-events-enabled', 'B\n');
expect('--track-heap-objects', 'B\n');
expect('--throw-deprecation', 'B\n');
expect('--zero-fill-buffers', 'B\n');
expect('--v8-pool-size=10', 'B\n');
if (common.hasCrypto) {
  expect('--use-openssl-ca', 'B\n');
  expect('--use-bundled-ca', 'B\n');
  expect('--openssl-config=_ossl_cfg', 'B\n');
}

// V8 options
expect('--abort-on-uncaught-exception', 'B\n');
expect('--abort_on_uncaught_exception', 'B\n');
expect('--abort_on-uncaught_exception', 'B\n');
expect('--max_old_space_size=0', 'B\n');
expect('--max-old_space-size=0', 'B\n');
expect('--max-old-space-size=0', 'B\n');

function expect(opt, want) {
  const printB = require.resolve('../fixtures/printB.js');
  const argv = [printB];
  const opts = {
    env: { NODE_OPTIONS: opt },
    maxBuffer: 1000000000,
  };
  exec(process.execPath, argv, opts, common.mustCall(function(err, stdout) {
    assert.ifError(err);
    if (!RegExp(want).test(stdout)) {
      console.error('For %j, failed to find %j in: <\n%s\n>',
                    opt, want, stdout);
      assert.fail(`Expected ${want}`);
    }
  }));
}
