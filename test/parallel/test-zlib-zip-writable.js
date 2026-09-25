'use strict';

require('../common');

const assert = require('node:assert');
const zlib = require('node:zlib');
const fs = require('node:fs/promises');
const path = require('node:path');
const tmpdir = require('../common/tmpdir');
const { test } = require('node:test');

tmpdir.refresh();

async function buildArchive(entries, comment) {
  const chunks = [];
  for await (const chunk of zlib.createZipArchive(entries, comment)) chunks.push(chunk);
  return Buffer.concat(chunks);
}

async function createTempZip(entries, comment) {
  const dir = await fs.mkdtemp(path.join(tmpdir.path, 'zlib-zip-writable-'));
  const filePath = path.join(dir, 'archive.zip');
  await fs.writeFile(filePath, await buildArchive(entries, comment));
  return { filePath, cleanup: () => fs.rm(dir, { recursive: true, force: true }) };
}

// -- ZipBuffer --------------------------------------------------------------

test('ZipBuffer is always writable', async () => {
  const archive = await buildArchive([]);
  using zip = new zlib.ZipBuffer(archive);
  assert.strictEqual(zip.writable, true);
});

test('ZipBuffer preserves the archive comment across toBuffer()', async () => {
  const archive = await buildArchive([], 'original comment');
  using zip = new zlib.ZipBuffer(archive);
  assert.strictEqual(zip.comment, 'original comment');
  const rebuilt = await zip.toBuffer();
  using reread = new zlib.ZipBuffer(rebuilt);
  assert.strictEqual(reread.comment, 'original comment');
});

test('ZipBuffer add()/addEntry()/delete()/clear() mutate the in-memory index', async () => {
  const archive = await buildArchive([
    await zlib.ZipEntry.create('a.txt', Buffer.from('a')),
  ]);
  using zip = new zlib.ZipBuffer(archive);
  assert.strictEqual(zip.size, 1);

  const added = await zip.add('b.txt', Buffer.from('b'));
  assert.strictEqual(added.name, 'b.txt');
  assert.strictEqual(zip.size, 2);
  assert.strictEqual(zip.has('b.txt'), true);

  const entry = await zlib.ZipEntry.create('c.txt', Buffer.from('c'));
  assert.strictEqual(zip.addEntry(entry), entry);
  assert.strictEqual(zip.size, 3);

  assert.strictEqual(zip.delete('a.txt'), true);
  assert.strictEqual(zip.delete('a.txt'), false);
  assert.strictEqual(zip.size, 2);

  zip.clear();
  assert.strictEqual(zip.size, 0);
});

test('ZipBuffer.toBuffer() serializes the current live set, replacing on overwrite', async () => {
  const archive = await buildArchive([
    await zlib.ZipEntry.create('a.txt', Buffer.from('original')),
    await zlib.ZipEntry.create('b.txt', Buffer.from('keep')),
  ]);
  using zip = new zlib.ZipBuffer(archive);
  await zip.add('a.txt', Buffer.from('replaced'));
  zip.delete('b.txt');
  await zip.add('c.txt', Buffer.from('new'));

  const rebuilt = await zip.toBuffer();
  using reread = new zlib.ZipBuffer(rebuilt);
  assert.deepStrictEqual([...reread.keys()].sort(), ['a.txt', 'c.txt']);
  assert.strictEqual((await reread.get('a.txt').content()).toString(), 'replaced');
  assert.strictEqual((await reread.get('c.txt').content()).toString(), 'new');
});

test('ZipBuffer.addEntry() rejects a non-ZipEntry value', async () => {
  const archive = await buildArchive([]);
  using zip = new zlib.ZipBuffer(archive);
  assert.throws(() => zip.addEntry({ name: 'fake.txt' }), { code: 'ERR_INVALID_ARG_TYPE' });
});

// -- ZipFile ------------------------------------------------------------------

test('ZipFile defaults to read-only and rejects mutation', async () => {
  const { filePath, cleanup } = await createTempZip([await zlib.ZipEntry.create('a.txt', Buffer.from('a'))]);
  try {
    const zip = await zlib.ZipFile.open(filePath);
    try {
      assert.strictEqual(zip.writable, false);
      await assert.rejects(zip.add('b.txt', Buffer.from('b')), { code: 'ERR_ZIP_NOT_WRITABLE' });
      await assert.rejects(zip.delete('a.txt'), { code: 'ERR_ZIP_NOT_WRITABLE' });
    } finally {
      await zip.close();
    }
  } finally {
    await cleanup();
  }
});

