// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');

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
assert.strictEqual(isUnderMountPoint('/mount', '/mount'), true);
assert.strictEqual(isUnderMountPoint('/mount/file', '/mount'), true);
assert.strictEqual(isUnderMountPoint('/mountx', '/mount'), false);
assert.strictEqual(isUnderMountPoint('/other', '/mount'), false);
// Root mount point
assert.strictEqual(isUnderMountPoint('/anything', '/'), true);

// getRelativePath
assert.strictEqual(getRelativePath('/mount', '/mount'), '/');
assert.strictEqual(getRelativePath('/mount/file.js', '/mount'), '/file.js');
assert.strictEqual(getRelativePath('/mount/a/b', '/mount'), '/a/b');
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
  assert.strictEqual(myVfs.realpathSync('/internals-test/file.txt'), '/internals-test/file.txt');
  assert.strictEqual(myVfs.realpathSync('/internals-test/dir'), '/internals-test/dir');

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
