'use strict';

// Exercise VirtualFileHandle / MemoryFileHandle methods directly via
// the promises.open() handle returned by VFS.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/file.txt', 'hello world');

(async () => {
  // Open file via provider directly (returns a real FileHandle)
  const handle = await myVfs.provider.open('/file.txt', 'r');
  assert.ok(handle);
  assert.strictEqual(handle.path, '/file.txt');
  assert.strictEqual(handle.flags, 'r');
  assert.strictEqual(typeof handle.mode, 'number');
  assert.strictEqual(handle.closed, false);

  // readFile
  const content = await handle.readFile('utf8');
  assert.strictEqual(content, 'hello world');

  // stat
  const stats = await handle.stat();
  assert.strictEqual(stats.size, 11);

  // read into buffer
  const buf = Buffer.alloc(5);
  const { bytesRead } = await handle.read(buf, 0, 5, 0);
  assert.strictEqual(bytesRead, 5);
  assert.strictEqual(buf.toString(), 'hello');

  // readv
  const b1 = Buffer.alloc(5);
  const b2 = Buffer.alloc(6);
  const readvResult = await handle.readv([b1, b2], 0);
  assert.strictEqual(readvResult.bytesRead, 11);
  assert.strictEqual(b1.toString(), 'hello');
  assert.strictEqual(b2.toString(), ' world');

  // no-op metadata methods
  await handle.chmod();
  await handle.chown();
  await handle.utimes();
  await handle.datasync();
  await handle.sync();

  await handle.close();
  assert.strictEqual(handle.closed, true);
})().then(common.mustCall());

// Write mode: truncate, write, writev, appendFile, truncate
(async () => {
  const handle = await myVfs.provider.open('/out.txt', 'w+');

  // No explicit position so the handle position advances naturally
  await handle.write(Buffer.from('hello'), 0, 5);
  await handle.writev([Buffer.from(' '), Buffer.from('world')]);

  const stats = await handle.stat();
  assert.strictEqual(stats.size, 11);

  await handle.appendFile('!');
  const content = await handle.readFile('utf8');
  assert.strictEqual(content, 'hello world!');

  await handle.truncate(5);
  const truncated = await handle.readFile('utf8');
  assert.strictEqual(truncated, 'hello');

  await handle.close();
})().then(common.mustCall());

// readSync / writeSync / readFileSync / writeFileSync / statSync / truncateSync / closeSync
{
  const fd = myVfs.openSync('/sync.txt', 'w');
  const buf = Buffer.from('abc');
  myVfs.writeSync(fd, buf, 0, 3, 0);
  myVfs.closeSync(fd);

  const fd2 = myVfs.openSync('/sync.txt', 'r');
  const out = Buffer.alloc(3);
  myVfs.readSync(fd2, out, 0, 3, 0);
  assert.strictEqual(out.toString(), 'abc');
  const stats = myVfs.fstatSync(fd2);
  assert.strictEqual(stats.size, 3);
  myVfs.closeSync(fd2);
}

// using-style explicit resource management for handles
(async () => {
  const handle = await myVfs.provider.open('/file.txt', 'r');
  await handle[Symbol.asyncDispose]();
  assert.strictEqual(handle.closed, true);
})().then(common.mustCall());

// readableWebStream / readLines / createReadStream / createWriteStream throw
(async () => {
  const handle = await myVfs.provider.open('/file.txt', 'r');
  assert.throws(() => handle.readableWebStream(),
                { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
  assert.throws(() => handle.readLines(),
                { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
  assert.throws(() => handle.createReadStream(),
                { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
  assert.throws(() => handle.createWriteStream(),
                { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
  await handle.close();
})().then(common.mustCall());

// Operations after close throw EBADF
(async () => {
  const handle = await myVfs.provider.open('/file.txt', 'r');
  await handle.close();
  await assert.rejects(handle.read(Buffer.alloc(1), 0, 1, 0),
                       { code: 'EBADF' });
  assert.throws(() => handle.readSync(Buffer.alloc(1), 0, 1, 0),
                { code: 'EBADF' });
  await assert.rejects(handle.stat(), { code: 'EBADF' });
})().then(common.mustCall());