test('ZipFile opened writable: addEntry() appends where the old CD used to be', async () => {
  const { filePath, cleanup } = await createTempZip([
    await zlib.ZipEntry.create('a.txt', Buffer.from('AAAA'.repeat(20))),
    await zlib.ZipEntry.create('b.bin', Buffer.from([1, 2, 3]), { method: 'store' }),
  ]);
  try {
    const sizeBefore = (await fs.stat(filePath)).size;
    const zip = await zlib.ZipFile.open(filePath, { writable: true });
    try {
      assert.strictEqual(zip.writable, true);
      const added = await zip.add('c.txt', Buffer.from('a fresh member'));
      assert.strictEqual(added.name, 'c.txt');
      assert.strictEqual(zip.size, 3);

      // The file must actually be altered by the time the call returns.
      const sizeAfter = (await fs.stat(filePath)).size;
      assert.ok(sizeAfter > sizeBefore);

      // Original entries must be untouched.
      assert.strictEqual((await (await zip.get('a.txt')).content()).toString(), 'AAAA'.repeat(20));
      assert.deepStrictEqual(await (await zip.get('b.bin')).content(), Buffer.from([1, 2, 3]));
      assert.strictEqual((await (await zip.get('c.txt')).content()).toString(), 'a fresh member');
    } finally {
      await zip.close();
    }

    // Re-opening from scratch must see the same three entries.
    const reread = await zlib.ZipFile.open(filePath);
    try {
      assert.deepStrictEqual([...reread.keys()].sort(), ['a.txt', 'b.bin', 'c.txt']);
    } finally {
      await reread.close();
    }
  } finally {
    await cleanup();
  }
});

test('ZipFile addEntry() promotes a spent streaming entry into a readable file-backed entry', async () => {
  const { filePath, cleanup } = await createTempZip([
    await zlib.ZipEntry.create('seed.txt', Buffer.from('seed')),
  ]);
  try {
    const zip = await zlib.ZipFile.open(filePath, { writable: true });
    try {
      const payload = 'streamed payload'.repeat(64);
      async function* source() {
        yield Buffer.from(payload.slice(0, 10));
        yield Buffer.from(payload.slice(10));
      }
      const streamEntry = zlib.ZipEntry.createStream('s.txt', source());

      // Before it is written, a streaming entry has no readable content.
      await assert.rejects(streamEntry.content(), { code: 'ERR_INVALID_STATE' });

      const returned = await zip.addEntry(streamEntry);
      // addEntry() returns the same object, now promoted in place.
      assert.strictEqual(returned, streamEntry);

      // The once-spent entry is now readable, both buffered and streamed.
      assert.strictEqual((await streamEntry.content()).toString(), payload);
      const chunks = [];
      for await (const chunk of streamEntry.contentIterator()) chunks.push(chunk);
      assert.strictEqual(Buffer.concat(chunks).toString(), payload);

      // And re-serializable: it can be copied into a fresh archive.
      const copy = new zlib.ZipBuffer(await buildArchive([streamEntry]));
      assert.strictEqual(copy.get('s.txt').contentSync().toString(), payload);
      assert.strictEqual(copy.get('s.txt').crc32 >>> 0, streamEntry.crc32 >>> 0);

      // An in-memory entry added alongside keeps its own buffer (not promoted).
      const mem = await zlib.ZipEntry.create('m.txt', Buffer.from('in memory'));
      await zip.addEntry(mem);
      assert.deepStrictEqual(mem.rawContent, mem.rawContent); // still a Buffer
      assert.notStrictEqual(mem.rawContent, null);
    } finally {
      await zip.close();
    }

    // The bytes persisted correctly and re-open sees the streamed member.
    const reread = await zlib.ZipFile.open(filePath);
    try {
      assert.strictEqual(
        (await (await reread.get('s.txt')).content()).toString(),
        'streamed payload'.repeat(64));
    } finally {
      await reread.close();
    }
  } finally {
    await cleanup();
  }
});

test('an in-place rewrite keeps central and local data-descriptor flags in agreement', async () => {
  const { filePath, cleanup } = await createTempZip([
    await zlib.ZipEntry.create('seed.txt', Buffer.from('seed')),
  ]);
  try {
    const zip = await zlib.ZipFile.open(filePath, { writable: true });
    try {
      async function* source() { yield Buffer.from('streamed payload'); }
      await zip.addEntry(zlib.ZipEntry.createStream('s.txt', source()));
      // A second mutation rebuilds the central directory from the freshly
      // adopted headers. The streamed entry's on-disk local header (bit 3
      // set, with a data descriptor after its content) is never rewritten,
      // so the rebuilt central header must keep advertising bit 3 - a
      // cleared flag would contradict the local header (sec. 4.3.12).
      await zip.delete('seed.txt');
    } finally {
      await zip.close();
    }

    const raw = await fs.readFile(filePath);
    const FLAG_DATA_DESCRIPTOR = 0x0008;
    // Only s.txt is live: its central record locates its local header.
    const centralPos = raw.indexOf(Buffer.from([0x50, 0x4b, 0x01, 0x02]));
    assert.notStrictEqual(centralPos, -1);
    const centralFlags = raw.readUInt16LE(centralPos + 8);
    const localOffset = raw.readUInt32LE(centralPos + 42);
    assert.strictEqual(raw.readUInt32LE(localOffset), 0x04034b50);
    const localFlags = raw.readUInt16LE(localOffset + 6);
    assert.strictEqual(localFlags & FLAG_DATA_DESCRIPTOR, FLAG_DATA_DESCRIPTOR);
    assert.strictEqual(centralFlags & FLAG_DATA_DESCRIPTOR, FLAG_DATA_DESCRIPTOR);

    // The archive stays fully readable after the rewrite.
    const reread = await zlib.ZipFile.open(filePath);
    try {
      assert.strictEqual(
        (await (await reread.get('s.txt')).content()).toString(), 'streamed payload');
    } finally {
      await reread.close();
    }
  } finally {
    await cleanup();
  }
});

