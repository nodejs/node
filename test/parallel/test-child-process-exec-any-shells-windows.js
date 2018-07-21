'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

// This test is only relevant on Windows.
if (!common.isWindows)
  common.skip('Windows specific test.');

// This test ensures that child_process.exec can work with any shells.

const test = (shell) => {
  cp.exec('echo foo bar', { shell: shell }, (error, stdout, stderror) => {
    assert.ok(!error && !stderror);
    assert.ok(stdout.startsWith('foo bar'));
  });
};

test('cmd');
test('cmd.exe');
test('C:\\WINDOWS\\system32\\cmd.exe');
test('powershell');
test('C:\\Program Files\\Git\\bin\\bash.exe');
