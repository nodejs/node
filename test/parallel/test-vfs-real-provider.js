// Flags: --experimental-vfs
'use strict';

// Synchronous API for RealFSProvider: construction, basic file ops,
// stats, directories, copy, realpath. Async/promises live in
// test-vfs-real-provider-promises.js, file-handle ops live in
// test-vfs-real-provider-handle.js, and symlinks/path-escape live in
// test-vfs-real-provider-symlinks.js.

require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

tmpdir.refresh();

const testDir = path.join(tmpdir.path, 'vfs-real-provider');
fs.mkdirSync(testDir, { recursive: true });

// Capability flags + construction
{
  const provider = new vfs.RealFSProvider(testDir);
  assert.ok(provider);
  assert.strictEqual(provider.rootPath, testDir);
  assert.strictEqual(provider.readonly, false);
  assert.strictEqual(provider.supportsSymlinks, true);
  assert.strictEqual(provider.supportsWatch, true);
}

// Invalid rootPath
{
  assert.throws(() => new vfs.RealFSProvider(123),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => new vfs.RealFSProvider({}),
                { code: 'ERR_INVALID_ARG_TYPE' });
}

// vfs.create(provider) wires it up
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));
  assert.ok(realVfs);
  assert.strictEqual(realVfs.readonly, false);
}

// readFile / writeFile sync
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));
  realVfs.writeFileSync('/hello.txt', 'Hello from VFS!');
  const realPath = path.join(testDir, 'hello.txt');
  assert.strictEqual(fs.existsSync(realPath), true);
  assert.strictEqual(fs.readFileSync(realPath, 'utf8'), 'Hello from VFS!');
  assert.strictEqual(realVfs.readFileSync('/hello.txt', 'utf8'), 'Hello from VFS!');
  fs.unlinkSync(realPath);
}

// statSync / lstatSync
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));
  fs.writeFileSync(path.join(testDir, 'stat-test.txt'), 'content');
  fs.mkdirSync(path.join(testDir, 'stat-dir'), { recursive: true });

  assert.strictEqual(realVfs.statSync('/stat-test.txt').isFile(), true);
  assert.strictEqual(realVfs.statSync('/stat-dir').isDirectory(), true);
  assert.strictEqual(realVfs.lstatSync('/stat-test.txt').isFile(), true);

  assert.throws(() => realVfs.statSync('/nonexistent'),
                { code: 'ENOENT' });

  fs.unlinkSync(path.join(testDir, 'stat-test.txt'));
  fs.rmdirSync(path.join(testDir, 'stat-dir'));
}

// readdirSync
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));
  fs.mkdirSync(path.join(testDir, 'readdir-test', 'subdir'), { recursive: true });
  fs.writeFileSync(path.join(testDir, 'readdir-test', 'a.txt'), 'a');
  fs.writeFileSync(path.join(testDir, 'readdir-test', 'b.txt'), 'b');

  const entries = realVfs.readdirSync('/readdir-test');
  assert.deepStrictEqual(entries.sort(), ['a.txt', 'b.txt', 'subdir']);

  const dirents = realVfs.readdirSync('/readdir-test', { withFileTypes: true });
  assert.strictEqual(dirents.length, 3);
  assert.ok(dirents.some((d) => d.name === 'a.txt' && d.isFile()));
  assert.ok(dirents.some((d) => d.name === 'subdir' && d.isDirectory()));

  fs.unlinkSync(path.join(testDir, 'readdir-test', 'a.txt'));
  fs.unlinkSync(path.join(testDir, 'readdir-test', 'b.txt'));
  fs.rmdirSync(path.join(testDir, 'readdir-test', 'subdir'));
  fs.rmdirSync(path.join(testDir, 'readdir-test'));
}

// mkdir / rmdir / recursive mkdir
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));
  realVfs.mkdirSync('/new-dir');
  assert.strictEqual(fs.existsSync(path.join(testDir, 'new-dir')), true);
  realVfs.rmdirSync('/new-dir');
  assert.strictEqual(fs.existsSync(path.join(testDir, 'new-dir')), false);

  realVfs.mkdirSync('/deep/nested/dir', { recursive: true });
  assert.strictEqual(fs.existsSync(path.join(testDir, 'deep/nested/dir')), true);
  fs.rmdirSync(path.join(testDir, 'deep/nested/dir'));
  fs.rmdirSync(path.join(testDir, 'deep/nested'));
  fs.rmdirSync(path.join(testDir, 'deep'));
}

// unlink
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));
  fs.writeFileSync(path.join(testDir, 'to-delete.txt'), 'delete me');
  assert.strictEqual(realVfs.existsSync('/to-delete.txt'), true);
  realVfs.unlinkSync('/to-delete.txt');
  assert.strictEqual(fs.existsSync(path.join(testDir, 'to-delete.txt')), false);
}

// rename
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));
  fs.writeFileSync(path.join(testDir, 'old-name.txt'), 'rename me');
  realVfs.renameSync('/old-name.txt', '/new-name.txt');
  assert.strictEqual(fs.existsSync(path.join(testDir, 'old-name.txt')), false);
  assert.strictEqual(fs.readFileSync(path.join(testDir, 'new-name.txt'), 'utf8'),
                     'rename me');
  fs.unlinkSync(path.join(testDir, 'new-name.txt'));
}

// copyFile
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));
  fs.writeFileSync(path.join(testDir, 'source.txt'), 'copy me');
  realVfs.copyFileSync('/source.txt', '/dest.txt');
  assert.strictEqual(fs.readFileSync(path.join(testDir, 'dest.txt'), 'utf8'),
                     'copy me');
  fs.unlinkSync(path.join(testDir, 'source.txt'));
  fs.unlinkSync(path.join(testDir, 'dest.txt'));
}

// realpathSync (non-symlink)
{
  const realVfs = vfs.create(new vfs.RealFSProvider(testDir));
  fs.writeFileSync(path.join(testDir, 'real.txt'), 'content');
  assert.strictEqual(realVfs.realpathSync('/real.txt'), '/real.txt');
  fs.unlinkSync(path.join(testDir, 'real.txt'));
}
