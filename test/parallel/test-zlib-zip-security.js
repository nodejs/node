'use strict';

// Regression tests for the security review of the ZIP archive API. Each test
// encodes the *desired* safe behaviour; on the pre-fix code it fails, exposing
// the issue. See the review for context.

const common = require('../common');

const assert = require('node:assert');
const zlib = require('node:zlib');
const fs = require('node:fs');
const fsp = require('node:fs/promises');
const path = require('node:path');
const tmpdir = require('../common/tmpdir');
const { test } = require('node:test');

tmpdir.refresh();

let seq = 0;
async function tempZip(entries) {
  const dir = await fsp.mkdtemp(path.join(tmpdir.path, `zip-sec-${seq++}-`));
  const chunks = [];
  for await (const chunk of zlib.createZipArchive(entries)) chunks.push(chunk);
  const filePath = path.join(dir, 'archive.zip');
  await fsp.writeFile(filePath, Buffer.concat(chunks));
  return { dir, filePath, cleanup: () => fsp.rm(dir, { recursive: true, force: true }) };
}

// -- Finding 1: no closed-state guard; operations after close() fall through
// to a raw (possibly reused) file descriptor. -------------------------------

test('post-close operations are rejected cleanly, not via a raw fd', async () => {
  const { dir, filePath, cleanup } = await tempZip(
    [await zlib.ZipEntry.create('a.txt', Buffer.from('secret-archive-data'))]);
  try {
    const zip = await zlib.ZipFile.open(filePath, { writable: true });
    const retained = await zip.get('a.txt');
    await zip.close();

    // Encourage descriptor reuse: open an unrelated "victim" file right after
    // close(), which typically reclaims the just-freed fd number.
    const victimPath = path.join(dir, 'victim.txt');
    const VICTIM = 'do-not-touch-this-victim-file';
    await fsp.writeFile(victimPath, VICTIM);
    const victimFd = fs.openSync(victimPath, 'r+');
    try {
      // A retained entry must not read through the (reused) descriptor.
      await assert.rejects(retained.content(), { code: 'ERR_INVALID_STATE' });
      // A post-close mutation must not truncate/overwrite whatever now owns
      // the fd number.
      await assert.rejects(
        zip.addEntry(await zlib.ZipEntry.create('b.txt', Buffer.from('x'))),
        { code: 'ERR_INVALID_STATE' });
      // A second close() must be a safe no-op, not a double-close of a reused fd.
      await zip.close();
      // The victim file must be byte-for-byte intact.
      assert.strictEqual(fs.readFileSync(victimPath, 'utf8'), VICTIM);
    } finally {
      fs.closeSync(victimFd);
    }
  } finally {
    await cleanup();
  }
});

test('post-close operations are rejected cleanly (sync)', async () => {
  const { dir, filePath, cleanup } = await tempZip(
    [await zlib.ZipEntry.create('a.txt', Buffer.from('secret-archive-data'))]);
  try {
    const zip = zlib.ZipFile.openSync(filePath, { writable: true });
    const retained = zip.getSync('a.txt');
    zip.closeSync();

    const victimPath = path.join(dir, 'victim.txt');
    const VICTIM = 'do-not-touch-this-victim-file';
    fs.writeFileSync(victimPath, VICTIM);
    const victimFd = fs.openSync(victimPath, 'r+');
    try {
      assert.throws(() => retained.contentSync(), { code: 'ERR_INVALID_STATE' });
      assert.throws(
        () => zip.addEntrySync(zlib.ZipEntry.createSync('b.txt', Buffer.from('x'))),
        { code: 'ERR_INVALID_STATE' });
      // A second closeSync() must be a safe no-op, not EBADF on a reused fd.
      zip.closeSync();
      assert.strictEqual(fs.readFileSync(victimPath, 'utf8'), VICTIM);
    } finally {
      fs.closeSync(victimFd);
    }
  } finally {
    await cleanup();
  }
});

// -- Finding 2: add() awaits ZipEntry.create() before registering the mutation,
// so a synchronous close() can slip in and close the fd underneath it. -------

test('closeSync() while an async add() is outstanding is rejected', async () => {
  const { filePath, cleanup } = await tempZip(
    [await zlib.ZipEntry.create('a.txt', Buffer.from('data'))]);
  try {
    const zip = await zlib.ZipFile.open(filePath, { writable: true });
    // add() runs synchronously up to `await ZipEntry.create(...)` and then
    // suspends, having registered nothing yet.
    const adding = zip.add('b.txt', Buffer.alloc(1 << 16, 7));
    // The mutation must already be reserved, so a synchronous method sees the
    // archive as busy instead of racing the descriptor.
    assert.throws(() => zip.closeSync(), { code: 'ERR_INVALID_STATE' });
    await adding;
    await zip.close();

    const check = await zlib.ZipFile.open(filePath);
    try {
      assert.ok(check.has('b.txt'), 'the added entry must survive');
    } finally {
      await check.close();
    }
  } finally {
    await cleanup();
  }
});

