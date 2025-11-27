'use strict';

// Regression test for memory leaks in test runner
// Without the fix, these tests could hang or accumulate resources

const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');
const { writeFileSync } = require('fs');
const { join } = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

// Test 1: Verify readline interface cleanup
// Without fix: readline.Interface remains open when child has stderr output
// This could cause the process to hang or resources to leak
{
  const testFile = join(tmpdir.path, 'test-with-stderr.js');
  writeFileSync(
    testFile,
    `
    const test = require('node:test');
    test('test with stderr', () => {
      process.stderr.write('line1\\n');
      process.stderr.write('line2\\n');
    });
  `
  );

  const child = spawn(process.execPath, [
    '--test',
    '--test-isolation=process',
    testFile,
  ]);

  let stdout = '';
  child.stdout.on('data', (chunk) => {
    stdout += chunk;
  });

  child.on(
    'close',
    common.mustCall((code) => {
      assert.strictEqual(code, 0);
      assert.match(stdout, /tests 1/);
      assert.match(stdout, /pass 1/);
    })
  );

  // Without fix: process might not exit cleanly due to unclosed readline
  const timeout = setTimeout(() => {
    if (!child.killed) {
      child.kill();
      assert.fail('Process did not exit within timeout');
    }
  }, 30000);
  timeout.unref();
}

// Test 2: Verify watch mode can be stopped cleanly
// Without fix: watch mode listener might not be removed on abort
{
  const testFile = join(tmpdir.path, 'test-watch.js');
  writeFileSync(
    testFile,
    `
    const test = require('node:test');
    test('watch test', () => {});
  `
  );

  const child = spawn(process.execPath, ['--test', '--watch', testFile]);

  let stdout = '';
  child.stdout.on('data', (chunk) => {
    stdout += chunk;
    if (stdout.includes('pass 1')) {
      child.kill('SIGTERM');
    }
  });

  child.on(
    'close',
    common.mustCall(() => {
      assert.match(stdout, /tests 1/);
      assert.match(stdout, /pass 1/);
    })
  );

  // Without fix: process might not respond to SIGTERM properly
  const timeout = setTimeout(() => {
    if (!child.killed) {
      child.kill('SIGKILL');
      assert.fail('Watch mode did not exit cleanly on SIGTERM');
    }
  }, 10000);
  timeout.unref();
}
