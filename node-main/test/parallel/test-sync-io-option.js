'use strict';
const common = require('../common');
const assert = require('assert');
const { promisify } = require('util');
const execFile = promisify(require('child_process').execFile);

// Test that --trace-sync-io works. In particular, a warning should be printed
// when it is enabled and synchronous I/O happens after the first event loop
// turn, and no warnings should occur when it is disabled or synchronous I/O
// happens before the first event loop turn is over.

if (process.argv[2] === 'late-sync-io') {
  setImmediate(() => {
    require('fs').statSync(__filename);
  });
  return;
} else if (process.argv[2] === 'early-sync-io') {
  require('fs').statSync(__filename);
  return;
}

(async function() {
  for (const { execArgv, variant, warnings } of [
    { execArgv: ['--trace-sync-io'], variant: 'late-sync-io', warnings: 1 },
    { execArgv: [], variant: 'late-sync-io', warnings: 0 },
    { execArgv: ['--trace-sync-io'], variant: 'early-sync-io', warnings: 0 },
    { execArgv: [], variant: 'early-sync-io', warnings: 0 },
  ]) {
    const { stdout, stderr } =
      await execFile(process.execPath, [...execArgv, __filename, variant]);
    assert.strictEqual(stdout, '');
    const actualWarnings =
      stderr.match(/WARNING: Detected use of sync API[\s\S]*statSync/g);
    if (warnings === 0)
      assert.strictEqual(actualWarnings, null);
    else
      assert.strictEqual(actualWarnings.length, warnings);
  }
})().then(common.mustCall());
