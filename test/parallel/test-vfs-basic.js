// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// Test that VirtualFileSystem can be created via vfs.create()
{
  const myVfs = vfs.create();
  assert.ok(myVfs);
  assert.strictEqual(typeof myVfs.writeFileSync, 'function');
  assert.strictEqual(myVfs.mounted, false);
}

// Test adding and reading a static file
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/test', { recursive: true });
  myVfs.writeFileSync('/test/file.txt', 'hello world');

  assert.strictEqual(myVfs.existsSync('/test/file.txt'), true);
  assert.strictEqual(myVfs.existsSync('/test'), true);
  assert.strictEqual(myVfs.existsSync('/nonexistent'), false);

  const content = myVfs.readFileSync('/test/file.txt');
  assert.ok(Buffer.isBuffer(content));
  assert.strictEqual(content.toString(), 'hello world');

  // Read with encoding
  const textContent = myVfs.readFileSync('/test/file.txt', 'utf8');
  assert.strictEqual(textContent, 'hello world');
}

// Test statSync
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/test/dir', { recursive: true });
  myVfs.writeFileSync('/test/file.txt', 'content');

  const fileStat = myVfs.statSync('/test/file.txt');
  assert.strictEqual(fileStat.isFile(), true);
  assert.strictEqual(fileStat.isDirectory(), false);
  assert.strictEqual(fileStat.size, 7); // 'content'.length

  const dirStat = myVfs.statSync('/test/dir');
  assert.strictEqual(dirStat.isFile(), false);
  assert.strictEqual(dirStat.isDirectory(), true);

  // Test ENOENT
  assert.throws(() => {
    myVfs.statSync('/nonexistent');
  }, { code: 'ENOENT' });
}

// Test readdirSync
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir/subdir', { recursive: true });
  myVfs.writeFileSync('/dir/a.txt', 'a');
  myVfs.writeFileSync('/dir/b.txt', 'b');

  const entries = myVfs.readdirSync('/dir');
  assert.deepStrictEqual(entries.sort(), ['a.txt', 'b.txt', 'subdir']);

  // Test with file types
  const dirents = myVfs.readdirSync('/dir', { withFileTypes: true });
  assert.strictEqual(dirents.length, 3);

  const fileEntry = dirents.find((d) => d.name === 'a.txt');
  assert.ok(fileEntry);
  assert.strictEqual(fileEntry.isFile(), true);
  assert.strictEqual(fileEntry.isDirectory(), false);

  const dirEntry = dirents.find((d) => d.name === 'subdir');
  assert.ok(dirEntry);
  assert.strictEqual(dirEntry.isFile(), false);
  assert.strictEqual(dirEntry.isDirectory(), true);

  // Test ENOTDIR
  assert.throws(() => {
    myVfs.readdirSync('/dir/a.txt');
  }, { code: 'ENOTDIR' });

  // Test ENOENT
  assert.throws(() => {
    myVfs.readdirSync('/nonexistent');
  }, { code: 'ENOENT' });
}

// Test removing entries
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/test', { recursive: true });
  myVfs.writeFileSync('/test/file.txt', 'content');

  assert.strictEqual(myVfs.existsSync('/test/file.txt'), true);
  myVfs.unlinkSync('/test/file.txt');
  assert.strictEqual(myVfs.existsSync('/test/file.txt'), false);

  // Unlinking non-existent entry throws ENOENT
  assert.throws(() => {
    myVfs.unlinkSync('/nonexistent');
  }, { code: 'ENOENT' });
}

// Test mount mode
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/data', { recursive: true });
  myVfs.writeFileSync('/data/file.txt', 'mounted content');

  assert.strictEqual(myVfs.mounted, false);
  myVfs.mount('/app/virtual');
  assert.strictEqual(myVfs.mounted, true);
  assert.strictEqual(myVfs.mountPoint, '/app/virtual');

  // With mount, shouldHandle should work
  assert.strictEqual(myVfs.shouldHandle('/app/virtual/data/file.txt'), true);
  assert.strictEqual(myVfs.shouldHandle('/other/path'), false);

  myVfs.unmount();
  assert.strictEqual(myVfs.mounted, false);
}

// Test internalModuleStat (used by Module._stat)
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dir', { recursive: true });
  myVfs.writeFileSync('/module.js', 'module.exports = {}');

  assert.strictEqual(myVfs.internalModuleStat('/module.js'), 0); // file
  assert.strictEqual(myVfs.internalModuleStat('/dir'), 1); // directory
  assert.strictEqual(myVfs.internalModuleStat('/nonexistent'), -2); // ENOENT
}

// Test reading directory as file throws EISDIR
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/mydir', { recursive: true });

  assert.throws(() => {
    myVfs.readFileSync('/mydir');
  }, { code: 'EISDIR' });
}

