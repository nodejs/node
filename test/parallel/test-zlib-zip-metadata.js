'use strict';

require('../common');

const assert = require('node:assert');
const zlib = require('node:zlib');
const { test } = require('node:test');

async function roundTrip(options) {
  const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('x'), options);
  const chunks = [];
  for await (const chunk of zlib.createZipArchive([entry])) chunks.push(chunk);
  const archive = Buffer.concat(chunks);
  return [...zlib.ZipEntry.read(archive)][0];
}

test('modification time round-trips at 2-second resolution', async () => {
  const modified = new Date(2024, 5, 15, 13, 45, 30);
  const read = await roundTrip({ modified });
  assert.strictEqual(read.modified.getFullYear(), 2024);
  assert.strictEqual(read.modified.getMonth(), 5);
  assert.strictEqual(read.modified.getDate(), 15);
  assert.strictEqual(read.modified.getHours(), 13);
  assert.strictEqual(read.modified.getMinutes(), 45);
  assert.strictEqual(read.modified.getSeconds(), 30);
});

test('a sub-second modification time is preserved to the second via an extra field', async () => {
  // The DOS field is 2-second, local; a sub-second time additionally writes an
  // extended-timestamp extra field, so it round-trips to the exact UTC second.
  const read = await roundTrip({ modified: new Date(1700000000500) });
  assert.strictEqual(read.modified.getTime(), 1700000000000);
});

test('an odd whole-second modification time is preserved via an extra field', async () => {
  // The DOS fields have 2-second resolution, so an odd second is just as
  // unrepresentable as a sub-second part and also gets the extended-timestamp
  // extra field.
  const read = await roundTrip({ modified: new Date(1700000001000) });
  assert.strictEqual(read.modified.getTime(), 1700000001000);
});

test('a date before 1980 is clamped to the DOS epoch', async () => {
  const read = await roundTrip({ modified: new Date(1970, 0, 1) });
  assert.strictEqual(read.modified.getFullYear(), 1980);
  assert.strictEqual(read.modified.getMonth(), 0);
  assert.strictEqual(read.modified.getDate(), 1);
});

test('a date after 2107 is clamped to the DOS ceiling', async () => {
  const read = await roundTrip({ modified: new Date(2200, 0, 1) });
  assert.strictEqual(read.modified.getFullYear(), 2107);
  assert.strictEqual(read.modified.getMonth(), 11);
  assert.strictEqual(read.modified.getDate(), 31);
});

test('Unix mode round-trips for files and directories', async () => {
  const file = await roundTrip({ mode: 0o600 });
  assert.strictEqual(file.mode, 0o600);

  const dirEntry = await zlib.ZipEntry.create('dir/', Buffer.alloc(0), { mode: 0o700 });
  const chunks = [];
  for await (const chunk of zlib.createZipArchive([dirEntry])) chunks.push(chunk);
  const read = [...zlib.ZipEntry.read(Buffer.concat(chunks))][0];
  assert.strictEqual(read.mode, 0o700);
  assert.strictEqual(read.isDirectory, true);
});

test('default modes are applied when none is given', async () => {
  const file = await roundTrip({});
  assert.strictEqual(file.mode, 0o644);

  const dirEntry = await zlib.ZipEntry.create('dir/', Buffer.alloc(0));
  const chunks = [];
  for await (const chunk of zlib.createZipArchive([dirEntry])) chunks.push(chunk);
  const read = [...zlib.ZipEntry.read(Buffer.concat(chunks))][0];
  assert.strictEqual(read.mode, 0o755);
});

test('an entry comment round-trips', async () => {
  const read = await roundTrip({ comment: 'a comment' });
  assert.strictEqual(read.comment, 'a comment');
});

test('zipEntry.compressed reports compressed storage for any method', async () => {
  const big = Buffer.from('x'.repeat(1000)); // Compressible, so deflate/zstd win
  const deflated = await zlib.ZipEntry.create('d.txt', big, { method: 'deflate' });
  const stored = await zlib.ZipEntry.create('s.bin', Buffer.from([1, 2, 3]), { method: 'store' });
  const zstd = await zlib.ZipEntry.create('z.txt', big, { method: 'zstd' });
  assert.strictEqual(deflated.compressed, true);
  assert.strictEqual(stored.compressed, false);
  assert.strictEqual(zstd.compressed, true);

  // Survives a read-back round-trip too.
  assert.strictEqual((await roundTrip({ method: 'store' })).compressed, false);
});
