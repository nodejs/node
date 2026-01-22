'use strict';

require('../common');
const assert = require('assert');
const fs = require('fs');

// Test that virtualCwd option is disabled by default
{
  const vfs = fs.createVirtual();
  assert.strictEqual(vfs.virtualCwdEnabled, false);

  // Should throw when trying to use cwd() without enabling
  assert.throws(() => {
    vfs.cwd();
  }, { code: 'ERR_INVALID_STATE' });

  // Should throw when trying to use chdir() without enabling
  assert.throws(() => {
    vfs.chdir('/');
  }, { code: 'ERR_INVALID_STATE' });
}

// Test that virtualCwd option can be enabled
{
  const vfs = fs.createVirtual({ virtualCwd: true });
  assert.strictEqual(vfs.virtualCwdEnabled, true);

  // Initial cwd should be null
  assert.strictEqual(vfs.cwd(), null);
}

// Test basic chdir functionality
{
  const vfs = fs.createVirtual({ virtualCwd: true });
  vfs.addDirectory('/project');
  vfs.addDirectory('/project/src');
  vfs.addFile('/project/src/index.js', 'module.exports = "hello";');
  vfs.mount('/virtual');

  // Change to a directory that exists
  vfs.chdir('/virtual/project');
  assert.strictEqual(vfs.cwd(), '/virtual/project');

  // Change to a subdirectory
  vfs.chdir('/virtual/project/src');
  assert.strictEqual(vfs.cwd(), '/virtual/project/src');

  vfs.unmount();
}

// Test chdir with non-existent path throws ENOENT
{
  const vfs = fs.createVirtual({ virtualCwd: true });
  vfs.addDirectory('/project');
  vfs.mount('/virtual');

  assert.throws(() => {
    vfs.chdir('/virtual/nonexistent');
  }, { code: 'ENOENT' });

  vfs.unmount();
}

// Test chdir with file path throws ENOTDIR
{
  const vfs = fs.createVirtual({ virtualCwd: true });
  vfs.addFile('/file.txt', 'content');
  vfs.mount('/virtual');

  assert.throws(() => {
    vfs.chdir('/virtual/file.txt');
  }, { code: 'ENOTDIR' });

  vfs.unmount();
}

// Test resolvePath with virtual cwd
{
  const vfs = fs.createVirtual({ virtualCwd: true });
  vfs.addDirectory('/project');
  vfs.addDirectory('/project/src');
  vfs.addFile('/project/src/index.js', 'module.exports = "hello";');
  vfs.mount('/virtual');

  // Before setting cwd, relative paths use real cwd
  const resolvedBefore = vfs.resolvePath('test.js');
  assert.ok(resolvedBefore.endsWith('test.js'));

  // Set virtual cwd
  vfs.chdir('/virtual/project');

  // Absolute paths are returned as-is
  assert.strictEqual(vfs.resolvePath('/absolute/path'), '/absolute/path');

  // Relative paths are resolved relative to virtual cwd
  assert.strictEqual(vfs.resolvePath('src/index.js'), '/virtual/project/src/index.js');
  assert.strictEqual(vfs.resolvePath('./src/index.js'), '/virtual/project/src/index.js');

  // Change to subdirectory and resolve again
  vfs.chdir('/virtual/project/src');
  assert.strictEqual(vfs.resolvePath('index.js'), '/virtual/project/src/index.js');

  vfs.unmount();
}

// Test resolvePath without virtual cwd enabled
{
  const vfs = fs.createVirtual({ virtualCwd: false });
  vfs.mount('/virtual');

  // Should still work, but uses real cwd for relative paths
  const resolved = vfs.resolvePath('/absolute/path');
  assert.strictEqual(resolved, '/absolute/path');

  vfs.unmount();
}

// Test chdir in overlay mode
{
  const vfs = fs.createVirtual({ virtualCwd: true });
  vfs.addDirectory('/project');
  vfs.addDirectory('/project/lib');
  vfs.overlay();

  vfs.chdir('/project');
  assert.strictEqual(vfs.cwd(), '/project');

  vfs.chdir('/project/lib');
  assert.strictEqual(vfs.cwd(), '/project/lib');

  vfs.unmount();
}

// Test process.chdir() interception
{
  const vfs = fs.createVirtual({ virtualCwd: true });
  vfs.addDirectory('/project');
  vfs.addDirectory('/project/src');
  vfs.mount('/virtual');

  const originalCwd = process.cwd();

  // process.chdir to VFS path
  process.chdir('/virtual/project');
  assert.strictEqual(process.cwd(), '/virtual/project');
  assert.strictEqual(vfs.cwd(), '/virtual/project');

  // process.chdir to another VFS path
  process.chdir('/virtual/project/src');
  assert.strictEqual(process.cwd(), '/virtual/project/src');
  assert.strictEqual(vfs.cwd(), '/virtual/project/src');

  vfs.unmount();

  // After unmount, process.cwd should return original cwd
  assert.strictEqual(process.cwd(), originalCwd);
}

// Test process.chdir() to real path falls through
{
  const vfs = fs.createVirtual({ virtualCwd: true });
  vfs.addDirectory('/project');
  vfs.mount('/virtual');

  const originalCwd = process.cwd();

  // Change to a real directory (not under /virtual)
  process.chdir('/tmp');
  assert.strictEqual(process.cwd(), '/tmp');
  // vfs.cwd() should still be null (not set)
  assert.strictEqual(vfs.cwd(), null);

  // Change back to original
  process.chdir(originalCwd);

  vfs.unmount();
}

// Test process.cwd() returns virtual cwd when set
{
  const vfs = fs.createVirtual({ virtualCwd: true });
  vfs.addDirectory('/project');
  vfs.mount('/virtual');

  const originalCwd = process.cwd();

  // Before chdir, process.cwd returns real cwd
  assert.strictEqual(process.cwd(), originalCwd);

  // Set virtual cwd
  vfs.chdir('/virtual/project');
  assert.strictEqual(process.cwd(), '/virtual/project');

  vfs.unmount();

  // After unmount, returns real cwd
  assert.strictEqual(process.cwd(), originalCwd);
}

// Test that process.chdir/cwd are not hooked when virtualCwd is disabled
{
  const originalChdir = process.chdir;
  const originalCwd = process.cwd;

  const vfs = fs.createVirtual({ virtualCwd: false });
  vfs.addDirectory('/project');
  vfs.mount('/virtual');

  // process.chdir and process.cwd should not be modified
  assert.strictEqual(process.chdir, originalChdir);
  assert.strictEqual(process.cwd, originalCwd);

  vfs.unmount();
}

// Test virtual cwd is reset on unmount
{
  const vfs = fs.createVirtual({ virtualCwd: true });
  vfs.addDirectory('/project');
  vfs.mount('/virtual');

  vfs.chdir('/virtual/project');
  assert.strictEqual(vfs.cwd(), '/virtual/project');

  vfs.unmount();

  // After unmount, cwd should throw (not enabled)
  // Actually, virtualCwdEnabled is still true, just unmounted
  // Let's remount and check cwd is reset
  vfs.mount('/virtual');
  assert.strictEqual(vfs.cwd(), null);

  vfs.unmount();
}
