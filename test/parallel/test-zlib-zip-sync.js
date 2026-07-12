'use strict';

require('../common');

const assert = require('node:assert');
const zlib = require('node:zlib');
const fs = require('node:fs');
const path = require('node:path');
const tmpdir = require('../common/tmpdir');
const { test } = require('node:test');

tmpdir.refresh();

function buildArchiveSync(entries, comment) {
  const chunks = [];
  for (const chunk of zlib.createZipArchiveSync(entries, comment)) chunks.push(chunk);
  return Buffer.concat(chunks);
}

function createTempZipSync(entries, comment) {
  const dir = fs.mkdtempSync(path.join(tmpdir.path, 'zlib-zip-sync-'));
  const filePath = path.join(dir, 'archive.zip');
  fs.writeFileSync(filePath, buildArchiveSync(entries, comment));
  return { filePath, cleanup: () => fs.rmSync(dir, { recursive: true, force: true }) };
}

// -- ZipEntry -----------------------------------------------------------------

test('ZipEntry.createSync()/contentSync() round-trip, deflate and store', () => {
  const deflateEntry = zlib.ZipEntry.createSync('a.txt', Buffer.from('a'.repeat(1000)));
  assert.strictEqual(deflateEntry.method, 8);
  assert.strictEqual(deflateEntry.contentSync().toString(), 'a'.repeat(1000));

  const storeEntry = zlib.ZipEntry.createSync('b.bin', Buffer.from([1, 2, 3]), { method: 'store' });
  assert.strictEqual(storeEntry.method, 0);
  assert.deepStrictEqual(storeEntry.contentSync(), Buffer.from([1, 2, 3]));

  // Matches the crc32/size that the async ZipEntry.create() would produce.
  const data = Buffer.from('some content to compare');
  assert.strictEqual(zlib.ZipEntry.createSync('x', data).crc32, zlib.crc32(data));
});

test('ZipEntry.createSync()/contentSync() round-trip, zstd', () => {
  const zstdEntry = zlib.ZipEntry.createSync('z.txt', Buffer.from('z'.repeat(1000)), { method: 'zstd' });
  assert.strictEqual(zstdEntry.method, 93);
  assert.strictEqual(zstdEntry.contentSync().toString(), 'z'.repeat(1000));
});

test('ZipEntry.contentSync() enforces maxSize and CRC verification like content()', () => {
  const entry = zlib.ZipEntry.createSync('a.txt', Buffer.from('hello world'), { method: 'store' });
  assert.throws(() => entry.contentSync({ maxSize: 1 }), { code: 'ERR_ZIP_ENTRY_TOO_LARGE' });

  const archive = buildArchiveSync([entry]);
  const tampered = Buffer.from(archive);
  tampered[30 + 'a.txt'.length] ^= 0xff; // Flip a content byte.
  const [tamperedEntry] = zlib.ZipEntry.read(tampered);
  assert.throws(() => tamperedEntry.contentSync(), { code: 'ERR_ZIP_ENTRY_CORRUPT' });
  assert.strictEqual(tamperedEntry.contentSync({ verify: false }).length, 'hello world'.length);
});

test('createZipArchiveSync() throws for a streaming (pending) entry', () => {
  const streaming = zlib.ZipEntry.createStream('big.bin', (async function* () {})());
  assert.throws(() => [...zlib.createZipArchiveSync([streaming])], { code: 'ERR_INVALID_STATE' });
});

// -- ZipBuffer ----------------------------------------------------------------

test('ZipBuffer.addSync()/toBufferSync() round-trip', () => {
  const archive = buildArchiveSync([zlib.ZipEntry.createSync('a.txt', Buffer.from('a'))], 'a comment');
  using zip = new zlib.ZipBuffer(archive);
  const added = zip.addSync('b.txt', Buffer.from('b'));
  assert.strictEqual(added.name, 'b.txt');
  assert.strictEqual(zip.size, 2);

  const rebuilt = zip.toBufferSync();
  using reread = new zlib.ZipBuffer(rebuilt);
  assert.deepStrictEqual([...reread.keys()].sort(), ['a.txt', 'b.txt']);
  assert.strictEqual(reread.get('a.txt').contentSync().toString(), 'a');
  assert.strictEqual(reread.get('b.txt').contentSync().toString(), 'b');
  assert.strictEqual(reread.comment, 'a comment');
});

