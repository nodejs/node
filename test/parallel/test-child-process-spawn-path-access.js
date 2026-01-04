'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Scenario: PATH contains directories that are inaccessible or are actually files.
// The system spawn (execvp) might return EACCES in these cases on some platforms.
// We want to ensure Node.js consistently reports ENOENT if the command is truly missing.

const noPermDir = path.join(tmpdir.path, 'no-perm-dir');
fs.mkdirSync(noPermDir);

const fileInPath = path.join(tmpdir.path, 'file-in-path');
fs.writeFileSync(fileInPath, '');

if (!common.isWindows) {
  try {
    fs.chmodSync(noPermDir, '000');
  } catch (e) {
    // If we can't chmod (e.g. root or weird fs), skip the permission part of the test
    // but keep the structure.
    console.log('# Skipped chmod 000 on no-perm-dir due to error:', e.message);
  }
}

// Ensure cleanup restores permissions so tmpdir can be removed
process.prependListener('exit', () => {
  if (!common.isWindows && fs.existsSync(noPermDir)) {
    try {
      fs.chmodSync(noPermDir, '777');
    } catch {
      // Ignore cleanup errors during exit
    }
  }
});

const env = { ...process.env };
const sep = path.delimiter;

// Prepend the problematic entries to PATH
env.PATH = `${noPermDir}${sep}${fileInPath}${sep}${env.PATH}`;

const command = 'command-that-does-not-exist-at-all-' + Date.now();

const child = cp.spawn(command, { env });

child.on('error', common.mustCall((err) => {
  assert.strictEqual(err.code, 'ENOENT');
  assert.strictEqual(err.syscall, `spawn ${command}`);
}));

// Also test sync
try {
  cp.spawnSync(command, { env });
} catch (err) {
  assert.strictEqual(err.code, 'ENOENT');
}

// Case: File exists but is not executable. Should NOT be normalized to ENOENT.
if (!common.isWindows) {
  const nonExecFile = path.join(tmpdir.path, 'non-executable');
  fs.writeFileSync(nonExecFile, 'echo "should not run"');
  fs.chmodSync(nonExecFile, '644');

  const env2 = { ...process.env, PATH: tmpdir.path };
  const child2 = cp.spawn('non-executable', { env: env2 });
  child2.on('error', common.mustCall((err) => {
    // It should stay EACCES because the file actually exists
    assert.strictEqual(err.code, 'EACCES');
  }));

  // Also test empty PATH entry
  const env3 = { ...process.env, PATH: `${path.delimiter}${env.PATH}` };
  const child3 = cp.spawn(command, { env: env3 });
  child3.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ENOENT');
  }));
}

// Case: No PATH in envPairs
const env4 = { ...process.env };
delete env4.PATH;
const child4 = cp.spawn(command, { env: env4 });
child4.on('error', common.mustCall((err) => {
  assert.strictEqual(err.code, 'ENOENT');
}));

// Case: envPath ||= process.env.PATH (no env passed)
if (!common.isWindows) {
  const oldPath = process.env.PATH;
  process.env.PATH = `${noPermDir}${path.delimiter}${oldPath}`;
  try {
    const child5 = cp.spawn(command);
    child5.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ENOENT');
    }));
  } finally {
    process.env.PATH = oldPath;
  }
}
