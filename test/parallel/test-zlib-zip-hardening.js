'use strict';

require('../common');

const assert = require('node:assert');
const zlib = require('node:zlib');
const { test } = require('node:test');

async function buildArchive(entries, comment) {
  const chunks = [];
  for await (const chunk of zlib.createZipArchive(entries, comment)) chunks.push(chunk);
  return Buffer.concat(chunks);
}

function buildEocd({ diskNumber = 0, cdDiskNumber = 0, cdDiskRecords = 0,
                     totalRecords = 0, cdSize = 0, cdOffset = 0, comment = Buffer.alloc(0) } = {}) {
  const buf = Buffer.allocUnsafe(22 + comment.length);
  buf.writeUInt32LE(0x06054b50, 0);
  buf.writeUInt16LE(diskNumber, 4);
  buf.writeUInt16LE(cdDiskNumber, 6);
  buf.writeUInt16LE(cdDiskRecords, 8);
  buf.writeUInt16LE(totalRecords, 10);
  buf.writeUInt32LE(cdSize, 12);
  buf.writeUInt32LE(cdOffset, 16);
  buf.writeUInt16LE(comment.length, 20);
  comment.copy(buf, 22);
  return buf;
}

test('an empty or tiny buffer is rejected as an invalid archive', () => {
  for (const buf of [Buffer.alloc(0), Buffer.alloc(10), Buffer.from('not a zip')]) {
    assert.throws(() => [...zlib.ZipEntry.read(buf)], { code: 'ERR_ZIP_INVALID_ARCHIVE' });
  }
});

test('garbage or truncated data is rejected', () => {
  const garbage = Buffer.alloc(100, 0x41);
  assert.throws(() => [...zlib.ZipEntry.read(garbage)], { code: 'ERR_ZIP_INVALID_ARCHIVE' });

  const truncated = buildEocd({ totalRecords: 5, cdSize: 46 * 5 }).subarray(0, 10);
  assert.throws(() => [...zlib.ZipEntry.read(truncated)], { code: 'ERR_ZIP_INVALID_ARCHIVE' });
});

test('an EOCD-looking signature inside a trailing comment is not mistaken for the real one', async () => {
  const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('hi'), { method: 'store' });
  // The comment scan walks backward through the trailing comment bytes
  // before it reaches the genuine EOCD signature; embedding 4 bytes that
  // look like one partway through must not be mistaken for the real record.
  const fakeSignature = String.fromCharCode(0x50, 0x4b, 0x05, 0x06);
  const archive = await buildArchive([entry], `before ${fakeSignature} after`);

  const read = [...zlib.ZipEntry.read(archive)];
  assert.strictEqual(read.length, 1);
  assert.strictEqual(read[0].name, 'f.txt');
});

test('a declared-size mismatch is rejected as corrupt', async () => {
  const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('hello world'), { method: 'store' });
  const archive = await buildArchive([entry]);

  // Shrink the *declared* uncompressed size in the central directory record
  // without touching the stored bytes themselves, so the amount of data
  // produced no longer matches what the header promised.
  const tampered = Buffer.from(archive);
  const centralHeaderStart = 30 + 'f.txt'.length + 'hello world'.length;
  const uncompressedSizeOffset = centralHeaderStart + 24;
  tampered.writeUInt32LE(1, uncompressedSizeOffset);

  const [tamperedEntry] = zlib.ZipEntry.read(tampered);
  assert.strictEqual(tamperedEntry.size, 1);
  await assert.rejects(tamperedEntry.content(), { code: 'ERR_ZIP_ENTRY_CORRUPT' });
});

test('CRC-32 verification catches a single flipped byte, and can be disabled', async () => {
  const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('hello world'), { method: 'store' });
  const archive = await buildArchive([entry]);
  const tampered = Buffer.from(archive);
  const contentStart = 30 + 'f.txt'.length;
  tampered[contentStart] ^= 0xff;

  const [tamperedEntry] = zlib.ZipEntry.read(tampered);
  await assert.rejects(tamperedEntry.content(), { code: 'ERR_ZIP_ENTRY_CORRUPT' });
  const unverified = await tamperedEntry.content({ verify: false });
  assert.strictEqual(unverified.length, 'hello world'.length);
});

test('content() enforces maxSize before allocating', async () => {
  const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('hello world'));
  await assert.rejects(entry.content({ maxSize: 1 }), { code: 'ERR_ZIP_ENTRY_TOO_LARGE' });
});

