'use strict';

require('../common');

const assert = require('node:assert');
const zlib = require('node:zlib');
const fs = require('node:fs/promises');
const path = require('node:path');
const tmpdir = require('../common/tmpdir');
const { test } = require('node:test');

tmpdir.refresh();

async function buildArchive(entries) {
  const chunks = [];
  for await (const chunk of zlib.createZipArchive(entries)) chunks.push(chunk);
  return Buffer.concat(chunks);
}

async function collect(iterator) {
  const chunks = [];
  for await (const chunk of iterator) chunks.push(chunk);
  return Buffer.concat(chunks);
}

const text = Buffer.from('raw content '.repeat(500));

function decode(entry, raw) {
  switch (entry.method) {
    case 0: return raw;
    case 8: return zlib.inflateRawSync(raw);
    case 93: return zlib.zstdDecompressSync(raw);
  }
  assert.fail(`unexpected method ${entry.method}`);
}

async function assertRawReads(entry, expected) {
  const raw = await entry.rawContent();
  assert.strictEqual(raw.length, entry.compressedSize);
  assert.deepStrictEqual(decode(entry, raw), expected);
  assert.deepStrictEqual(entry.rawContentSync(), raw);
  assert.deepStrictEqual(await collect(entry.rawContentIterator()), raw);
}

test('in-memory entries expose their raw bytes for every method', async () => {
  for (const method of ['deflate', 'store', 'zstd']) {
    const entry = await zlib.ZipEntry.create(`${method}.txt`, text, { method });
    assert.strictEqual(entry.compressed, method !== 'store');
    await assertRawReads(entry, text);
  }
});

test('parsed and file-backed entries expose their raw bytes', async () => {
  const archive = await buildArchive([
    await zlib.ZipEntry.create('d.txt', text),
    await zlib.ZipEntry.create('s.txt', text, { method: 'store' }),
    await zlib.ZipEntry.create('z.txt', text, { method: 'zstd' }),
    await zlib.ZipEntry.create('empty.txt', Buffer.alloc(0)),
  ]);
  for (const entry of zlib.ZipEntry.read(archive)) {
    await assertRawReads(entry, entry.name === 'empty.txt' ? Buffer.alloc(0) : text);
  }

  const dir = await fs.mkdtemp(path.join(tmpdir.path, 'zlib-zip-raw-'));
  const filePath = path.join(dir, 'archive.zip');
  await fs.writeFile(filePath, archive);
  const zip = await zlib.ZipFile.open(filePath);
  let entry;
  try {
    for (const name of zip.keys()) {
      entry = await zip.get(name);
      await assertRawReads(entry, name === 'empty.txt' ? Buffer.alloc(0) : text);
    }
  } finally {
    await zip.close();
  }
  await assert.rejects(entry.rawContent(), { code: 'ERR_INVALID_STATE' });
  assert.throws(() => entry.rawContentSync(), { code: 'ERR_INVALID_STATE' });
  await assert.rejects(collect(entry.rawContentIterator()), { code: 'ERR_INVALID_STATE' });
});

test('rawContent() and rawContentSync() return caller-owned copies', async () => {
  const data = Buffer.from('stored bytes');
  const entry = await zlib.ZipEntry.create('s.txt', data, { method: 'store' });
  const raw = await entry.rawContent();
  raw.fill(0);
  const rawSync = entry.rawContentSync();
  assert.deepStrictEqual(rawSync, data);
  rawSync.fill(0);
  assert.deepStrictEqual(await entry.content(), data);
  assert.deepStrictEqual(data, Buffer.from('stored bytes'));
});

test('maxSize bounds the compressed size', async () => {
  const entry = await zlib.ZipEntry.create('s.txt', text, { method: 'store' });
  const limit = { maxSize: text.length - 1 };
  await assert.rejects(entry.rawContent(limit), { code: 'ERR_ZIP_ENTRY_TOO_LARGE' });
  assert.throws(() => entry.rawContentSync(limit), { code: 'ERR_ZIP_ENTRY_TOO_LARGE' });
  assert.throws(() => entry.rawContentIterator(limit), { code: 'ERR_ZIP_ENTRY_TOO_LARGE' });

  const exact = { maxSize: text.length };
  assert.deepStrictEqual(await entry.rawContent(exact), text);
  assert.deepStrictEqual(entry.rawContentSync(exact), text);
  assert.deepStrictEqual(await collect(entry.rawContentIterator(exact)), text);
});

test('a pending streaming entry has no raw content yet', async () => {
  async function* source() { yield text; }
  const entry = zlib.ZipEntry.createStream('s.txt', source());
  await assert.rejects(entry.rawContent(), { code: 'ERR_INVALID_STATE' });
  assert.throws(() => entry.rawContentSync(), { code: 'ERR_INVALID_STATE' });
  assert.throws(() => entry.rawContentIterator(), { code: 'ERR_INVALID_STATE' });
  await entry[Symbol.asyncDispose]();
});