test('an earlier add() completes before a later close()', async () => {
  const { filePath, cleanup } = await tempZip(
    [await zlib.ZipEntry.create('a.txt', Buffer.from('data'))]);
  try {
    const zip = await zlib.ZipFile.open(filePath, { writable: true });
    const adding = zip.add('b.txt', Buffer.alloc(1 << 16, 9));
    const closing = zip.close();
    // add() was issued first; it must land before close() tears down the fd.
    await Promise.all([adding, closing]);

    const check = await zlib.ZipFile.open(filePath);
    try {
      assert.ok(check.has('b.txt'), 'the earlier add() must not be lost');
    } finally {
      await check.close();
    }
  } finally {
    await cleanup();
  }
});

// -- Finding 3 (root cause): zipFiles() classifies via lstat() and then
// re-opens the path with createReadStream(), so it neither validates the
// opened descriptor's type nor pins it against a swap. A non-regular file
// therefore slips through as an ordinary stream source. --------------------

test('zipFiles() rejects a non-regular special file', {
  skip: common.isWindows ? 'no /dev/null semantics on Windows' : false,
}, async () => {
  const chunks = [];
  await assert.rejects(
    (async () => {
      for await (const chunk of zlib.zipFiles([['/dev/null', 'null']])) chunks.push(chunk);
    })(),
    (err) => err?.code !== undefined && err.code !== 'ERR_ASSERTION',
    'archiving a character device should be rejected, not stored as an empty file');
});

// -- Concern B (verdict: by design): the default decompression ceiling guards
// the buffering path (content()) against a huge allocation; streaming
// (contentIterator / ZipFile.stream) is deliberately not bound by it, so a
// legitimately large member can be read chunk by chunk. Output is still capped
// per chunk at the declared size, and a caller wanting a cap passes maxSize.
// This locks in that asymmetry (a default ceiling on streaming would break
// multi-gigabyte reads - see test/pummel/test-zlib-zip-slow.js). -------------

test('the default ceiling bounds content() but not streaming', async () => {
  const chunks = [];
  for await (const c of zlib.createZipArchive(
    [await zlib.ZipEntry.create('big.txt', Buffer.alloc(4096, 1), { method: 'store' })])) {
    chunks.push(c);
  }
  using zip = new zlib.ZipBuffer(Buffer.concat(chunks));
  const entry = zip.get('big.txt');

  const saved = zlib.getMaxZipContentSize();
  zlib.setMaxZipContentSize(1024); // Below the entry's 4096 declared bytes.
  try {
    // One-shot buffering enforces the default ceiling...
    await assert.rejects(entry.content(), { code: 'ERR_ZIP_ENTRY_TOO_LARGE' });
    // ...but streaming (the bounded-memory path) is not capped by it.
    let total = 0;
    for await (const chunk of entry.contentIterator()) total += chunk.length;
    assert.strictEqual(total, 4096);
    // An explicit maxSize still caps streaming when the caller wants it.
    await assert.rejects((async () => {
      let n = 0;
      for await (const chunk of entry.contentIterator({ maxSize: 1024 })) n += chunk.length;
      return n;
    })(), { code: 'ERR_ZIP_ENTRY_TOO_LARGE' });
  } finally {
    zlib.setMaxZipContentSize(saved);
  }
});

// -- Concern C: when a Zip64 end record is present its values silently override
// the classic EOCD without checking they agree. A non-sentinel classic field
// that disagrees with Zip64 is a parser differential (a classic-only tool and
// Node see different archives) and must be rejected. --------------------------