// Test realpathSync
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/test', { recursive: true });
  myVfs.writeFileSync('/test/file.txt', 'content');

  const realpath = myVfs.realpathSync('/test/file.txt');
  assert.strictEqual(realpath, '/test/file.txt');

  // Non-existent path throws
  assert.throws(() => {
    myVfs.realpathSync('/nonexistent');
  }, { code: 'ENOENT' });
}

// Test rmdir on non-empty directory throws ENOTEMPTY
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/parent', { recursive: true });
  myVfs.writeFileSync('/parent/file.txt', 'content');

  assert.throws(() => {
    myVfs.rmdirSync('/parent');
  }, { code: 'ENOTEMPTY' });

  // After removing the file, rmdir should succeed
  myVfs.unlinkSync('/parent/file.txt');
  myVfs.rmdirSync('/parent');
  assert.strictEqual(myVfs.existsSync('/parent'), false);
}

// Test mkdir on existing directory throws EEXIST (without recursive)
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/existing', { recursive: true });

  assert.throws(() => {
    myVfs.mkdirSync('/existing');
  }, { code: 'EEXIST' });

  // With recursive: true, it should not throw
  myVfs.mkdirSync('/existing', { recursive: true });
  assert.strictEqual(myVfs.existsSync('/existing'), true);
}

// Test mounting at root '/' (exercises joinMountPath with '/' relativePath)
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/app/data', { recursive: true });
  myVfs.writeFileSync('/app/data/config.json', '{}');

  // Mount at root
  myVfs.mount('/');
  assert.strictEqual(myVfs.mountPoint, '/');

  // Access the mount point root itself (relativePath = '/')
  assert.strictEqual(myVfs.shouldHandle('/'), true);
  assert.strictEqual(myVfs.shouldHandle('/app'), true);
  assert.strictEqual(myVfs.shouldHandle('/app/data/config.json'), true);

  myVfs.unmount();
}

// Test deeply nested paths (exercises getParentPath and getBaseName internally)
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/a/b/c/d/e', { recursive: true });
  myVfs.writeFileSync('/a/b/c/d/e/file.txt', 'deep');

  // Read the deeply nested file
  assert.strictEqual(myVfs.readFileSync('/a/b/c/d/e/file.txt', 'utf8'), 'deep');

  // Readdir at various levels
  assert.ok(myVfs.readdirSync('/a').includes('b'));
  assert.ok(myVfs.readdirSync('/a/b').includes('c'));
  assert.ok(myVfs.readdirSync('/a/b/c/d/e').includes('file.txt'));

  // Stat files at root level
  myVfs.writeFileSync('/root-file.txt', 'root');
  const stat = myVfs.statSync('/root-file.txt');
  assert.strictEqual(stat.isFile(), true);
}

// Test truncateSync on file handle
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/truncate.txt', 'hello world');

  // Open file and truncate
  const fd = myVfs.openSync('/truncate.txt', 'r+');
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  // Truncate to smaller size
  handle.entry.truncateSync(5);
  myVfs.closeSync(fd);

  assert.strictEqual(myVfs.readFileSync('/truncate.txt', 'utf8'), 'hello');
}

// Test truncateSync extending file
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/extend.txt', 'hi');

  const fd = myVfs.openSync('/extend.txt', 'r+');
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  // Truncate to larger size (extends with null bytes)
  handle.entry.truncateSync(10);
  myVfs.closeSync(fd);

  const content = myVfs.readFileSync('/extend.txt');
  assert.strictEqual(content.length, 10);
  assert.strictEqual(content.slice(0, 2).toString(), 'hi');
}

// Test async truncate
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/async-truncate.txt', 'async content');

  const fd = myVfs.openSync('/async-truncate.txt', 'r+');
  const handle = require('internal/vfs/fd').getVirtualFd(fd);

  handle.entry.truncate(5).then(common.mustCall(() => {
    myVfs.closeSync(fd);
    assert.strictEqual(myVfs.readFileSync('/async-truncate.txt', 'utf8'), 'async');
  }));
}

// Test rename operation
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/old-name.txt', 'content');

  myVfs.renameSync('/old-name.txt', '/new-name.txt');

  assert.strictEqual(myVfs.existsSync('/old-name.txt'), false);
  assert.strictEqual(myVfs.existsSync('/new-name.txt'), true);
  assert.strictEqual(myVfs.readFileSync('/new-name.txt', 'utf8'), 'content');
}

// Test copyFile operation
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/source.txt', 'copy me');

  myVfs.copyFileSync('/source.txt', '/dest.txt');

  assert.strictEqual(myVfs.existsSync('/source.txt'), true);
  assert.strictEqual(myVfs.existsSync('/dest.txt'), true);
  assert.strictEqual(myVfs.readFileSync('/dest.txt', 'utf8'), 'copy me');
}
