'use strict';

// Test exec() when a timeout is set, but not expired.

const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

const {
  cleanupStaleProcess,
  logAfterTime
} = require('../common/child_process');

const kTimeoutNotSupposedToExpire = 2 ** 30;
const childRunTime = common.platformTimeout(100);

// The time spent in the child should be smaller than the timeout below.
assert(childRunTime < kTimeoutNotSupposedToExpire);

if (process.argv[2] === 'child') {
  logAfterTime(childRunTime);
  return;
}

// The property is also present on errors that are not caused by the timeout.
cp.execFile('this-command-does-not-exist', { timeout: 2 ** 30 },
            common.mustCall((err) => {
              assert.strictEqual(err.code, 'ENOENT');
              assert.strictEqual(err.timedOut, false);
            }));

// The timeout expires after the child exited, while a descendant still keeps
// its stdio open: the signal is not sent, so timedOut stays false. The child
// prints the pid of the descendant and the test ends it once the timer has
// fired, so the test does not depend on how fast the child starts.
{
  const timeout = common.platformTimeout(5000);
  const child = cp.execFile(process.execPath, ['-e', `
    const { spawn } = require('child_process');
    // Detached: on Windows a non-detached descendant is killed with the job
    // object of the child when the child exits.
    const child = spawn(process.execPath,
                        ['-e', 'setTimeout(() => {}, ${common.platformTimeout(30000)})'],
                        { stdio: 'inherit', detached: true });
    child.unref();
    process.stdout.write(String(child.pid));
  `], { timeout }, common.mustSucceed(() => {
    assert.strictEqual(child.exitCode, 0);
    assert.strictEqual(child.timedOut, false);
  }));
  const pid = new Promise((resolve) => {
    child.stdout.once('data', (chunk) => resolve(Number(String(chunk))));
  });
  child.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
    // The timeout of the child started before this timer, so it fires first.
    // The descendant is ended after that, which closes the stdio of the child.
    setTimeout(async () => process.kill(await pid), timeout);
  }));
}

const [cmd, opts] = common.escapePOSIXShell`"${process.execPath}" "${__filename}" child`;

const child = cp.exec(cmd, {
  ...opts,
  timeout: kTimeoutNotSupposedToExpire,
}, common.mustSucceed((stdout, stderr) => {
  assert.strictEqual(stdout.trim(), 'child stdout');
  assert.strictEqual(stderr.trim(), 'child stderr');
  assert.strictEqual(child.timedOut, false);
}));

cleanupStaleProcess(__filename);