test('a forged small header whose content inflates past its declared size is rejected', async () => {
  // The up-front maxSize check trusts the declared size, so a bomb forges a
  // tiny declared size to clear it; the decompressor's maxOutputLength
  // backstop is capped at declared + 1 bytes, so the lie is caught (as
  // corruption - the declared size is provably wrong) without ever
  // materializing more than the declared size, no matter how large maxSize
  // is.
  for (const { method, re } of [
    { method: 'deflate', re: /inflates beyond its declared size/ },
    { method: 'zstd', re: /decompresses beyond its declared size/ },
  ]) {
    const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('x'.repeat(5000)), { method });
    const archive = await buildArchive([entry]);
    const tampered = Buffer.from(archive);
    const eocd = tampered.length - 22; // No comment, so EOCD is the last 22 bytes
    const cdOffset = tampered.readUInt32LE(eocd + 16);
    tampered.writeUInt32LE(50, cdOffset + 24); // Forge declared uncompressedSize

    const [e] = zlib.ZipEntry.read(tampered);
    assert.strictEqual(e.size, 50); // 50 <= maxSize 100 clears the up-front check
    await assert.rejects(e.content({ maxSize: 100 }), { code: 'ERR_ZIP_ENTRY_CORRUPT', message: re });
    assert.throws(() => e.contentSync({ maxSize: 100 }), { code: 'ERR_ZIP_ENTRY_CORRUPT', message: re });
  }
});

test('getMaxZipContentSize()/setMaxZipContentSize() control the default guard', async () => {
  const original = zlib.getMaxZipContentSize();
  try {
    zlib.setMaxZipContentSize(1);
    const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('hello world'));
    await assert.rejects(entry.content(), { code: 'ERR_ZIP_ENTRY_TOO_LARGE' });
  } finally {
    zlib.setMaxZipContentSize(original);
  }
  assert.strictEqual(zlib.getMaxZipContentSize(), original);
});

test('streaming a partially-consumed entry does not hang or leak', async () => {
  async function* source() {
    for (let i = 0; i < 1000; i++) {
      yield Buffer.alloc(1024, i & 0xff);
    }
  }
  const entry = zlib.ZipEntry.createStream('big.bin', source());
  let count = 0;
  let bytesSeen = 0;
  for await (const chunk of entry) {
    count++;
    bytesSeen += chunk.length;
    if (count > 2) break;
  }
  assert.ok(count > 2);
  assert.ok(bytesSeen > 0);
});

test('an overlong file name is rejected', async () => {
  await assert.rejects(
    zlib.ZipEntry.create('x'.repeat(70000), Buffer.alloc(0)),
    { code: 'ERR_ZIP_ENTRY_TOO_LARGE' },
  );
});

test('an empty file name is rejected', async () => {
  await assert.rejects(
    zlib.ZipEntry.create('', Buffer.alloc(0)),
    { code: 'ERR_INVALID_ARG_VALUE' },
  );
});

test('a multi-disk archive is rejected', () => {
  const eocd = buildEocd({ diskNumber: 1, cdDiskNumber: 1 });
  assert.throws(() => [...zlib.ZipEntry.read(eocd)], { code: 'ERR_ZIP_UNSUPPORTED_FEATURE' });
});

test('an encrypted entry is rejected', async () => {
  const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('secret'), { method: 'store' });
  const archive = await buildArchive([entry]);
  // Set the encrypted bit (bit 0) in both the local and central header flags.
  const tampered = Buffer.from(archive);
  const localFlagsOffset = 6;
  tampered.writeUInt16LE(tampered.readUInt16LE(localFlagsOffset) | 0x0001, localFlagsOffset);
  const centralHeaderStart = 30 + 'f.txt'.length + 'secret'.length;
  const centralFlagsOffset = centralHeaderStart + 8;
  tampered.writeUInt16LE(tampered.readUInt16LE(centralFlagsOffset) | 0x0001, centralFlagsOffset);

  const [tamperedEntry] = zlib.ZipEntry.read(tampered);
  await assert.rejects(tamperedEntry.content(), { code: 'ERR_ZIP_UNSUPPORTED_FEATURE' });
});

test('an entry using an unsupported compression method is rejected', async () => {
  const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('hi'), { method: 'store' });
  const archive = await buildArchive([entry]);
  // Set the method to 1 (Shrunk), which this implementation does not support,
  // in both the local and central headers.
  const tampered = Buffer.from(archive);
  const localMethodOffset = 8;
  tampered.writeUInt16LE(1, localMethodOffset);
  const centralHeaderStart = 30 + 'f.txt'.length + 'hi'.length;
  const centralMethodOffset = centralHeaderStart + 10;
  tampered.writeUInt16LE(1, centralMethodOffset);

  const [tamperedEntry] = zlib.ZipEntry.read(tampered);
  assert.strictEqual(tamperedEntry.method, 1);
  await assert.rejects(tamperedEntry.content(), { code: 'ERR_ZIP_UNSUPPORTED_FEATURE' });
  assert.throws(() => tamperedEntry.contentSync(), { code: 'ERR_ZIP_UNSUPPORTED_FEATURE' });
});

