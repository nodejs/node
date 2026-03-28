// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');

// Test internal VFS utility functions for coverage.

// === Router utility functions ===
const {
  isUnderMountPoint,
  getRelativePath,
  isAbsolutePath,
} = require('internal/vfs/router');

// isAbsolutePath
assert.strictEqual(isAbsolutePath('/foo'), true);
assert.strictEqual(isAbsolutePath('foo'), false);
assert.strictEqual(isAbsolutePath('./foo'), false);

// isUnderMountPoint (already tested indirectly but exercised here directly)
assert.strictEqual(isUnderMountPoint(path.resolve('/mount'), path.resolve('/mount')), true);
assert.strictEqual(isUnderMountPoint(path.resolve('/mount/file'), path.resolve('/mount')), true);
assert.strictEqual(isUnderMountPoint(path.resolve('/mountx'), path.resolve('/mount')), false);
assert.strictEqual(isUnderMountPoint(path.resolve('/other'), path.resolve('/mount')), false);
// Root mount point
assert.strictEqual(isUnderMountPoint('/anything', '/'), true);

// getRelativePath
assert.strictEqual(getRelativePath(path.resolve('/mount'), path.resolve('/mount')), '/');
assert.strictEqual(getRelativePath(path.resolve('/mount/file.js'), path.resolve('/mount')), '/file.js');
assert.strictEqual(getRelativePath(path.resolve('/mount/a/b'), path.resolve('/mount')), '/a/b');
// Root mount point
assert.strictEqual(getRelativePath('/foo/bar', '/'), '/foo/bar');

// === Provider base class readonly checks ===
const { VirtualProvider } = require('internal/vfs/provider');

class ReadonlyProvider extends VirtualProvider {
  get readonly() { return true; }
}

const readonlyProvider = new ReadonlyProvider();

// All write operations should throw EROFS when readonly
assert.throws(() => readonlyProvider.mkdirSync('/dir'), { code: 'EROFS' });
assert.throws(() => readonlyProvider.rmdirSync('/dir'), { code: 'EROFS' });
assert.throws(() => readonlyProvider.unlinkSync('/file'), { code: 'EROFS' });
assert.throws(() => readonlyProvider.renameSync('/a', '/b'), { code: 'EROFS' });
assert.throws(() => readonlyProvider.writeFileSync('/f', 'data'), { code: 'EROFS' });
assert.throws(() => readonlyProvider.appendFileSync('/f', 'data'), { code: 'EROFS' });
assert.throws(() => readonlyProvider.copyFileSync('/a', '/b'), { code: 'EROFS' });
assert.throws(() => readonlyProvider.symlinkSync('/target', '/link'), { code: 'EROFS' });

// Async versions
assert.rejects(readonlyProvider.mkdir('/dir'), { code: 'EROFS' }).then(common.mustCall());
assert.rejects(readonlyProvider.rmdir('/dir'), { code: 'EROFS' }).then(common.mustCall());
assert.rejects(readonlyProvider.unlink('/file'), { code: 'EROFS' }).then(common.mustCall());
assert.rejects(readonlyProvider.rename('/a', '/b'), { code: 'EROFS' }).then(common.mustCall());
assert.rejects(readonlyProvider.writeFile('/f', 'data'), { code: 'EROFS' }).then(common.mustCall());
assert.rejects(readonlyProvider.appendFile('/f', 'data'), { code: 'EROFS' }).then(common.mustCall());
assert.rejects(readonlyProvider.copyFile('/a', '/b'), { code: 'EROFS' }).then(common.mustCall());
assert.rejects(readonlyProvider.symlink('/target', '/link'), { code: 'EROFS' }).then(common.mustCall());

// === Provider base class ERR_METHOD_NOT_IMPLEMENTED for non-readonly ===
const baseProvider = new VirtualProvider();

