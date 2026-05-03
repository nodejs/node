// Flags: --expose-internals
'use strict';

// Cover branch paths in MemoryFileHandle (and base VirtualFileHandle).

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/file.txt', 'hello world');

(async () => {
  // readv with explicit position and a partial read at EOF
  const handle = await myVfs.provider.open('/file.txt', 'r');
  const b1 = Buffer.alloc(5);
  const b2 = Buffer.alloc(20); // larger than remaining → partial → break
  const r = await handle.readv([b1, b2], 0);
  assert.strictEqual(b1.toString(), 'hello');
  assert.strictEqual(r.bytesRead, 11);
  await handle.close();

  // writev with explicit position
  const wh = await myVfs.provider.open('/wv.txt', 'w+');
  await wh.writev([Buffer.from('AB'), Buffer.from('CD')], 0);
  await wh.close();
  assert.strictEqual(myVfs.readFileSync('/wv.txt', 'utf8'), 'ABCD');

  // appendFile with string + encoding option
  const ah = await myVfs.provider.open('/ap.txt', 'a+');
  await ah.appendFile('hello', { encoding: 'utf8' });
  await ah.close();
  assert.strictEqual(myVfs.readFileSync('/ap.txt', 'utf8'), 'hello');
})().then(common.mustCall());

// #checkReadable: 'w' mode rejects reads with EBADF
(async () => {
  const handle = await myVfs.provider.open('/wonly.txt', 'w');
  assert.throws(() => handle.readSync(Buffer.alloc(1), 0, 1, 0), { code: 'EBADF' });
  await assert.rejects(handle.read(Buffer.alloc(1), 0, 1, 0), { code: 'EBADF' });
  assert.throws(() => handle.readFileSync(), { code: 'EBADF' });
  await assert.rejects(handle.readFile(), { code: 'EBADF' });
  await handle.close();
})().then(common.mustCall());

// #checkWritable: 'r' mode rejects writes with EBADF
(async () => {
  myVfs.writeFileSync('/ronly.txt', 'x');
  const handle = await myVfs.provider.open('/ronly.txt', 'r');
  assert.throws(() => handle.writeSync(Buffer.from('y'), 0, 1, 0), { code: 'EBADF' });
  await assert.rejects(handle.write(Buffer.from('y'), 0, 1, 0), { code: 'EBADF' });
  assert.throws(() => handle.writeFileSync('y'), { code: 'EBADF' });
  await assert.rejects(handle.writeFile('y'), { code: 'EBADF' });
  assert.throws(() => handle.truncateSync(0), { code: 'EBADF' });
  await assert.rejects(handle.truncate(0), { code: 'EBADF' });
  await handle.close();
})().then(common.mustCall());

// writeFileSync with string + encoding
(async () => {
  const handle = await myVfs.provider.open('/se.txt', 'w+');
  await handle.writeFile('héllo', { encoding: 'utf8' });
  const got = await handle.readFile('utf8');
  assert.strictEqual(got, 'héllo');
  await handle.close();
})().then(common.mustCall());

// truncateSync extending past current size
(async () => {
  const handle = await myVfs.provider.open('/grow.txt', 'w+');
  await handle.writeFile('abc');
  await handle.truncate(10);
  const stats = await handle.stat();
  assert.strictEqual(stats.size, 10);
  // Content has zero-filled extension
  const data = await handle.readFile();
  assert.strictEqual(data.length, 10);
  await handle.close();
})().then(common.mustCall());

// MemoryFileHandle without a #getStats callback throws ERR_INVALID_STATE
{
  const { MemoryFileHandle } = require('internal/vfs/file_handle');
  // Pass undefined as getStats — entry is null so the dynamic-content
  // branches don't trigger; statSync falls into the "stats not available"
  // path.
  const h = new MemoryFileHandle('/x', 'r', 0o644, Buffer.alloc(0), null, undefined);
  assert.throws(() => h.statSync(), { code: 'ERR_INVALID_STATE' });
}

// readv on closed handle rejects EBADF
(async () => {
  const handle = await myVfs.provider.open('/file.txt', 'r');
  await handle.close();
  await assert.rejects(handle.readv([Buffer.alloc(1)], 0), { code: 'EBADF' });
  await assert.rejects(handle.writev([Buffer.alloc(1)], 0), { code: 'EBADF' });
  await assert.rejects(handle.appendFile('x'), { code: 'EBADF' });
})().then(common.mustCall());
