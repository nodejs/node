'use strict';
var common = require('../common');
var assert = require('assert');
var child_process = require('child_process');

function test(fun, code) {
  child_process[fun]('does-not-exist', common.mustCall(function(er, _, stderr) {
    assert.equal(er.code, code);
    assert(/does\-not\-exist/.test(er.cmd));
    if (fun === 'exec') {
      assert(/does-not-exist: not found/.test(stderr));
    }
  }));
}

if (common.isWindows) {
  test('exec', 1); // exit code of cmd.exe
} else {
  test('exec', 127); // exit code of /bin/sh
}

test('execFile', 'ENOENT');
