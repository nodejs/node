'use strict';
var common = require('../common');
var assert = require('assert');
var child_process = require('child_process');

function test(fun, code) {
  var errors = 0;

  fun('does-not-exist', function(err) {
    assert.equal(err.code, code);
    assert(/does\-not\-exist/.test(err.cmd));
    errors++;
  });

  process.on('exit', function() {
    assert.equal(errors, 1);
  });
}

if (common.isWindows) {
  test(child_process.exec, 1); // exit code of cmd.exe
} else {
  test(child_process.exec, 127); // exit code of /bin/sh
}

test(child_process.execFile, 'ENOENT');
