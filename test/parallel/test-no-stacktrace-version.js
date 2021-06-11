'use strict';
// Flags: --no-stacktrace-version

// This test ensures that the version is hidden at the end of stacktraces

const common = require('../common');
const assert = require('assert');
const exec = require('child_process').exec;
const fixtures = require('../common/fixtures');

function errExec(script, callback) {
  const cmd = `"${process.argv[0]}" --no-stacktrace-version "${fixtures.path(script)}"`;
  return exec(cmd, (err, stdout, stderr) => {
    // There was some error
    assert.ok(err);

    // More than one line of error output.
    assert.ok(stderr.split('\n').length);

    // Proxy the args for more tests.
    callback(err, stdout, stderr);
  });
}

errExec('throws_error.js', common.mustCall((err, stdout, stderr) => {
  assert.match(stderr, /at node:internal\/main\/run_main_module:17:47\r?\n$/);
}));