test('open() rejects contradictory classic-EOCD vs Zip64 metadata', async () => {
  // 0x10000 entries forces Zip64; the classic total-records field becomes the
  // 0xFFFF overflow sentinel.
  const entries = [];
  for (let i = 0; i < 0x10000; i++) {
    entries.push(zlib.ZipEntry.createSync(`e${i}`, Buffer.alloc(0), { method: 'store' }));
  }
  const bytes = Buffer.concat([...zlib.createZipArchiveSync(entries)]);

  const eocd = bytes.lastIndexOf(Buffer.from([0x50, 0x4b, 0x05, 0x06]));
  assert.ok(eocd >= 0, 'classic EOCD present');
  // The classic total-records field must be the 0xFFFF overflow sentinel.
  assert.strictEqual(bytes.readUInt16LE(eocd + 10), 0xFFFF);
  // Rewrite the sentinel to a smaller, non-sentinel value that disagrees with
  // the Zip64 record (which says 0x10000).
  const tampered = Buffer.from(bytes);
  tampered.writeUInt16LE(3, eocd + 10);

  assert.throws(() => new zlib.ZipBuffer(tampered), { code: 'ERR_ZIP_INVALID_ARCHIVE' });

  const dir = await fsp.mkdtemp(path.join(tmpdir.path, `zip-sec-${seq++}-`));
  const p = path.join(dir, 'contradiction.zip');
  try {
    await fsp.writeFile(p, tampered);
    await assert.rejects(zlib.ZipFile.open(p), { code: 'ERR_ZIP_INVALID_ARCHIVE' });
    assert.throws(() => zlib.ZipFile.openSync(p), { code: 'ERR_ZIP_INVALID_ARCHIVE' });
  } finally {
    await fsp.rm(dir, { recursive: true, force: true });
  }
}, { timeout: 120_000 });

test('central-directory record count must account for its full declared size', async () => {
  const chunks = [];
  for await (const chunk of zlib.createZipArchive([
    await zlib.ZipEntry.create('visible.txt', Buffer.from('visible'), { method: 'store' }),
    await zlib.ZipEntry.create('hidden.txt', Buffer.from('hidden'), { method: 'store' }),
  ])) chunks.push(chunk);
  const tampered = Buffer.concat(chunks);
  const eocd = tampered.length - 22;
  assert.strictEqual(tampered.readUInt16LE(eocd + 8), 2);
  assert.strictEqual(tampered.readUInt16LE(eocd + 10), 2);

  // Keep the single-disk counts consistent with each other, but make both
  // disagree with the two complete records in the declared directory size.
  tampered.writeUInt16LE(1, eocd + 8);
  tampered.writeUInt16LE(1, eocd + 10);
  const expected = {
    code: 'ERR_ZIP_INVALID_ARCHIVE',
    message: /central directory record count is inconsistent with its size/,
  };

  assert.throws(() => [...zlib.ZipEntry.read(tampered)], expected);
  assert.throws(() => new zlib.ZipBuffer(tampered), expected);

  const dir = await fsp.mkdtemp(path.join(tmpdir.path, `zip-sec-${seq++}-`));
  const p = path.join(dir, 'record-count-mismatch.zip');
  try {
    await fsp.writeFile(p, tampered);
    await assert.rejects(zlib.ZipFile.open(p), expected);
    assert.throws(() => zlib.ZipFile.openSync(p), expected);
  } finally {
    await fsp.rm(dir, { recursive: true, force: true });
  }
});

// -- Finding 4: the file-backed open-time overlap check uses a 30-byte lower
// bound for each local header, while the in-memory reader uses the exact
// local-header length. A crafted "quoted overlap" archive therefore passes
// ZipFile.open() but is (correctly) rejected by ZipBuffer. -----------------

test('ZipFile.open() enforces the same member-overlap check as ZipBuffer', async () => {
  const chunks = [];
  for await (const chunk of zlib.createZipArchive([
    await zlib.ZipEntry.create('A', Buffer.alloc(40, 1), { method: 'store' }),
    await zlib.ZipEntry.create('B', Buffer.alloc(40, 2), { method: 'store' }),
  ])) chunks.push(chunk);
  const tampered = Buffer.concat(chunks);
  // Enlarge entry A's *local* extra-field length. Its real data range (which
  // the read path locates via the local header) now runs past entry B's local
  // header - the quoted/overlapping zip-bomb shape. The central directory is
  // left untouched, so the offset+30+compressedSize open check still sees room.
  tampered.writeUInt16LE(tampered.readUInt16LE(28) + 8, 28);

  // The in-memory reader rejects it outright.
  assert.throws(() => new zlib.ZipBuffer(tampered), { code: 'ERR_ZIP_INVALID_ARCHIVE' });

  // The file-backed reader must reject it too, at open time - not silently
  // accept an archive whose members overlap.
  const dir = await fsp.mkdtemp(path.join(tmpdir.path, `zip-sec-${seq++}-`));
  const p = path.join(dir, 'overlap.zip');
  try {
    await fsp.writeFile(p, tampered);
    await assert.rejects(zlib.ZipFile.open(p), { code: 'ERR_ZIP_INVALID_ARCHIVE' });
    assert.throws(() => zlib.ZipFile.openSync(p), { code: 'ERR_ZIP_INVALID_ARCHIVE' });
  } finally {
    await fsp.rm(dir, { recursive: true, force: true });
  }
});
