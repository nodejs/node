'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

if (common.isSunOS || common.isWindows || common.isAIX || common.isIBMi) {
  // The current working directory cannot be removed on these platforms.
  common.skip('cannot rmdir current working directory');
}

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Create a temporary directory
const testDir = path.join(tmpdir.path, 'test-cwd-deleted');
fs.mkdirSync(testDir);

// Save original cwd
const originalCwd = process.cwd();

try {
  // Change to the test directory
  process.chdir(testDir);
  
  // Delete the directory while we're in it
  fs.rmdirSync(testDir);
  
  // Verify that process.cwd() throws with improved error message
  assert.throws(
    () => process.cwd(),
    {
      code: 'ENOENT',
      message: /process\.cwd\(\) failed: current working directory no longer exists/
    }
  );
} finally {
  // Restore original cwd for cleanup
  try {
    process.chdir(originalCwd);
  } catch {
    // Ignore errors if we can't change back
  }
}
