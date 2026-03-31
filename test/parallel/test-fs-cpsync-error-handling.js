'use strict';

// Test that fs.cpSync properly handles directory iteration errors
// instead of causing process abort.
//
// This test ensures that when directory_iterator construction fails
// (e.g., due to permission issues or malformed paths), the error
// is properly converted to a JavaScript exception rather than
// triggering std::terminate or __libcpp_verbose_abort.

const common = require('../common');
const { cpSync, mkdirSync, writeFileSync } = require('fs');
const { join } = require('path');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

// Test 1: Copying from non-existent directory should throw, not abort
{
  const nonExistent = join(tmpdir.path, 'does-not-exist');
  const dest = join(tmpdir.path, 'dest1');
  
  assert.throws(
    () => cpSync(nonExistent, dest, { recursive: true }),
    {
      code: 'ENOENT',
      message: /ENOENT/
    },
    'cpSync should throw ENOENT for non-existent source directory'
  );
}

// Test 2: Copying directory with unreadable subdirectory
// (This test is platform-specific and may be skipped on Windows)
if (!common.isWindows) {
  const srcDir = join(tmpdir.path, 'src-unreadable');
  const unreadableSubdir = join(srcDir, 'unreadable');
  const dest = join(tmpdir.path, 'dest2');
  
  mkdirSync(srcDir);
  mkdirSync(unreadableSubdir);
  writeFileSync(join(unreadableSubdir, 'file.txt'), 'content');
  
  try {
    require('fs').chmodSync(unreadableSubdir, 0o000);
    
    // Should throw error, not abort
    assert.throws(
      () => cpSync(srcDir, dest, { recursive: true }),
      {
        code: 'EACCES',
      },
      'cpSync should throw EACCES for unreadable directory'
    );
  } finally {
    // Restore permissions for cleanup
    require('fs').chmodSync(unreadableSubdir, 0o755);
  }
}

// Test 3: Basic successful copy to ensure fix doesn't break normal operation
{
  const srcDir = join(tmpdir.path, 'src-normal');
  const dest = join(tmpdir.path, 'dest-normal');
  
  mkdirSync(srcDir);
  writeFileSync(join(srcDir, 'file1.txt'), 'content1');
  mkdirSync(join(srcDir, 'subdir'));
  writeFileSync(join(srcDir, 'subdir', 'file2.txt'), 'content2');
  
  // Should not throw
  cpSync(srcDir, dest, { recursive: true });
  
  const fs = require('fs');
  assert.ok(fs.existsSync(join(dest, 'file1.txt')));
  assert.ok(fs.existsSync(join(dest, 'subdir', 'file2.txt')));
  assert.strictEqual(
    fs.readFileSync(join(dest, 'file1.txt'), 'utf8'),
    'content1'
  );
  assert.strictEqual(
    fs.readFileSync(join(dest, 'subdir', 'file2.txt'), 'utf8'),
    'content2'
  );
}
