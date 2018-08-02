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
    assert.ok(stdout.includes('foo') && stdout.includes('bar'));
  });
};

test('cmd');
test('cmd.exe');
test('CMD');
test('powershell');

cp.exec('where bash', (error, stdout) => {
  if (error) {
    return;
  }
  test(stdout.trim());
});

cp.exec(`Get-ChildItem "${__dirname}" | Select-Object -Property Name`,
        { shell: 'PowerShell' }, (error, stdout, stderror) => {
          assert.ok(!error && !stderror);
          assert.ok(stdout.includes(
            'test-child-process-exec-any-shells-windows.js'));
        });
