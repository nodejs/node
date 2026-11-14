'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

// Verify that fork() and execFile() call spawn through module.exports
// so that user-level wrapping of child_process.spawn works correctly.
// This is consistent with exec() which already uses module.exports.execFile().

if (process.argv[2] === 'child') {
  process.exit(0);
}

const originalSpawn = cp.spawn;

// Test that fork() uses module.exports.spawn.
cp.spawn = common.mustCall(function(...args) {
  cp.spawn = originalSpawn;
  return originalSpawn.apply(this, args);
});

const child = cp.fork(__filename, ['child']);
child.on('exit', common.mustCall((code) => {
  assert.strictEqual(code, 0);

  // Test that execFile() uses module.exports.spawn.
  cp.spawn = common.mustCall(function(...args) {
    cp.spawn = originalSpawn;
    return originalSpawn.apply(this, args);
  });

  cp.execFile(process.execPath, ['--version'], common.mustSucceed());
}));
