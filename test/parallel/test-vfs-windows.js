'use strict';

const common = require('../common');

if (!common.isWindows) {
  common.skip('Windows-specific test');
}

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

// Test mounting with Windows drive letter path
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/data.txt', 'Hello from VFS');
  myVfs.mkdirSync('/subdir');
  myVfs.writeFileSync('/subdir/nested.txt', 'Nested content');

  // Mount at a Windows path with drive letter
  const mountPath = path.join(process.env.TEMP || 'C:\\temp', 'vfs-test-mount');
  myVfs.mount(mountPath);

  assert.strictEqual(myVfs.mounted, true);
  assert.strictEqual(myVfs.mountPoint, mountPath);

  // Read file through fs module using Windows path
  const content = fs.readFileSync(path.join(mountPath, 'data.txt'), 'utf8');
  assert.strictEqual(content, 'Hello from VFS');

  // Read nested file
  const nestedContent = fs.readFileSync(path.join(mountPath, 'subdir', 'nested.txt'), 'utf8');
  assert.strictEqual(nestedContent, 'Nested content');

  // Stat operations
  const stat = fs.statSync(path.join(mountPath, 'data.txt'));
  assert.strictEqual(stat.isFile(), true);

  const dirStat = fs.statSync(path.join(mountPath, 'subdir'));
  assert.strictEqual(dirStat.isDirectory(), true);

  // Readdir
  const entries = fs.readdirSync(mountPath);
  assert.deepStrictEqual(entries.sort(), ['data.txt', 'subdir']);

  myVfs.unmount();
  assert.strictEqual(myVfs.mounted, false);
}

// Test mounting at drive root (e.g., C:\virtual)
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/test.txt', 'Drive root test');

  // Use current drive letter
  const driveLetter = process.cwd().slice(0, 2); // e.g., "C:"
  const mountPath = driveLetter + '\\vfs-test-root';
  myVfs.mount(mountPath);

  assert.strictEqual(myVfs.mounted, true);

  const content = fs.readFileSync(mountPath + '\\test.txt', 'utf8');
  assert.strictEqual(content, 'Drive root test');

  myVfs.unmount();
}

// Test that mountPoint returns Windows-style absolute path
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'content');

  const mountPath = 'C:\\virtual';
  myVfs.mount(mountPath);

  // mountPoint should be an absolute Windows path
  assert.ok(myVfs.mountPoint.includes(':'), 'mountPoint should contain drive letter');
  assert.ok(path.isAbsolute(myVfs.mountPoint), 'mountPoint should be absolute');

  myVfs.unmount();
}

// Test require from Windows VFS path
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/module.js', 'module.exports = { value: 42 };');

  const mountPath = path.join(process.env.TEMP || 'C:\\temp', 'vfs-require-test');
  myVfs.mount(mountPath);

  const modulePath = path.join(mountPath, 'module.js');
  const mod = require(modulePath);
  assert.strictEqual(mod.value, 42);

  myVfs.unmount();

  // Clean up require cache
  delete require.cache[modulePath];
}
