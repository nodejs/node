'use strict';
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');

function test(fn, code) {
  fn('does-not-exist', common.mustCall(function(err) {
    assert.strictEqual(err.code, code);
    assert(err.cmd.includes('does-not-exist'));
  }));
}

if (common.isWindows) {
  test(child_process.exec, 1); // exit code of cmd.exe
} else {
  test(child_process.exec, 127); // exit code of /bin/sh
}

test(child_process.execFile, 'ENOENT');