// -- shapes borrowed from CPython's zipfile test corpus (synthesized bytes) ------

test('every possible truncation of an archive is rejected, deterministically', async () => {
  // CPython's test_damaged_zipfile: any prefix of a valid archive must fail
  // cleanly (never succeed, never throw a non-Error). The fuzz test samples
  // truncation points randomly; this pins all of them for a small archive.
  const archive = await buildArchive(
    [await zlib.ZipEntry.create('f.txt', Buffer.from('hello world'), { method: 'store' })]);
  for (let n = 0; n < archive.length; n++) {
    assert.throws(() => [...zlib.ZipEntry.read(archive.subarray(0, n))],
                  { code: /^ERR_ZIP_/ }, `truncation at ${n} bytes`);
  }
});

test('trailing padding after the EOCD is tolerated', async () => {
  // Some streaming writers pad their output to a block size; CPython
  // tolerates trailing newlines/NULs and so does the pass-2 EOCD scan.
  const archive = await buildArchive(
    [await zlib.ZipEntry.create('f.txt', Buffer.from('hi'), { method: 'store' })]);
  const padded = Buffer.concat([archive, Buffer.from('\r\n\0\0\0')]);
  const [entry] = zlib.ZipEntry.read(padded);
  assert.strictEqual(entry.name, 'f.txt');
  assert.strictEqual((await entry.content()).toString(), 'hi');
});

test('junk appended past a declared comment is tolerated and the comment preserved', async () => {
  const archive = await buildArchive(
    [await zlib.ZipEntry.create('f.txt', Buffer.from('hi'), { method: 'store' })],
    'this is a comment');
  const appended = Buffer.concat([archive, Buffer.from('abcdef\r\n')]);
  const zip = new zlib.ZipBuffer(appended);
  assert.strictEqual(zip.comment, 'this is a comment');
  assert.strictEqual(zip.get('f.txt').contentSync().toString(), 'hi');
});

test('an EOCD comment length overrunning the end of the file is rejected', async () => {
  // CPython's _EndRecData silently returns a truncated comment here; this
  // implementation deliberately rejects the candidate instead (both scan
  // passes require the declared comment to fit), so the archive has no
  // recognizable EOCD at all.
  const comment = 'padding-padding-padding';
  const archive = Buffer.from(await buildArchive([], comment));
  const eocdOffset = archive.length - 22 - comment.length;
  assert.strictEqual(archive.readUInt32LE(eocdOffset), 0x06054b50);
  archive.writeUInt16LE(comment.length + 10, eocdOffset + 20);
  assert.throws(() => [...zlib.ZipEntry.read(archive)],
                { code: 'ERR_ZIP_INVALID_ARCHIVE', message: /no end of central directory/ });
});

test('a central directory that cannot fit before the EOCD is rejected', () => {
  // CPython: "negative central directory offset". The declared size/offset
  // put the directory past the EOCD, so the prefix computation goes
  // negative.
  const eocd = buildEocd({ cdDiskRecords: 1, totalRecords: 1, cdSize: 1, cdOffset: 0xffffff });
  assert.throws(() => [...zlib.ZipEntry.read(eocd)],
                { code: 'ERR_ZIP_INVALID_ARCHIVE', message: /does not fit/ });
});

test('a record count inconsistent with the directory size is rejected', () => {
  // 100 records cannot fit in 46 bytes of central directory. Filler bytes
  // ahead of the EOCD stand in for that region so the directory nominally
  // fits inside the archive and the count check is what fires.
  const eocd = buildEocd({ cdDiskRecords: 100, totalRecords: 100, cdSize: 46 });
  const archive = Buffer.concat([Buffer.alloc(46), eocd]);
  assert.throws(() => [...zlib.ZipEntry.read(archive)],
                { code: 'ERR_ZIP_INVALID_ARCHIVE', message: /inconsistent/ });
});

test('a corrupted or overrunning central directory header is rejected', async () => {
  const nameA = 'a.txt';
  const nameB = 'b.txt';
  const content = Buffer.from('x');
  const two = await buildArchive([
    await zlib.ZipEntry.create(nameA, content, { method: 'store' }),
    await zlib.ZipEntry.create(nameB, content, { method: 'store' }),
  ]);
  const memberSize = 30 + nameA.length + content.length;
  const centralStart = 2 * memberSize;
  const centralRecord = 46 + nameA.length;

  // Zero the second central record's signature ("bad magic number").
  const badMagic = Buffer.from(two);
  badMagic.writeUInt32LE(0, centralStart + centralRecord);
  assert.throws(() => [...zlib.ZipEntry.read(badMagic)],
                { code: 'ERR_ZIP_INVALID_ARCHIVE', message: /signature is invalid/ });

  // Grow the first central record's name length so it overruns the declared
  // central directory size ("truncated central directory").
  const overrun = Buffer.from(two);
  overrun.writeUInt16LE(2000, centralStart + 28);
  assert.throws(() => [...zlib.ZipEntry.read(overrun)], { code: 'ERR_ZIP_INVALID_ARCHIVE' });
});