// -- ZipFile ------------------------------------------------------------------

test('ZipFile.openSync() reads the same entries as open()', async () => {
  const { filePath, cleanup } = createTempZipSync([
    zlib.ZipEntry.createSync('a.txt', Buffer.from('a')),
    zlib.ZipEntry.createSync('b.bin', Buffer.from([1, 2, 3]), { method: 'store' }),
  ]);
  try {
    const zip = zlib.ZipFile.openSync(filePath);
    try {
      assert.strictEqual(zip.writable, false);
      assert.strictEqual(zip.size, 2);
      assert.strictEqual(zip.getSync('a.txt').contentSync().toString(), 'a');
      assert.deepStrictEqual(zip.getSync('b.bin').contentSync(), Buffer.from([1, 2, 3]));
      assert.deepStrictEqual([...zip.valuesSync()].map((e) => e.name).sort(), ['a.txt', 'b.bin']);
      assert.deepStrictEqual([...zip.entriesSync()].map(([n]) => n).sort(), ['a.txt', 'b.bin']);
      const seen = [];
      zip.forEachSync((entry, name) => seen.push(name));
      assert.deepStrictEqual(seen.sort(), ['a.txt', 'b.bin']);
    } finally {
      zip.closeSync();
    }

    const asyncZip = await zlib.ZipFile.open(filePath);
    try {
      assert.deepStrictEqual([...asyncZip.keys()].sort(), ['a.txt', 'b.bin']);
    } finally {
      await asyncZip.close();
    }
  } finally {
    cleanup();
  }
});

test('ZipFile opened via openSync supports `using` (Symbol.dispose)', () => {
  const { filePath, cleanup } = createTempZipSync([zlib.ZipEntry.createSync('a.txt', Buffer.from('a'))]);
  try {
    {
      using zip = zlib.ZipFile.openSync(filePath);
      assert.strictEqual(zip.getSync('a.txt').contentSync().toString(), 'a');
    }
    // Disposed: the fd should be closed. Re-opening the same path must still work.
    const reopened = zlib.ZipFile.openSync(filePath);
    reopened.closeSync();
  } finally {
    cleanup();
  }
});

test('ZipFile.addEntrySync()/addSync()/deleteSync() alter the file synchronously', () => {
  const { filePath, cleanup } = createTempZipSync([
    zlib.ZipEntry.createSync('a.txt', Buffer.from('AAAA'.repeat(20))),
    zlib.ZipEntry.createSync('b.bin', Buffer.from([1, 2, 3]), { method: 'store' }),
  ]);
  try {
    const zip = zlib.ZipFile.openSync(filePath, { writable: true });
    try {
      assert.strictEqual(zip.writable, true);
      const sizeBefore = fs.statSync(filePath).size;
      const added = zip.addSync('c.txt', Buffer.from('a fresh member'));
      assert.strictEqual(added.name, 'c.txt');
      assert.strictEqual(zip.size, 3);
      assert.ok(fs.statSync(filePath).size > sizeBefore);
      assert.strictEqual(zip.getSync('c.txt').contentSync().toString(), 'a fresh member');
      // Original entries untouched.
      assert.strictEqual(zip.getSync('a.txt').contentSync().toString(), 'AAAA'.repeat(20));

      const entry = zlib.ZipEntry.createSync('d.txt', Buffer.from('d'));
      assert.strictEqual(zip.addEntrySync(entry), entry);

      const sizeBeforeDelete = fs.statSync(filePath).size;
      assert.strictEqual(zip.deleteSync('b.bin'), true);
      assert.ok(fs.statSync(filePath).size < sizeBeforeDelete);
      assert.strictEqual(zip.deleteSync('does-not-exist'), false);
    } finally {
      zip.closeSync();
    }

    const reread = zlib.ZipFile.openSync(filePath);
    try {
      assert.deepStrictEqual([...reread.keys()].sort(), ['a.txt', 'c.txt', 'd.txt']);
    } finally {
      reread.closeSync();
    }
  } finally {
    cleanup();
  }
});

