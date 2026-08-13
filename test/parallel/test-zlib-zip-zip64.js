'use strict';

require('../common');

const assert = require('node:assert');
const zlib = require('node:zlib');
const { test } = require('node:test');

const ZIP64_EOCD_SIGNATURE = Buffer.from([0x50, 0x4b, 0x06, 0x06]);

async function buildArchive(count) {
  const entries = [];
  for (let i = 0; i < count; i++) {
    entries.push(await zlib.ZipEntry.create(`entry-${i}`, Buffer.alloc(0), { method: 'store' }));
  }
  const chunks = [];
  for await (const chunk of zlib.createZipArchive(entries)) chunks.push(chunk);
  return Buffer.concat(chunks);
}

test('an entry count at or above 0xFFFF forces Zip64 structures', async () => {
  const archive = await buildArchive(0x10000);
  assert.ok(archive.includes(ZIP64_EOCD_SIGNATURE));

  const read = [...zlib.ZipEntry.read(archive)];
  assert.strictEqual(read.length, 0x10000);

  using zip = new zlib.ZipBuffer(archive);
  assert.strictEqual(zip.size, 0x10000);
  assert.strictEqual(zip.has('entry-0'), true);
  assert.strictEqual(zip.has(`entry-${0x10000 - 1}`), true);
}, { timeout: 120_000 });

test('an archive below the Zip64 thresholds contains no Zip64 structures', async () => {
  const archive = await buildArchive(10);
  assert.ok(!archive.includes(ZIP64_EOCD_SIGNATURE));
  assert.strictEqual([...zlib.ZipEntry.read(archive)].length, 10);
});