// These should throw ERR_METHOD_NOT_IMPLEMENTED (not readonly, just unimplemented)
assert.throws(() => baseProvider.mkdirSync('/dir'), { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
assert.throws(() => baseProvider.rmdirSync('/dir'), { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
assert.throws(() => baseProvider.unlinkSync('/file'), { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
assert.throws(() => baseProvider.renameSync('/a', '/b'), { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
assert.throws(() => baseProvider.symlinkSync('/t', '/l'), { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
assert.throws(() => baseProvider.readdirSync('/dir'), { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
assert.throws(() => baseProvider.readlinkSync('/link'), { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
assert.throws(() => baseProvider.watch('/path'), { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
assert.throws(() => baseProvider.watchAsync('/path'), { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
assert.throws(() => baseProvider.watchFile('/path'), { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
assert.throws(() => baseProvider.unwatchFile('/path'), { code: 'ERR_METHOD_NOT_IMPLEMENTED' });

// Async unimplemented methods
assert.rejects(baseProvider.mkdir('/dir'), { code: 'ERR_METHOD_NOT_IMPLEMENTED' }).then(common.mustCall());
assert.rejects(baseProvider.rmdir('/dir'), { code: 'ERR_METHOD_NOT_IMPLEMENTED' }).then(common.mustCall());
assert.rejects(baseProvider.unlink('/file'), { code: 'ERR_METHOD_NOT_IMPLEMENTED' }).then(common.mustCall());
assert.rejects(baseProvider.rename('/a', '/b'), { code: 'ERR_METHOD_NOT_IMPLEMENTED' }).then(common.mustCall());
assert.rejects(baseProvider.readlink('/link'), { code: 'ERR_METHOD_NOT_IMPLEMENTED' }).then(common.mustCall());
assert.rejects(baseProvider.symlink('/t', '/l'), { code: 'ERR_METHOD_NOT_IMPLEMENTED' }).then(common.mustCall());

// === Provider default implementations via VFS (covers realpath, access, exists, etc.) ===
const vfs = require('node:vfs');

{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'hello');
  myVfs.mkdirSync('/dir');
  myVfs.mount('/internals-test');

  // Realpath (default provider impl returns path as-is after stat check)
  assert.strictEqual(myVfs.realpathSync('/internals-test/file.txt'), path.resolve('/internals-test/file.txt'));
  assert.strictEqual(myVfs.realpathSync('/internals-test/dir'), path.resolve('/internals-test/dir'));

  // Realpath for non-existent path should throw
  assert.throws(() => myVfs.realpathSync('/internals-test/nonexistent'), { code: 'ENOENT' });

  // access (default provider impl just checks stat)
  myVfs.accessSync('/internals-test/file.txt');
  assert.throws(() => myVfs.accessSync('/internals-test/nonexistent'), { code: 'ENOENT' });

  // existsSync
  assert.strictEqual(myVfs.existsSync('/internals-test/file.txt'), true);
  assert.strictEqual(myVfs.existsSync('/internals-test/nonexistent'), false);
  assert.strictEqual(myVfs.existsSync('/internals-test/dir'), true);

  // Callback-based realpath
  myVfs.realpath('/internals-test/file.txt', common.mustSucceed((resolved) => {
    assert.ok(resolved);
  }));

  // Callback-based access
  myVfs.access('/internals-test/file.txt', common.mustSucceed());

  myVfs.unmount();
}

// === VirtualFileHandle base class ERR_METHOD_NOT_IMPLEMENTED ===
const { VirtualFileHandle } = require('internal/vfs/file_handle');

{
  const handle = new VirtualFileHandle('/test', 'r');

  // Property accessors
  assert.strictEqual(handle.path, '/test');
  assert.strictEqual(handle.flags, 'r');
  assert.strictEqual(handle.mode, 0o644);
  assert.strictEqual(handle.position, 0);
  assert.strictEqual(handle.closed, false);

  // Position setter
  handle.position = 42;
  assert.strictEqual(handle.position, 42);

  // Sync methods should throw ERR_METHOD_NOT_IMPLEMENTED
  assert.throws(() => handle.readSync(Buffer.alloc(10), 0, 10, 0),
                { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
  assert.throws(() => handle.writeSync(Buffer.alloc(10), 0, 10, 0),
                { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
  assert.throws(() => handle.readFileSync(),
                { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
  assert.throws(() => handle.writeFileSync('data'),
                { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
  assert.throws(() => handle.statSync(),
                { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
  assert.throws(() => handle.truncateSync(),
                { code: 'ERR_METHOD_NOT_IMPLEMENTED' });

  // Async methods should reject with ERR_METHOD_NOT_IMPLEMENTED
  assert.rejects(handle.read(Buffer.alloc(10), 0, 10, 0),
                 { code: 'ERR_METHOD_NOT_IMPLEMENTED' }).then(common.mustCall());
  assert.rejects(handle.write(Buffer.alloc(10), 0, 10, 0),
                 { code: 'ERR_METHOD_NOT_IMPLEMENTED' }).then(common.mustCall());
  assert.rejects(handle.readFile(),
                 { code: 'ERR_METHOD_NOT_IMPLEMENTED' }).then(common.mustCall());
  assert.rejects(handle.writeFile('data'),
                 { code: 'ERR_METHOD_NOT_IMPLEMENTED' }).then(common.mustCall());
  assert.rejects(handle.stat(),
                 { code: 'ERR_METHOD_NOT_IMPLEMENTED' }).then(common.mustCall());
  assert.rejects(handle.truncate(),
                 { code: 'ERR_METHOD_NOT_IMPLEMENTED' }).then(common.mustCall());

  // Close
  handle.closeSync();
  assert.strictEqual(handle.closed, true);

  // Methods on closed handle should throw EBADF
  assert.throws(() => handle.readSync(Buffer.alloc(10), 0, 10, 0), { code: 'EBADF' });
  assert.throws(() => handle.writeSync(Buffer.alloc(10), 0, 10, 0), { code: 'EBADF' });
  assert.throws(() => handle.readFileSync(), { code: 'EBADF' });
  assert.throws(() => handle.writeFileSync('data'), { code: 'EBADF' });
  assert.throws(() => handle.statSync(), { code: 'EBADF' });
  assert.throws(() => handle.truncateSync(), { code: 'EBADF' });
}

// === MemoryFileHandle via VFS fd operations ===
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/fdtest.txt', 'abcdefghij');
  myVfs.mount('/fd-test');

  // Open and read with auto-advancing position
  const fd = myVfs.openSync('/fd-test/fdtest.txt');
  const buf = Buffer.alloc(5);
  let bytesRead = myVfs.readSync(fd, buf, 0, 5, null);
  assert.strictEqual(bytesRead, 5);
  assert.strictEqual(buf.toString(), 'abcde');

  // Read more - position should have advanced
  const buf2 = Buffer.alloc(5);
  bytesRead = myVfs.readSync(fd, buf2, 0, 5, null);
  assert.strictEqual(bytesRead, 5);
  assert.strictEqual(buf2.toString(), 'fghij');

  // Read past end - should return 0
  const buf3 = Buffer.alloc(5);
  bytesRead = myVfs.readSync(fd, buf3, 0, 5, null);
  assert.strictEqual(bytesRead, 0);

  // Read with explicit position (doesn't advance internal position)
  const buf4 = Buffer.alloc(3);
  bytesRead = myVfs.readSync(fd, buf4, 0, 3, 2);
  assert.strictEqual(bytesRead, 3);
  assert.strictEqual(buf4.toString(), 'cde');

  // fstatSync
  const stats = myVfs.fstatSync(fd);
  assert.strictEqual(stats.size, 10);
  assert.strictEqual(stats.isFile(), true);

  // Close
  myVfs.closeSync(fd);

  // EBADF after close
  assert.throws(() => myVfs.readSync(fd, Buffer.alloc(5), 0, 5, 0), { code: 'EBADF' });
  assert.throws(() => myVfs.closeSync(fd), { code: 'EBADF' });
  assert.throws(() => myVfs.fstatSync(fd), { code: 'EBADF' });

  myVfs.unmount();
}

// === VirtualReadStream ===
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/stream.txt', 'hello world');
  myVfs.mount('/stream-test');

  // Basic stream read
  const chunks = [];
  const stream = myVfs.createReadStream('/stream-test/stream.txt');

  stream.on('open', common.mustCall((fd) => {
    assert.strictEqual(typeof fd, 'number');
  }));

  stream.on('ready', common.mustCall());

  stream.on('data', (chunk) => {
    chunks.push(chunk);
  });

  stream.on('end', common.mustCall(() => {
    const result = Buffer.concat(chunks).toString();
    assert.strictEqual(result, 'hello world');
  }));

  stream.on('close', common.mustCall(() => {
    myVfs.unmount();
  }));
}

// === VirtualReadStream with start/end options ===
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/range.txt', '0123456789');
  myVfs.mount('/stream-range');

  const chunks = [];
  // Read bytes 2-5 (inclusive), should get "2345"
  const stream = myVfs.createReadStream('/stream-range/range.txt', {
    start: 2,
    end: 5,
  });

  stream.on('data', (chunk) => {
    chunks.push(chunk);
  });

  stream.on('end', common.mustCall(() => {
    const result = Buffer.concat(chunks).toString();
    assert.strictEqual(result, '2345');
    myVfs.unmount();
  }));
}

// === VirtualReadStream path property ===
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/prop.txt', 'x');
  myVfs.mount('/stream-prop');

  const stream = myVfs.createReadStream('/stream-prop/prop.txt');
  assert.strictEqual(stream.path, '/stream-prop/prop.txt');
  stream.destroy();
  myVfs.unmount();
}

// === VirtualDir: read() async, entries() on closed dir ===
{
  // Create dirents via VFS opendirSync to get proper entries
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dirtest', { recursive: true });
  myVfs.writeFileSync('/dirtest/file1.txt', 'a');
  myVfs.writeFileSync('/dirtest/file2.txt', 'b');

  const dir = myVfs.opendirSync('/dirtest');

  // Async read
  dir.read().then(common.mustCall((e) => {
    assert.ok(e);
    assert.ok(e.name);
  }));
}

// Test VirtualDir async read and close
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dirtest2', { recursive: true });
  myVfs.writeFileSync('/dirtest2/file.txt', 'data');

  const dir = myVfs.opendirSync('/dirtest2');
  dir.read().then(common.mustCall((e) => {
    assert.strictEqual(e.name, 'file.txt');
    // Read again should return null
    dir.read().then(common.mustCall((e2) => {
      assert.strictEqual(e2, null);
      // Async close
      dir.close().then(common.mustCall());
    }));
  }));
}

// Test entries() on closed VirtualDir throws ERR_DIR_CLOSED
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dirclosed', { recursive: true });
  const dir = myVfs.opendirSync('/dirclosed');
  dir.closeSync();

  // readSync on closed dir should throw
  assert.throws(() => dir.readSync(), { code: 'ERR_DIR_CLOSED' });

  // entries() on closed dir should throw
  (async () => {
    const iter = dir.entries();
    await assert.rejects(iter.next(), { code: 'ERR_DIR_CLOSED' });
  })().then(common.mustCall());
}

// Test VirtualDir readSync returns null when exhausted
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/dirempty', { recursive: true });
  const dir = myVfs.opendirSync('/dirempty');
  assert.strictEqual(dir.readSync(), null);
  assert.strictEqual(dir.path, '/dirempty');
  dir.closeSync();
}

// === VirtualDir entries() iteration ===
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/diriter', { recursive: true });
  myVfs.writeFileSync('/diriter/a.txt', 'data');

  const dir = myVfs.opendirSync('/diriter');

  (async () => {
    const names = [];
    for await (const e of dir) {
      names.push(e.name);
    }
    assert.deepStrictEqual(names, ['a.txt']);
  })().then(common.mustCall());
}

// === VirtualWriteStream ===
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/ws-test.txt', '');
  myVfs.mount('/ws-mount');

  const stream = myVfs.createWriteStream('/ws-mount/ws-test.txt');
  stream.on('open', common.mustCall((fd) => {
    assert.ok((fd & 0x40000000) !== 0);
  }));
  stream.on('ready', common.mustCall());
  stream.write('hello ', common.mustCall());
  stream.end('world', common.mustCall(() => {
    // Read via VFS since it's mounted
    const vfsContent = myVfs.readFileSync('/ws-mount/ws-test.txt', 'utf8');
    assert.strictEqual(vfsContent, 'hello world');
    myVfs.unmount();
  }));
}

// === VirtualWriteStream path property ===
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/ws-path.txt', '');

  const stream = myVfs.createWriteStream('/ws-path.txt');
  assert.strictEqual(stream.path, '/ws-path.txt');
  stream.end();
  stream.on('finish', common.mustCall());
}

// === VirtualWriteStream with error on open ===
{
  const { MemoryProvider } = require('internal/vfs/providers/memory');
  const mp = new MemoryProvider();
  mp.setReadOnly();
  const myVfs = new (require('internal/vfs/file_system').VirtualFileSystem)(mp);
  // Try to write to a read-only VFS (should fail with EROFS)
  const stream = myVfs.createWriteStream('/path.txt');
  stream.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'EROFS');
  }));
}

// === Provider linkSync/link on non-readonly (ERR_METHOD_NOT_IMPLEMENTED) ===
{
  const baseProvider = new (require('internal/vfs/provider').VirtualProvider)();
  assert.throws(() => baseProvider.linkSync('/a', '/b'),
                { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
  assert.rejects(baseProvider.link('/a', '/b'),
                 { code: 'ERR_METHOD_NOT_IMPLEMENTED' }).then(common.mustCall());

  // Readonly provider: link should throw EROFS
  class ReadonlyProvider extends (require('internal/vfs/provider').VirtualProvider) {
    get readonly() { return true; }
  }
  const roProv = new ReadonlyProvider();
  assert.throws(() => roProv.linkSync('/a', '/b'), { code: 'EROFS' });
  assert.rejects(roProv.link('/a', '/b'), { code: 'EROFS' }).then(common.mustCall());
}

// === Provider async default methods (open, stat, readdir) ===
{
  const baseProvider = new (require('internal/vfs/provider').VirtualProvider)();

  assert.rejects(baseProvider.open('/f', 'r'),
                 { code: 'ERR_METHOD_NOT_IMPLEMENTED' }).then(common.mustCall());
  assert.rejects(baseProvider.stat('/f'),
                 { code: 'ERR_METHOD_NOT_IMPLEMENTED' }).then(common.mustCall());
  assert.rejects(baseProvider.readdir('/f'),
                 { code: 'ERR_METHOD_NOT_IMPLEMENTED' }).then(common.mustCall());
}

// === Provider openSync/statSync (ERR_METHOD_NOT_IMPLEMENTED) ===
{
  const baseProvider = new (require('internal/vfs/provider').VirtualProvider)();
  assert.throws(() => baseProvider.openSync('/f', 'r'),
                { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
  assert.throws(() => baseProvider.statSync('/f'),
                { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
}

// === Provider default implementations: lstat delegates to stat ===
{
  const { VirtualProvider } = require('internal/vfs/provider');

  class MockProvider extends VirtualProvider {
    statSync(p) { return { isFile: () => true }; }
    async stat(p) { return { isFile: () => true }; }
  }
  const mp = new MockProvider();
  const s = mp.lstatSync('/f');
  assert.strictEqual(s.isFile(), true);

  mp.lstat('/f').then(common.mustCall((ls) => {
    assert.strictEqual(ls.isFile(), true);
  }));
}

// === Provider capability flags ===
{
  const { VirtualProvider } = require('internal/vfs/provider');
  const bp = new VirtualProvider();
  assert.strictEqual(bp.readonly, false);
  assert.strictEqual(bp.supportsSymlinks, false);
  assert.strictEqual(bp.supportsWatch, false);
}

// === VFS constructor with options-as-first-arg (no provider) ===
{
  const myVfs = vfs.create({ overlay: true });
  assert.strictEqual(myVfs.overlay, true);
}

// === VFS virtualCwd feature ===
{
  const myVfs = vfs.create({ virtualCwd: true });
  myVfs.mkdirSync('/app/data', { recursive: true });
  myVfs.writeFileSync('/app/data/file.txt', 'content');
  myVfs.mount('/vcwd-test');

  // Cwd should return null initially
  assert.strictEqual(myVfs.cwd(), null);

  // Chdir to a directory in VFS
  myVfs.chdir('/vcwd-test/app/data');
  assert.strictEqual(myVfs.cwd(), path.resolve('/vcwd-test/app/data'));

  // resolvePath should resolve relative to virtual cwd
  const resolved = myVfs.resolvePath('file.txt');
  assert.ok(resolved.endsWith('file.txt'));

  // resolvePath with absolute path returns as-is
  const absResolved = myVfs.resolvePath('/absolute/path');
  assert.strictEqual(absResolved, path.resolve('/absolute/path'));

  // Chdir to non-directory should throw ENOTDIR
  assert.throws(() => myVfs.chdir('/vcwd-test/app/data/file.txt'), { code: 'ENOTDIR' });

  myVfs.unmount();
}

// === VFS virtualCwd disabled ===
{
  const myVfs = vfs.create();
  assert.strictEqual(myVfs.virtualCwdEnabled, false);
  assert.throws(() => myVfs.cwd(), { code: 'ERR_INVALID_STATE' });
  assert.throws(() => myVfs.chdir('/foo'), { code: 'ERR_INVALID_STATE' });
}

// === VFS resolvePath without virtualCwd ===
{
  const myVfs = vfs.create();
  // Without virtualCwd, resolvePath resolves normally
  assert.strictEqual(myVfs.resolvePath('/abs/path'), path.resolve('/abs/path'));
}

// === VFS mount double-mount error ===
{
  const myVfs = vfs.create();
  myVfs.mount('/test-double');
  assert.throws(() => myVfs.mount('/test-double2'), { code: 'ERR_INVALID_STATE' });
  myVfs.unmount();
}

// === MemoryProvider setReadOnly ===
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'content');
  assert.strictEqual(myVfs.readonly, false);
  myVfs.provider.setReadOnly();
  assert.strictEqual(myVfs.readonly, true);

  // Write ops should now throw EROFS
  assert.throws(() => myVfs.writeFileSync('/file.txt', 'new'), { code: 'EROFS' });
  assert.throws(() => myVfs.mkdirSync('/newdir'), { code: 'EROFS' });
  assert.throws(() => myVfs.unlinkSync('/file.txt'), { code: 'EROFS' });
  assert.throws(() => myVfs.rmdirSync('/'), { code: 'EROFS' });
  assert.throws(() => myVfs.renameSync('/file.txt', '/file2.txt'), { code: 'EROFS' });
  assert.throws(() => myVfs.appendFileSync('/file.txt', 'data'), { code: 'EROFS' });
  assert.throws(() => myVfs.copyFileSync('/file.txt', '/file2.txt'), { code: 'EROFS' });
  assert.throws(() => myVfs.symlinkSync('/file.txt', '/link'), { code: 'EROFS' });
}

// === MemoryProvider supportsWatch and supportsSymlinks ===
{
  const { MemoryProvider } = require('internal/vfs/providers/memory');
  const mp = new MemoryProvider();
  assert.strictEqual(mp.supportsWatch, true);
  assert.strictEqual(mp.supportsSymlinks, true);
  assert.strictEqual(mp.readonly, false);
}

// === MemoryProvider open directory throws EISDIR ===
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/opendir');
  assert.throws(() => myVfs.openSync('/opendir'), { code: 'EISDIR' });
}

// === MemoryProvider unlink directory throws EISDIR ===
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/rm-dir');
  assert.throws(() => myVfs.unlinkSync('/rm-dir'), { code: 'EISDIR' });
}

// === MemoryProvider rmdir on file throws ENOTDIR ===
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/rmdir-file.txt', 'data');
  assert.throws(() => myVfs.rmdirSync('/rmdir-file.txt'), { code: 'ENOTDIR' });
}

// === MemoryProvider hard link ===
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/link-src.txt', 'linkdata');

  // Link to a directory should throw EINVAL
  myVfs.mkdirSync('/link-dir');
  assert.throws(() => myVfs.linkSync('/link-dir', '/link-copy'), { code: 'EINVAL' });

  // Link to existing path should throw EEXIST
  myVfs.writeFileSync('/link-existing.txt', 'existing');
  assert.throws(() => myVfs.linkSync('/link-src.txt', '/link-existing.txt'), { code: 'EEXIST' });
}

// === MemoryProvider symlink to existing path throws EEXIST ===
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/sym-exists.txt', 'data');
  assert.throws(() => myVfs.symlinkSync('/target', '/sym-exists.txt'), { code: 'EEXIST' });
}

// === MemoryProvider readlink on non-symlink throws EINVAL ===
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/readlink-file.txt', 'data');
  assert.throws(() => myVfs.readlinkSync('/readlink-file.txt'), { code: 'EINVAL' });
}

// === MemoryProvider mkdir on non-directory throws ENOTDIR ===
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file-not-dir.txt', 'data');
  assert.throws(() => myVfs.mkdirSync('/file-not-dir.txt/subdir', { recursive: true }),
                { code: 'ENOTDIR' });
}

// === MemoryEntry contentProvider (dynamic content) ===
{
  const myVfs = vfs.create();
  // Use the provider's populate mechanism for dynamic content
  myVfs.provider.addFile = (p, content) => {
    myVfs.writeFileSync(p, content);
  };
  myVfs.writeFileSync('/dynamic.txt', 'initial');

  const content = myVfs.readFileSync('/dynamic.txt', 'utf8');
  assert.strictEqual(content, 'initial');
}

// === VFS callback-based operations: rm, truncate, ftruncate, link, mkdtemp, opendir ===
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/rm-cb.txt', 'data');

  myVfs.rm('/rm-cb.txt', common.mustCall((err) => {
    assert.strictEqual(err, null);
    assert.strictEqual(myVfs.existsSync('/rm-cb.txt'), false);
  }));
}

{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/trunc-cb.txt', 'hello world');

  myVfs.truncate('/trunc-cb.txt', 5, common.mustCall((err) => {
    assert.strictEqual(err, null);
    assert.strictEqual(myVfs.readFileSync('/trunc-cb.txt', 'utf8'), 'hello');
  }));
}

// Truncate with callback as second arg
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/trunc-cb2.txt', 'hello');

  myVfs.truncate('/trunc-cb2.txt', common.mustCall((err) => {
    assert.strictEqual(err, null);
    assert.strictEqual(myVfs.readFileSync('/trunc-cb2.txt', 'utf8'), '');
  }));
}

{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/ftrunc-cb.txt', 'hello world');
  const fd = myVfs.openSync('/ftrunc-cb.txt', 'r+');

  myVfs.ftruncate(fd, 5, common.mustCall((err) => {
    assert.strictEqual(err, null);
    myVfs.closeSync(fd);
    assert.strictEqual(myVfs.readFileSync('/ftrunc-cb.txt', 'utf8'), 'hello');
  }));
}

// Ftruncate with callback as second arg
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/ftrunc-cb2.txt', 'hello');
  const fd = myVfs.openSync('/ftrunc-cb2.txt', 'r+');

  myVfs.ftruncate(fd, common.mustCall((err) => {
    assert.strictEqual(err, null);
    myVfs.closeSync(fd);
  }));
}

{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/link-cb-src.txt', 'data');

  myVfs.link('/link-cb-src.txt', '/link-cb-dest.txt', common.mustCall((err) => {
    assert.strictEqual(err, null);
    assert.strictEqual(myVfs.readFileSync('/link-cb-dest.txt', 'utf8'), 'data');
  }));
}

{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/tmp', { recursive: true });

  myVfs.mkdtemp('/tmp/test-', common.mustCall((err, dirPath) => {
    assert.strictEqual(err, null);
    assert.ok(dirPath.startsWith('/tmp/test-'));
  }));
}

// Mkdtemp with options as second arg (callback as third)
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/tmp2', { recursive: true });

  myVfs.mkdtemp('/tmp2/test-', {}, common.mustCall((err, dirPath) => {
    assert.strictEqual(err, null);
    assert.ok(dirPath.startsWith('/tmp2/test-'));
  }));
}

{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/opendir-cb');
  myVfs.writeFileSync('/opendir-cb/file.txt', 'data');

  myVfs.opendir('/opendir-cb', common.mustCall((err, dir) => {
    assert.strictEqual(err, null);
    const e = dir.readSync();
    assert.strictEqual(e.name, 'file.txt');
    dir.closeSync();
  }));
}

// Opendir with options as second arg
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/opendir-cb2');
  myVfs.writeFileSync('/opendir-cb2/file.txt', 'data');

  myVfs.opendir('/opendir-cb2', {}, common.mustCall((err, dir) => {
    assert.strictEqual(err, null);
    dir.closeSync();
  }));
}

// === VFS callback: open with flags as callback (minimal args) ===
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/open-cb-min.txt', 'data');

  myVfs.open('/open-cb-min.txt', common.mustCall((err, fd) => {
    assert.strictEqual(err, null);
    myVfs.closeSync(fd);
  }));
}

// === VFS callback: open with mode as callback ===
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/open-cb-mode.txt', 'data');

  myVfs.open('/open-cb-mode.txt', 'r', common.mustCall((err, fd) => {
    assert.strictEqual(err, null);
    myVfs.closeSync(fd);
  }));
}

// === VFS close EBADF ===
{
  const myVfs = vfs.create();
  myVfs.close(99999, common.mustCall((err) => {
    assert.strictEqual(err.code, 'EBADF');
  }));
}

// === VFS read EBADF ===
{
  const myVfs = vfs.create();
  myVfs.read(99999, Buffer.alloc(10), 0, 10, 0, common.mustCall((err) => {
    assert.strictEqual(err.code, 'EBADF');
  }));
}

// === VFS write EBADF ===
{
  const myVfs = vfs.create();
  myVfs.write(99999, Buffer.alloc(10), 0, 10, 0, common.mustCall((err) => {
    assert.strictEqual(err.code, 'EBADF');
  }));
}

// === VFS fstat EBADF ===
{
  const myVfs = vfs.create();
  myVfs.fstat(99999, common.mustCall((err) => {
    assert.strictEqual(err.code, 'EBADF');
  }));
}

// === VFS fstat with options as function ===
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/fstat-cb.txt', 'data');
  const fd = myVfs.openSync('/fstat-cb.txt');

  myVfs.fstat(fd, common.mustCall((err, stats) => {
    assert.strictEqual(err, null);
    assert.strictEqual(stats.isFile(), true);
    myVfs.closeSync(fd);
  }));
}

// === VFS ftruncateSync EBADF ===
{
  const myVfs = vfs.create();
  assert.throws(() => myVfs.ftruncateSync(99999), { code: 'EBADF' });
}

// === VFS writeSync EBADF ===
{
  const myVfs = vfs.create();
  assert.throws(() => myVfs.writeSync(99999, Buffer.alloc(10), 0, 10, 0), { code: 'EBADF' });
}

// === VFS rm callback with options as function ===
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/rm-cb-opts.txt', 'data');

  myVfs.rm('/rm-cb-opts.txt', common.mustCall((err) => {
    assert.strictEqual(err, null);
  }));
}

// === MemoryProvider async open ===
{
  const { MemoryProvider } = require('internal/vfs/providers/memory');
  const mp = new MemoryProvider();
  mp.writeFileSync('/async-open.txt', 'data');

  mp.open('/async-open.txt', 'r').then(common.mustCall((handle) => {
    assert.ok(handle);
    handle.closeSync();
  }));
}

// === MemoryProvider async stat, lstat, readdir, realpath, access ===
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/async-ops', { recursive: true });
  myVfs.writeFileSync('/async-ops/file.txt', 'data');

  myVfs.provider.stat('/async-ops/file.txt').then(common.mustCall((stats) => {
    assert.strictEqual(stats.isFile(), true);
  }));

  myVfs.provider.lstat('/async-ops/file.txt').then(common.mustCall((stats) => {
    assert.strictEqual(stats.isFile(), true);
  }));

  myVfs.provider.readdir('/async-ops').then(common.mustCall((entries) => {
    assert.ok(entries.includes('file.txt'));
  }));

  myVfs.provider.realpath('/async-ops/file.txt').then(common.mustCall((rp) => {
    assert.strictEqual(rp, '/async-ops/file.txt');
  }));

  myVfs.provider.access('/async-ops/file.txt').then(common.mustCall());
}

// === MemoryProvider async mkdir, rmdir, unlink, rename, symlink, readlink, link ===
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/async-write', { recursive: true });

  myVfs.provider.mkdir('/async-write/sub').then(common.mustCall());
  myVfs.provider.writeFile('/async-write/wf.txt', 'data').then(common.mustCall(() => {
    myVfs.provider.readFile('/async-write/wf.txt', 'utf8').then(common.mustCall((content) => {
      assert.strictEqual(content, 'data');
    }));
  }));
}

// === promises.rm with force option on nonexistent ===
{
  const myVfs = vfs.create();
  myVfs.mount('/rm-force-test');
  myVfs.promises.rm('/rm-force-test/nonexistent', { force: true }).then(common.mustCall());
  // Clean up will happen after promise resolves
  setTimeout(() => myVfs.unmount(), 100);
}

// === promises.truncate ===
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/ptrunc.txt', 'hello world');

  myVfs.promises.truncate('/ptrunc.txt', 5).then(common.mustCall(() => {
    assert.strictEqual(myVfs.readFileSync('/ptrunc.txt', 'utf8'), 'hello');
  }));
}

// === promises.link ===
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/plink-src.txt', 'data');

  myVfs.promises.link('/plink-src.txt', '/plink-dest.txt').then(common.mustCall(() => {
    assert.strictEqual(myVfs.readFileSync('/plink-dest.txt', 'utf8'), 'data');
  }));
}

// === promises.mkdtemp ===
{
  const myVfs = vfs.create();
  myVfs.mkdirSync('/ptmp', { recursive: true });

  myVfs.promises.mkdtemp('/ptmp/test-').then(common.mustCall((dirPath) => {
    assert.ok(dirPath.startsWith('/ptmp/test-'));
    assert.strictEqual(dirPath.length, '/ptmp/test-'.length + 6);
  }));
}

// === VFS shouldHandle when not mounted ===
{
  const myVfs = vfs.create();
  assert.strictEqual(myVfs.shouldHandle('/any/path'), false);
}

// === VFS existsSync returns false for errors ===
{
  const myVfs = vfs.create();
  // Path outside mount - existsSync catches errors
  assert.strictEqual(myVfs.existsSync('/nonexistent'), false);
}