test('ZipFile.addEntrySync() rejects a pending streaming entry', () => {
  const { filePath, cleanup } = createTempZipSync([zlib.ZipEntry.createSync('a.txt', Buffer.from('a'))]);
  try {
    const zip = zlib.ZipFile.openSync(filePath, { writable: true });
    try {
      const streaming = zlib.ZipEntry.createStream('big.bin', (async function* () {})());
      assert.throws(() => zip.addEntrySync(streaming), { code: 'ERR_INVALID_STATE' });
    } finally {
      zip.closeSync();
    }
  } finally {
    cleanup();
  }
});

test('ZipFile sync mutators throw ERR_ZIP_NOT_WRITABLE when not opened writable', () => {
  const { filePath, cleanup } = createTempZipSync([zlib.ZipEntry.createSync('a.txt', Buffer.from('a'))]);
  try {
    const zip = zlib.ZipFile.openSync(filePath);
    try {
      assert.throws(() => zip.addSync('b.txt', Buffer.from('b')), { code: 'ERR_ZIP_NOT_WRITABLE' });
      assert.throws(() => zip.deleteSync('a.txt'), { code: 'ERR_ZIP_NOT_WRITABLE' });
    } finally {
      zip.closeSync();
    }
  } finally {
    cleanup();
  }
});

test('ZipFile.compactSync() matches compact() and does not touch the open file', () => {
  const { filePath, cleanup } = createTempZipSync([
    zlib.ZipEntry.createSync('a.txt', Buffer.from('a'.repeat(1000))),
    zlib.ZipEntry.createSync('b.txt', Buffer.from('b'.repeat(1000))),
  ]);
  try {
    const zip = zlib.ZipFile.openSync(filePath, { writable: true });
    try {
      zip.deleteSync('b.txt');
      const sizeWithDeadSpace = fs.statSync(filePath).size;
      const compacted = zip.compactSync();
      assert.strictEqual(fs.statSync(filePath).size, sizeWithDeadSpace);
      assert.ok(compacted.length < sizeWithDeadSpace);
      using reread = new zlib.ZipBuffer(compacted);
      assert.deepStrictEqual([...reread.keys()], ['a.txt']);
    } finally {
      zip.closeSync();
    }
  } finally {
    cleanup();
  }
});

test('a sync ZipFile method throws while an async mutation has not settled yet', async () => {
  const { filePath, cleanup } = createTempZipSync([zlib.ZipEntry.createSync('seed.txt', Buffer.from('seed'))]);
  try {
    const zip = await zlib.ZipFile.open(filePath, { writable: true });
    try {
      // addEntry() (unlike add()) increments the busy counter synchronously,
      // at call time - before any internal awaiting - so the checks below
      // are guaranteed to observe the archive as busy.
      const entry = zlib.ZipEntry.createSync('slow.txt', Buffer.from('x'));
      const pending = zip.addEntry(entry);
      assert.throws(() => zip.getSync('seed.txt'), { code: 'ERR_INVALID_STATE' });
      assert.throws(() => zip.addSync('y.txt', Buffer.from('y')), { code: 'ERR_INVALID_STATE' });
      assert.throws(() => zip.closeSync(), { code: 'ERR_INVALID_STATE' });
      await pending;
      // Once settled, sync methods work again.
      assert.strictEqual(zip.getSync('seed.txt').contentSync().toString(), 'seed');
    } finally {
      await zip.close();
    }
  } finally {
    cleanup();
  }
});
