'use strict';
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');
const fs = require('fs');

if (common.isWindows) {
  common.skip('no RLIMIT_NOFILE on Windows');
  return;
}

const ulimit = Number(child_process.execSync('ulimit -Hn'));
if (ulimit > 64 || Number.isNaN(ulimit)) {
  // Sorry about this nonsense. It can be replaced if
  // https://github.com/nodejs/node-v0.x-archive/pull/2143#issuecomment-2847886
  // ever happens.
  const result = child_process.spawnSync(
    '/bin/sh',
    ['-c', `ulimit -n 64 && '${process.execPath}' '${__filename}'`]
  );
  assert.strictEqual(result.stdout.toString(), '');
  assert.strictEqual(result.stderr.toString(), '');
  assert.strictEqual(result.status, 0);
  assert.strictEqual(result.error, undefined);
  return;
}

const openFds = [];

for (;;) {
  try {
    openFds.push(fs.openSync(__filename, 'r'));
  } catch (err) {
    assert.strictEqual(err.code, 'EMFILE');
    break;
  }
}

// Should emit an error, not throw.
const proc = child_process.spawn(process.execPath, ['-e', '0']);

proc.on('error', common.mustCall(function(err) {
  assert.strictEqual(err.code, 'EMFILE');
}));

proc.on('exit', common.mustNotCall('"exit" event should not be emitted'));

// close one fd for LSan
if (openFds.length >= 1) {
  fs.closeSync(openFds.pop());
}