test('short or overrunning extra-field records do not make an entry unreadable', async () => {
  // CPython's test_zipfile_with_short_extra_field shape: advisory extra
  // metadata that is malformed is skipped, never fatal.
  const name = 'f.txt';
  const content = Buffer.from('hi');
  const base = await buildArchive(
    [await zlib.ZipEntry.create(name, content, { method: 'store' })]);
  const centralHeaderStart = 30 + name.length + content.length;
  for (const extra of [
    Buffer.from([0x99]), // Shorter than a TLV header
    Buffer.from([0x99, 0x99, 0xff]), // Still shorter than a TLV header
    Buffer.from([0x99, 0x99, 0xff, 0xff]), // Declared length overruns wildly
  ]) {
    const before = base.subarray(0, centralHeaderStart + 46 + name.length);
    const after = base.subarray(centralHeaderStart + 46 + name.length);
    const patched = Buffer.concat([before, extra, after]);
    patched.writeUInt16LE(extra.length, centralHeaderStart + 30);
    const eocdOffset = patched.length - 22;
    patched.writeUInt32LE(patched.readUInt32LE(eocdOffset + 12) + extra.length, eocdOffset + 12);
    const [entry] = zlib.ZipEntry.read(patched);
    assert.strictEqual(entry.name, name);
    assert.strictEqual((await entry.content()).toString(), 'hi');
  }
});

test('two central records quoting the same data are rejected as a possible zip bomb', async () => {
  // The "quoted overlap" bomb shape (CVE-2024-0450 in other
  // implementations): N central records pointing at one region turn a tiny
  // file into N full-size extractions.
  const nameA = 'a.txt';
  const nameB = 'b.txt';
  const content = Buffer.from('x');
  const two = Buffer.from(await buildArchive([
    await zlib.ZipEntry.create(nameA, content, { method: 'store' }),
    await zlib.ZipEntry.create(nameB, content, { method: 'store' }),
  ]));
  const memberSize = 30 + nameA.length + content.length;
  const centralStart = 2 * memberSize;
  const centralRecord = 46 + nameA.length;
  // Point the second record's local header at the first member's.
  two.writeUInt32LE(0, centralStart + centralRecord + 42);
  assert.throws(() => [...zlib.ZipEntry.read(two)],
                { code: 'ERR_ZIP_INVALID_ARCHIVE', message: /possible zip bomb/ });
});

test('a member whose data crosses into the central directory is rejected', async () => {
  const name = 'f.txt';
  const content = Buffer.from('hello');
  const archive = Buffer.from(await buildArchive(
    [await zlib.ZipEntry.create(name, content, { method: 'store' })]));
  const centralHeaderStart = 30 + name.length + content.length;
  // Lie about the compressed size so the member's data range reaches into
  // the central directory (while staying inside the buffer).
  archive.writeUInt32LE(content.length + 40, centralHeaderStart + 20);
  assert.throws(() => [...zlib.ZipEntry.read(archive)],
                { code: 'ERR_ZIP_INVALID_ARCHIVE', message: /possible zip bomb/ });
});

test('a Zip64 EOCD record size-field lie is tolerated when the locator is correct', () => {
  // The record's own size field is only consulted by the backward recovery
  // scan; when the locator points straight at a valid record, a lying size
  // field (CPython rejects 0 or 100 here) does not matter.
  const record = Buffer.alloc(56);
  record.writeUInt32LE(0x06064b50, 0);
  record.writeBigUInt64LE(100n, 4); // Lie: the remainder is 44 bytes
  record.writeUInt16LE((3 << 8) | 45, 12);
  record.writeUInt16LE(45, 14);
  const locator = Buffer.alloc(20);
  locator.writeUInt32LE(0x07064b50, 0);
  locator.writeBigUInt64LE(0n, 8);
  locator.writeUInt32LE(1, 16);
  const eocd = buildEocd({ cdSize: 0xffffffff });
  const zip = new zlib.ZipBuffer(Buffer.concat([record, locator, eocd]));
  assert.strictEqual(zip.size, 0);
});

test('a NUL byte inside an entry name is preserved verbatim', async () => {
  // CPython truncates the name at the NUL on read; this implementation
  // surfaces names verbatim and leaves interpretation to the caller.
  const name = 'foo\x00bar.txt';
  const archive = await buildArchive(
    [await zlib.ZipEntry.create(name, Buffer.from('x'), { method: 'store' })]);
  const [entry] = zlib.ZipEntry.read(archive);
  assert.strictEqual(entry.name, name);
});