test('ZipFile opened writable: delete() rewrites the CD without growing the file', async () => {
  const { filePath, cleanup } = await createTempZip([
    await zlib.ZipEntry.create('a.txt', Buffer.from('a')),
    await zlib.ZipEntry.create('b.txt', Buffer.from('b')),
  ]);
  try {
    const zip = await zlib.ZipFile.open(filePath, { writable: true });
    try {
      const sizeBefore = (await fs.stat(filePath)).size;
      assert.strictEqual(await zip.delete('b.txt'), true);
      const sizeAfter = (await fs.stat(filePath)).size;
      assert.ok(sizeAfter < sizeBefore);
      assert.strictEqual(zip.has('b.txt'), false);
      assert.strictEqual(zip.size, 1);
      assert.strictEqual(await zip.delete('does-not-exist'), false);
    } finally {
      await zip.close();
    }
  } finally {
    await cleanup();
  }
});

test('ZipFile.compact() streams a fresh archive with no dead entries and does not touch the open file', async () => {
  const { filePath, cleanup } = await createTempZip([
    await zlib.ZipEntry.create('a.txt', Buffer.from('a'.repeat(1000))),
    await zlib.ZipEntry.create('b.txt', Buffer.from('b'.repeat(1000))),
  ]);
  try {
    const zip = await zlib.ZipFile.open(filePath, { writable: true });
    try {
      await zip.delete('b.txt'); // Leaves dead space behind
      const sizeWithDeadSpace = (await fs.stat(filePath)).size;

      const chunks = [];
      for await (const chunk of zip.compact()) chunks.push(chunk);
      const compacted = Buffer.concat(chunks);

      // compact() must not have modified the still-open file.
      assert.strictEqual((await fs.stat(filePath)).size, sizeWithDeadSpace);

      assert.ok(compacted.length < sizeWithDeadSpace);
      using reread = new zlib.ZipBuffer(compacted);
      assert.deepStrictEqual([...reread.keys()], ['a.txt']);
      assert.strictEqual((await reread.get('a.txt').content()).toString(), 'a'.repeat(1000));
    } finally {
      await zip.close();
    }
  } finally {
    await cleanup();
  }
});

test('ZipFile preserves the archive comment across writes', async () => {
  const { filePath, cleanup } = await createTempZip(
    [await zlib.ZipEntry.create('a.txt', Buffer.from('a'))], 'a comment');
  try {
    const zip = await zlib.ZipFile.open(filePath, { writable: true });
    try {
      assert.strictEqual(zip.comment, 'a comment');
      await zip.add('b.txt', Buffer.from('b'));
      assert.strictEqual(zip.comment, 'a comment');
    } finally {
      await zip.close();
    }
    const reread = await zlib.ZipFile.open(filePath);
    try {
      assert.strictEqual(reread.comment, 'a comment');
    } finally {
      await reread.close();
    }
  } finally {
    await cleanup();
  }
});

test('ZipFile serializes concurrent add()/delete() calls instead of racing', async () => {
  const { filePath, cleanup } = await createTempZip([await zlib.ZipEntry.create('seed.txt', Buffer.from('seed'))]);
  try {
    const zip = await zlib.ZipFile.open(filePath, { writable: true });
    try {
      const names = [];
      for (let i = 0; i < 20; i++) names.push(`f${i}.txt`);
      await Promise.all(names.map((name) => zip.add(name, Buffer.from(name))));
      assert.strictEqual(zip.size, names.length + 1);
      for (const name of names) {
        assert.strictEqual((await (await zip.get(name)).content()).toString(), name);
      }
    } finally {
      await zip.close();
    }

    const reread = await zlib.ZipFile.open(filePath);
    try {
      assert.strictEqual(reread.size, 21);
    } finally {
      await reread.close();
    }
  } finally {
    await cleanup();
  }
}, { timeout: 30_000 });
