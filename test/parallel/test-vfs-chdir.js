'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

// Test that virtualCwd option is disabled by default
{
  const myVfs = vfs.create();
  assert.strictEqual(myVfs.virtualCwdEnabled, false);

  // Should throw when trying to use cwd() without enabling
  assert.throws(() => {
    myVfs.cwd();
  }, { code: 'ERR_INVALID_STATE' });

  // Should throw when trying to use chdir() without enabling
  assert.throws(() => {
    myVfs.chdir('/');
  }, { code: 'ERR_INVALID_STATE' });
}

// Test that virtualCwd option can be enabled
{
  const myVfs = vfs.create({ virtualCwd: true });
  assert.strictEqual(myVfs.virtualCwdEnabled, true);

  // Initial cwd should be null
  assert.strictEqual(myVfs.cwd(), null);
}

// Test basic chdir functionality
{
  const myVfs = vfs.create({ virtualCwd: true });
  myVfs.mkdirSync('/project/src', { recursive: true });
  myVfs.writeFileSync('/project/src/index.js', 'module.exports = "hello";');
  myVfs.mount('/virtual');

  // Change to a directory that exists
  myVfs.chdir('/virtual/project');
  assert.strictEqual(myVfs.cwd(), '/virtual/project');

  // Change to a subdirectory
  myVfs.chdir('/virtual/project/src');
  assert.strictEqual(myVfs.cwd(), '/virtual/project/src');

  myVfs.unmount();
}

// Test chdir with non-existent path throws ENOENT
{
  const myVfs = vfs.create({ virtualCwd: true });
  myVfs.mkdirSync('/project', { recursive: true });
  myVfs.mount('/virtual');

  assert.throws(() => {
    myVfs.chdir('/virtual/nonexistent');
  }, { code: 'ENOENT' });

  myVfs.unmount();
}

// Test chdir with file path throws ENOTDIR
{
  const myVfs = vfs.create({ virtualCwd: true });
  myVfs.writeFileSync('/file.txt', 'content');
  myVfs.mount('/virtual');

  assert.throws(() => {
    myVfs.chdir('/virtual/file.txt');
  }, { code: 'ENOTDIR' });

  myVfs.unmount();
}

// Test resolvePath with virtual cwd
{
  const myVfs = vfs.create({ virtualCwd: true });
  myVfs.mkdirSync('/project/src', { recursive: true });
  myVfs.writeFileSync('/project/src/index.js', 'module.exports = "hello";');
  myVfs.mount('/virtual');

  // Before setting cwd, relative paths use real cwd
  const resolvedBefore = myVfs.resolvePath('test.js');
  assert.ok(resolvedBefore.endsWith('test.js'));

  // Set virtual cwd
  myVfs.chdir('/virtual/project');

  // Absolute paths are returned as-is
  assert.strictEqual(myVfs.resolvePath('/absolute/path'), '/absolute/path');

  // Relative paths are resolved relative to virtual cwd
  assert.strictEqual(myVfs.resolvePath('src/index.js'), '/virtual/project/src/index.js');
  assert.strictEqual(myVfs.resolvePath('./src/index.js'), '/virtual/project/src/index.js');

  // Change to subdirectory and resolve again
  myVfs.chdir('/virtual/project/src');
  assert.strictEqual(myVfs.resolvePath('index.js'), '/virtual/project/src/index.js');

  myVfs.unmount();
}

// Test resolvePath without virtual cwd enabled
{
  const myVfs = vfs.create({ virtualCwd: false });
  myVfs.mount('/virtual');

  // Should still work, but uses real cwd for relative paths
  const resolved = myVfs.resolvePath('/absolute/path');
  assert.strictEqual(resolved, '/absolute/path');

  myVfs.unmount();
}

// Test process.chdir() interception
{
  const myVfs = vfs.create({ virtualCwd: true });
  myVfs.mkdirSync('/project/src', { recursive: true });
  myVfs.mount('/virtual');

  const originalCwd = process.cwd();

  // process.chdir to VFS path
  process.chdir('/virtual/project');
  assert.strictEqual(process.cwd(), '/virtual/project');
  assert.strictEqual(myVfs.cwd(), '/virtual/project');

  // process.chdir to another VFS path
  process.chdir('/virtual/project/src');
  assert.strictEqual(process.cwd(), '/virtual/project/src');
  assert.strictEqual(myVfs.cwd(), '/virtual/project/src');

  myVfs.unmount();

  // After unmount, process.cwd should return original cwd
  assert.strictEqual(process.cwd(), originalCwd);
}

// Test process.chdir() to real path falls through
{
  const myVfs = vfs.create({ virtualCwd: true });
  myVfs.mkdirSync('/project', { recursive: true });
  myVfs.mount('/virtual');

  const originalCwd = process.cwd();

  // Change to a real directory (not under /virtual)
  // Use realpathSync because /tmp may be a symlink (e.g., /tmp -> /private/tmp on macOS)
  const tmpDir = fs.realpathSync('/tmp');
  process.chdir('/tmp');
  assert.strictEqual(process.cwd(), tmpDir);
  // myVfs.cwd() should still be null (not set)
  assert.strictEqual(myVfs.cwd(), null);

  // Change back to original
  process.chdir(originalCwd);

  myVfs.unmount();
}

// Test process.cwd() returns virtual cwd when set
{
  const myVfs = vfs.create({ virtualCwd: true });
  myVfs.mkdirSync('/project', { recursive: true });
  myVfs.mount('/virtual');

  const originalCwd = process.cwd();

  // Set virtual cwd
  myVfs.chdir('/virtual/project');
  assert.strictEqual(process.cwd(), '/virtual/project');

  myVfs.unmount();

  // After unmount, returns real cwd
  assert.strictEqual(process.cwd(), originalCwd);
}

// Test that process.chdir/cwd are not hooked when virtualCwd is disabled
{
  const originalChdir = process.chdir;
  const originalCwd = process.cwd;

  const myVfs = vfs.create({ virtualCwd: false });
  myVfs.mkdirSync('/project', { recursive: true });
  myVfs.mount('/virtual');

  // process.chdir and process.cwd should not be modified
  assert.strictEqual(process.chdir, originalChdir);
  assert.strictEqual(process.cwd, originalCwd);

  myVfs.unmount();
}

// Test virtual cwd is reset on unmount
{
  const myVfs = vfs.create({ virtualCwd: true });
  myVfs.mkdirSync('/project', { recursive: true });
  myVfs.mount('/virtual');

  myVfs.chdir('/virtual/project');
  assert.strictEqual(myVfs.cwd(), '/virtual/project');

  myVfs.unmount();

  // After remount, virtual cwd should be reset to null
  myVfs.mount('/virtual');
  assert.strictEqual(myVfs.cwd(), null);

  myVfs.unmount();
}
