// Flags: --experimental-vfs
'use strict';

// Exercise the VFS callback-style async API on every method.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/dir/sub', { recursive: true });
myVfs.writeFileSync('/dir/file.txt', 'hello');
myVfs.writeFileSync('/dir/other.txt', 'other');

// readFile (no options)
myVfs.readFile('/dir/file.txt', common.mustSucceed((data) => {
  assert.ok(Buffer.isBuffer(data));
}));

// writeFile + appendFile (no options) -> readFile
myVfs.writeFile('/cb-write.txt', 'a', common.mustSucceed(() => {
  myVfs.readFile('/cb-write.txt', 'utf8', common.mustSucceed((data) => {
    assert.strictEqual(data, 'a');
  }));
}));

// stat / lstat (with and without options)
myVfs.stat('/dir/file.txt', common.mustSucceed((st) => {
  assert.strictEqual(st.size, 5);
}));
myVfs.stat('/dir/file.txt', { bigint: true }, common.mustSucceed((st) => {
  assert.strictEqual(typeof st.size, 'bigint');
}));
myVfs.lstat('/dir/file.txt', common.mustSucceed((st) => {
  assert.ok(st.isFile());
}));

// readdir
myVfs.readdir('/dir', common.mustSucceed((names) => {
  assert.ok(names.includes('file.txt'));
}));

// realpath
myVfs.realpath('/dir/file.txt', common.mustSucceed((p) => {
  assert.strictEqual(p, '/dir/file.txt');
}));

// access (with and without mode)
myVfs.access('/dir/file.txt', common.mustSucceed());
myVfs.access('/dir/file.txt', 0, common.mustSucceed());
myVfs.access('/missing.txt', common.mustCall((err) => {
  assert.strictEqual(err.code, 'ENOENT');
}));

// open / read / write / close cb chain
myVfs.open('/dir/file.txt', common.mustSucceed((fd) => {
  const buf = Buffer.alloc(5);
  myVfs.read(fd, buf, 0, 5, 0, common.mustSucceed((bytesRead) => {
    assert.strictEqual(bytesRead, 5);
    assert.strictEqual(buf.toString(), 'hello');
    myVfs.close(fd, common.mustSucceed());
  }));
}));

// Open with explicit flags / mode
myVfs.open('/dir/new1.txt', 'w', common.mustSucceed((fd) => {
  const buf = Buffer.from('xyz');
  myVfs.write(fd, buf, 0, 3, 0, common.mustSucceed((bytesWritten) => {
    assert.strictEqual(bytesWritten, 3);
    myVfs.fstat(fd, common.mustSucceed((st) => {
      assert.strictEqual(st.size, 3);
      myVfs.ftruncate(fd, 1, common.mustSucceed(() => {
        myVfs.close(fd, common.mustCall());
      }));
    }));
  }));
}));

// Open with explicit flags, no mode arg form
myVfs.open('/dir/new2.txt', 'w', 0o644, common.mustSucceed((fd) => {
  myVfs.close(fd, common.mustCall());
}));

// rm callback (file)
myVfs.writeFileSync('/cb-rm.txt', 'x');
myVfs.rm('/cb-rm.txt', common.mustSucceed(() => {
  assert.strictEqual(myVfs.existsSync('/cb-rm.txt'), false);
}));

// Rm callback with options (recursive)
myVfs.mkdirSync('/cb-rm-dir/sub', { recursive: true });
myVfs.writeFileSync('/cb-rm-dir/sub/f.txt', 'x');
myVfs.rm('/cb-rm-dir', { recursive: true }, common.mustSucceed());

// Rm callback failure path
myVfs.rm('/missing-rm', common.mustCall((err) => {
  assert.strictEqual(err.code, 'ENOENT');
}));

// truncate / ftruncate cb
myVfs.writeFileSync('/cb-tr.txt', 'abcdef');
myVfs.truncate('/cb-tr.txt', 3, common.mustSucceed(() => {
  assert.strictEqual(myVfs.readFileSync('/cb-tr.txt', 'utf8'), 'abc');
}));
myVfs.truncate('/missing-tr.txt', common.mustCall((err) => {
  assert.ok(err);
}));
myVfs.ftruncate(0xFFFFFFF /* invalid fd */, common.mustCall((err) => {
  assert.strictEqual(err.code, 'EBADF');
}));

// link cb
myVfs.writeFileSync('/cb-link-src.txt', 'x');
myVfs.link('/cb-link-src.txt', '/cb-link-dst.txt', common.mustSucceed());
myVfs.link('/missing-src.txt', '/cb-bad-link.txt', common.mustCall((err) => {
  assert.ok(err);
}));

// mkdtemp cb
myVfs.mkdtemp('/tmp-', common.mustSucceed((p) => {
  assert.ok(p.startsWith('/tmp-'));
}));
myVfs.mkdtemp('/tmp-', {}, common.mustSucceed((p) => {
  assert.ok(p.startsWith('/tmp-'));
}));

// opendir cb
myVfs.opendir('/dir', common.mustSucceed((dir) => {
  assert.strictEqual(dir.path, '/dir');
  dir.closeSync();
}));
myVfs.opendir('/missing-dir', common.mustCall((err) => {
  assert.ok(err);
}));

// EBADF callback paths
myVfs.read(0xFFFFFFF, Buffer.alloc(1), 0, 1, 0, common.mustCall((err) => {
  assert.strictEqual(err.code, 'EBADF');
}));
myVfs.write(0xFFFFFFF, Buffer.alloc(1), 0, 1, 0, common.mustCall((err) => {
  assert.strictEqual(err.code, 'EBADF');
}));
myVfs.close(0xFFFFFFF, common.mustCall((err) => {
  assert.strictEqual(err.code, 'EBADF');
}));
myVfs.fstat(0xFFFFFFF, common.mustCall((err) => {
  assert.strictEqual(err.code, 'EBADF');
}));

// readlink cb
myVfs.symlinkSync('/dir/file.txt', '/cb-link');
myVfs.readlink('/cb-link', common.mustSucceed((target) => {
  assert.strictEqual(target, '/dir/file.txt');
}));
