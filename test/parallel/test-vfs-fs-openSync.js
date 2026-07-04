// Flags: --experimental-vfs
'use strict';

// fs.openSync / fs.readSync / fs.writeSync / fs.fstatSync / fs.closeSync /
// fs.ftruncateSync / fs.readvSync / fs.writevSync dispatch to VFS and operate
// on the bitmask-encoded virtual fd. The noop FD handlers (fchmodSync, etc.)
// short-circuit to true for virtual fds.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const mountPoint = path.resolve('/tmp/vfs-openSync-' + process.pid);
const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello world');
myVfs.mount(mountPoint);

// openSync + fstatSync + readSync + closeSync
{
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r');
  assert.notStrictEqual(fd & 0x40000000, 0); // VFS bitmask is set
  const stats = fs.fstatSync(fd);
  assert.strictEqual(stats.isFile(), true);
  const buf = Buffer.alloc(11);
  assert.strictEqual(fs.readSync(fd, buf, 0, 11, 0), 11);
  assert.strictEqual(buf.toString(), 'hello world');
  fs.closeSync(fd);
}

// readSync with position null uses and advances the current file position
{
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r');
  const b1 = Buffer.alloc(5);
  const b2 = Buffer.alloc(6);

  assert.strictEqual(fs.readSync(fd, b1, 0, 5, null), 5);
  assert.strictEqual(fs.readSync(fd, b2, 0, 6, null), 6);
  assert.strictEqual(b1.toString(), 'hello');
  assert.strictEqual(b2.toString(), ' world');
  fs.closeSync(fd);
}

// openSync + writeSync (buffer) + closeSync
{
  const fd = fs.openSync(path.join(mountPoint, 'src/wfd.txt'), 'w');
  assert.strictEqual(fs.writeSync(fd, Buffer.from('via-fd')), 6);
  fs.closeSync(fd);
  assert.strictEqual(
    fs.readFileSync(path.join(mountPoint, 'src/wfd.txt'), 'utf8'),
    'via-fd',
  );
}

// writeSync with string + encoding
{
  const fd = fs.openSync(path.join(mountPoint, 'src/str.txt'), 'w');
  const n = fs.writeSync(fd, 'string-data', 0, 'utf8');
  assert.ok(n > 0);
  fs.closeSync(fd);
  assert.strictEqual(
    fs.readFileSync(path.join(mountPoint, 'src/str.txt'), 'utf8'),
    'string-data',
  );
}

// ftruncateSync
{
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r+');
  fs.ftruncateSync(fd, 5);
  fs.closeSync(fd);
  assert.strictEqual(
    fs.readFileSync(path.join(mountPoint, 'src/hello.txt'), 'utf8'),
    'hello',
  );
}

// fchmodSync, fchownSync, futimesSync, fdatasyncSync, fsyncSync are noops
{
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r+');
  fs.fchmodSync(fd, 0o644);
  fs.fchownSync(fd, process.getuid?.() ?? 0, process.getgid?.() ?? 0);
  const now = new Date();
  fs.futimesSync(fd, now, now);
  fs.fdatasyncSync(fd);
  fs.fsyncSync(fd);
  fs.closeSync(fd);
}

// readvSync + writevSync
{
  const wf = fs.openSync(path.join(mountPoint, 'src/v.txt'), 'w');
  fs.writevSync(wf, [Buffer.from('abc'), Buffer.from('def')]);
  fs.closeSync(wf);

  const rf = fs.openSync(path.join(mountPoint, 'src/v.txt'), 'r');
  const b1 = Buffer.alloc(3);
  const b2 = Buffer.alloc(3);
  assert.strictEqual(fs.readvSync(rf, [b1, b2], 0), 6);
  assert.strictEqual(b1.toString() + b2.toString(), 'abcdef');
  fs.closeSync(rf);
}

myVfs.unmount();
