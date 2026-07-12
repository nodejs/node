// Flags: --experimental-vfs
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

  const fd3 = myVfs.openSync('/sync-current.txt', 'w');
  myVfs.writeSync(fd3, Buffer.from('abc'), 0, 3, -1);
  myVfs.writeSync(fd3, Buffer.from('def'), 0, 3, -1);
  myVfs.closeSync(fd3);
  assert.strictEqual(myVfs.readFileSync('/sync-current.txt', 'utf8'), 'abcdef');

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

// Readv with a partial read at EOF (second buffer larger than remaining)
(async () => {
  const handle = await myVfs.provider.open('/file.txt', 'r');
  const b1 = Buffer.alloc(5);
  const b2 = Buffer.alloc(20);
  const r = await handle.readv([b1, b2], 0);
  assert.strictEqual(b1.toString(), 'hello');
  assert.strictEqual(r.bytesRead, 11);
  await handle.close();
})().then(common.mustCall());

// Writev with explicit position 0
(async () => {
  const wh = await myVfs.provider.open('/wv.txt', 'w+');
  await wh.writev([Buffer.from('AB'), Buffer.from('CD')], 0);
  await wh.close();
  assert.strictEqual(myVfs.readFileSync('/wv.txt', 'utf8'), 'ABCD');
})().then(common.mustCall());

// appendFile with string + encoding option
(async () => {
  const ah = await myVfs.provider.open('/ap.txt', 'a+');
  await ah.appendFile('hello', { encoding: 'utf8' });
  await ah.close();
  assert.strictEqual(myVfs.readFileSync('/ap.txt', 'utf8'), 'hello');
})().then(common.mustCall());

// 'w'-mode handle rejects all read ops with EBADF
(async () => {
  const handle = await myVfs.provider.open('/wonly.txt', 'w');
  assert.throws(() => handle.readSync(Buffer.alloc(1), 0, 1, 0),
                { code: 'EBADF' });
  await assert.rejects(handle.read(Buffer.alloc(1), 0, 1, 0),
                       { code: 'EBADF' });
  assert.throws(() => handle.readFileSync(), { code: 'EBADF' });
  await assert.rejects(handle.readFile(), { code: 'EBADF' });
  await handle.close();
})().then(common.mustCall());

// 'r'-mode handle rejects all write ops with EBADF
(async () => {
  myVfs.writeFileSync('/ronly.txt', 'x');
  const handle = await myVfs.provider.open('/ronly.txt', 'r');
  assert.throws(() => handle.writeSync(Buffer.from('y'), 0, 1, 0),
                { code: 'EBADF' });
  await assert.rejects(handle.write(Buffer.from('y'), 0, 1, 0),
                       { code: 'EBADF' });
  assert.throws(() => handle.writeFileSync('y'), { code: 'EBADF' });
  await assert.rejects(handle.writeFile('y'), { code: 'EBADF' });
  assert.throws(() => handle.truncateSync(0), { code: 'EBADF' });
  await assert.rejects(handle.truncate(0), { code: 'EBADF' });
  await handle.close();
})().then(common.mustCall());

// writeFile with string + encoding
(async () => {
  const handle = await myVfs.provider.open('/se.txt', 'w+');
  await handle.writeFile('héllo', { encoding: 'utf8' });
  assert.strictEqual(await handle.readFile('utf8'), 'héllo');
  await handle.close();
})().then(common.mustCall());

// Truncate extending past current size zero-fills
(async () => {
  const handle = await myVfs.provider.open('/grow.txt', 'w+');
  await handle.writeFile('abc');
  await handle.truncate(10);
  assert.strictEqual((await handle.stat()).size, 10);
  assert.strictEqual((await handle.readFile()).length, 10);
  await handle.close();
})().then(common.mustCall());

// readv / writev / appendFile on a closed handle reject with EBADF
(async () => {
  const handle = await myVfs.provider.open('/file.txt', 'r');
  await handle.close();
  await assert.rejects(handle.readv([Buffer.alloc(1)], 0), { code: 'EBADF' });
  await assert.rejects(handle.writev([Buffer.alloc(1)], 0), { code: 'EBADF' });
  await assert.rejects(handle.appendFile('x'), { code: 'EBADF' });
})().then(common.mustCall());
