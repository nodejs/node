'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { execSync } = require('child_process');

tmpdir.refresh(); // Prepare a clean temporary directory

function countOccurrences(haystack, needle) {
  let count = 0;
  let offset = 0;

  while ((offset = haystack.indexOf(needle, offset)) !== -1) {
    count++;
    offset += needle.length;
  }

  return count;
}

const dirPath = path.join(tmpdir.path, '速_dir');
const filePath = path.join(dirPath, 'test_file.txt');

// Create a directory and a file within it
fs.mkdirSync(dirPath, { recursive: true });
fs.writeFileSync(filePath, 'This is a test file.');

// Set permissions to simulate a permission denied scenario
if (process.platform === 'win32') {
  // Windows: Deny delete permissions
  execSync(`icacls "${filePath}" /deny Everyone:(D)`);
} else {
  // Unix/Linux: Remove write permissions from the directory
  fs.chmodSync(dirPath, 0o555); // Read and execute permissions only
}

try {
  // Attempt to delete the directory which should now fail
  assert.throws(() => {
    fs.rmSync(dirPath, { recursive: true });
  }, (err) => {
    // Verify that the error is due to permission restrictions
    let expectedCode = 'EACCES';
    if (common.isMacOS) {
      expectedCode = 'ENOTEMPTY';
    } else if (common.isWindows) {
      expectedCode = 'EPERM';
    }
    assert.strictEqual(err.code, expectedCode);
    assert.strictEqual(err.syscall, 'rm');
    assert.strictEqual(err.path, dirPath);
    assert.strictEqual(countOccurrences(err.message, dirPath), 1);
    return true;
  });
} finally {
  // Cleanup - resetting permissions and removing the directory safely
  if (process.platform === 'win32') {
    // Remove the explicit permissions before attempting to delete
    execSync(`icacls "${filePath}" /remove:d Everyone`);
  } else {
    // Reset permissions to allow deletion
    fs.chmodSync(dirPath, 0o755); // Restore full permissions to the directory
  }

  // Attempt to clean up
  fs.rmSync(dirPath, { recursive: true }); // This should now succeed
}
