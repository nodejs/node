'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

if (process.argv[2] === 'child') {
  setTimeout(() => {
    // The following console statements are part of the test.
    console.log('child stdout');
    console.error('child stderr');
  }, common.platformTimeout(1000));
  return;
}

const cmd = `"${process.execPath}" "${__filename}" child`;

// Test the case where a timeout is set, and it expires.
cp.exec(cmd, { timeout: 1 }, common.mustCall((err, stdout, stderr) => {
  let sigterm = 'SIGTERM';
  assert.strictEqual(err.killed, true);
  // TODO OpenBSD returns a null signal and 143 for code
  if (common.isOpenBSD) {
    assert.strictEqual(err.code, 143);
    sigterm = null;
  } else {
    assert.strictEqual(err.code, null);
  }
  // At least starting with Darwin Kernel Version 16.4.0, sending a SIGTERM to a
  // process that is still starting up kills it with SIGKILL instead of SIGTERM.
  // See: https://github.com/libuv/libuv/issues/1226
  if (common.isOSX)
    assert.ok(err.signal === 'SIGTERM' || err.signal === 'SIGKILL');
  else
    assert.strictEqual(err.signal, sigterm);
  assert.strictEqual(err.cmd, cmd);
  assert.strictEqual(stdout.trim(), '');
  assert.strictEqual(stderr.trim(), '');
}));

// Test with a different kill signal.
cp.exec(cmd, {
  timeout: 1,
  killSignal: 'SIGKILL'
}, common.mustCall((err, stdout, stderr) => {
  assert.strictEqual(err.killed, true);
  assert.strictEqual(err.code, null);
  assert.strictEqual(err.signal, 'SIGKILL');
  assert.strictEqual(err.cmd, cmd);
  assert.strictEqual(stdout.trim(), '');
  assert.strictEqual(stderr.trim(), '');
}));

// Test the case where a timeout is set, but not expired.
cp.exec(cmd, { timeout: 2 ** 30 }, common.mustCall((err, stdout, stderr) => {
  assert.ifError(err);
  assert.strictEqual(stdout.trim(), 'child stdout');
  assert.strictEqual(stderr.trim(), 'child stderr');
}));

// Workaround for Windows Server 2008R2
// When CMD is used to launch a process and CMD is killed too quickly, the
// process can stay behind running in suspended state, never completing.
if (common.isWindows) {
  process.once('beforeExit', () => {
    const basename = __filename.replace(/.*[/\\]/g, '');
    cp.execFileSync(`${process.env.SystemRoot}\\System32\\wbem\\WMIC.exe`, [
      'process',
      'where',
      `commandline like '%${basename}%child'`,
      'delete',
      '/nointeractive'
    ]);
  });
}
