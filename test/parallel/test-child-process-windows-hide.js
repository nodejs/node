// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const internalCp = require('internal/child_process');
const cmd = process.execPath;
const args = ['-p', '42'];
const options = { windowsHide: true };

// Since windowsHide isn't really observable, monkey patch spawn() and
// spawnSync() to verify that the flag is being passed through correctly.
const originalSpawn = internalCp.ChildProcess.prototype.spawn;
const originalSpawnSync = internalCp.spawnSync;

internalCp.ChildProcess.prototype.spawn = common.mustCall(function(options) {
  assert.strictEqual(options.windowsHide, true);
  return originalSpawn.apply(this, arguments);
}, 2);

internalCp.spawnSync = common.mustCall(function(options) {
  assert.strictEqual(options.windowsHide, true);
  return originalSpawnSync.apply(this, arguments);
});

{
  const child = cp.spawnSync(cmd, args, options);

  assert.strictEqual(child.status, 0);
  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.stdout.toString().trim(), '42');
  assert.strictEqual(child.stderr.toString().trim(), '');
}

{
  const child = cp.spawn(cmd, args, options);

  child.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }));
}

{
  const callback = common.mustCall((err, stdout, stderr) => {
    assert.ifError(err);
    assert.strictEqual(stdout.trim(), '42');
    assert.strictEqual(stderr.trim(), '');
  });

  cp.execFile(cmd, args, options, callback);
}
