'use strict';

require('../common');

const assert = require('node:assert');
const tmpdir = require('../common/tmpdir');
const fs = require('node:fs/promises');
const zlib = require('node:zlib');
const { test } = require('node:test');

tmpdir.refresh();

// Additional zip.js coverage beyond the other test-zlib-zip-*.js files:
// DOS date/time edge cases, the Zip64 extra-field parser's normal and
// out-of-range paths, the "data was prepended to the archive" central
// directory recovery scan, buffer-coercion variants, entry-metadata
// validation, streaming-entry state guards, the ZipBuffer/ZipFile
// iteration protocols, and several ZipFile (on-disk) error paths that
// aren't reachable through ZipBuffer alone.

async function buildArchive(entries, comment) {
  const chunks = [];
  for await (const chunk of zlib.createZipArchive(entries, comment)) chunks.push(chunk);
  return Buffer.concat(chunks);
}

async function drain(iterable) {
  const chunks = [];
  for await (const chunk of iterable) chunks.push(chunk);
  return Buffer.concat(chunks);
}

// -- DOS date/time -----------------------------------------------------------

test('a zeroed DOS date/time field decodes to the 1980-01-01 epoch', async () => {
  const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('hi'), { method: 'store' });
  const archive = await buildArchive([entry]);
  const tampered = Buffer.from(archive);
  tampered.writeUInt16LE(0, 10); // local time
  tampered.writeUInt16LE(0, 12); // local date
  const centralStart = 30 + 'f.txt'.length + 'hi'.length;
  tampered.writeUInt16LE(0, centralStart + 12); // central time
  tampered.writeUInt16LE(0, centralStart + 14); // central date

  const [read] = zlib.ZipEntry.read(tampered);
  assert.strictEqual(read.modified.getTime(), new Date(1980, 0, 1, 0, 0, 0).getTime());
});

test('serializing an entry with an invalid modified Date is rejected', async () => {
  const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('hi'), { modified: new Date(NaN) });
  await assert.rejects(buildArchive([entry]), { code: 'ERR_INVALID_ARG_VALUE' });
});

// -- Zip64 structures ---------------------------------------------------------

function buildZip64Record({ diskNumber = 0, cdDiskNumber = 0, cdDiskRecords = 0n,
                            cdTotalRecords = 0n, cdSize = 0n, cdOffset = 0n } = {}) {
  const buf = Buffer.allocUnsafe(56);
  buf.writeUInt32LE(0x06064b50, 0);
  buf.writeBigUInt64LE(44n, 4);
  buf.writeUInt16LE((3 << 8) | 45, 12); // Made by Unix, version 4.5
  buf.writeUInt16LE(45, 14);
  buf.writeUInt32LE(diskNumber, 16);
  buf.writeUInt32LE(cdDiskNumber, 20);
  buf.writeBigUInt64LE(cdDiskRecords, 24);
  buf.writeBigUInt64LE(cdTotalRecords, 32);
  buf.writeBigUInt64LE(cdSize, 40);
  buf.writeBigUInt64LE(cdOffset, 48);
  return buf;
}

function buildZip64Locator({ recordDiskNumber = 0, recordOffset = 0n, totalDisks = 1 } = {}) {
  const buf = Buffer.allocUnsafe(20);
  buf.writeUInt32LE(0x07064b50, 0);
  buf.writeUInt32LE(recordDiskNumber, 4);
  buf.writeBigUInt64LE(recordOffset, 8);
  buf.writeUInt32LE(totalDisks, 16);
  return buf;
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

// A minimal (zero-entry) Zip64 archive: record + locator + classic EOCD,
// with the locator pointing directly at the record.
function buildMinimalZip64Archive({ record, locator, eocd } = {}) {
  return Buffer.concat([
    buildZip64Record(record),
    buildZip64Locator({ recordOffset: 0n, ...locator }),
    buildEocd(eocd),
  ]);
}

test('a well-formed minimal Zip64 archive round-trips its comment', () => {
  const buf = buildMinimalZip64Archive({ eocd: { comment: Buffer.from('hi') } });
  const zip = new zlib.ZipBuffer(buf);
  assert.strictEqual(zip.size, 0);
  assert.strictEqual(zip.comment, 'hi');
});

test('a Zip64 locator declaring more than one disk is rejected', () => {
  const buf = buildMinimalZip64Archive({ locator: { totalDisks: 2 } });
  assert.throws(() => [...zlib.ZipEntry.read(buf)], { code: 'ERR_ZIP_UNSUPPORTED_FEATURE' });
});

test('a Zip64 locator pointing at another disk is rejected', () => {
  const buf = buildMinimalZip64Archive({ locator: { recordDiskNumber: 1 } });
  assert.throws(() => [...zlib.ZipEntry.read(buf)], { code: 'ERR_ZIP_UNSUPPORTED_FEATURE' });
});

test('a Zip64 record on another disk is rejected', () => {
  const buf = buildMinimalZip64Archive({ record: { diskNumber: 1 } });
  assert.throws(() => [...zlib.ZipEntry.read(buf)], { code: 'ERR_ZIP_UNSUPPORTED_FEATURE' });
});

test('a Zip64 record with inconsistent disk record counts is rejected', () => {
  const buf = buildMinimalZip64Archive({ record: { cdDiskRecords: 1n, cdTotalRecords: 2n } });
  assert.throws(() => [...zlib.ZipEntry.read(buf)], { code: 'ERR_ZIP_UNSUPPORTED_FEATURE' });
});

test('a Zip64 record is found by scanning backward when data was prepended', () => {
  // The locator's declared offset is wrong (as if the archive had been
  // prepended with extra bytes, e.g. a self-extractor stub, after the
  // record/locator were written), but the record still physically sits
  // immediately before the locator; findArchiveEnd() must recover it.
  const buf = buildMinimalZip64Archive({
    locator: { recordOffset: 999_999n },
    eocd: { comment: Buffer.from('recovered') },
  });
  const zip = new zlib.ZipBuffer(buf);
  assert.strictEqual(zip.comment, 'recovered');
});

test('a required Zip64 record that cannot be found anywhere is rejected', () => {
  // An EOCD overflow sentinel makes the Zip64 record mandatory; with the
  // record's signature corrupted and the locator pointing nowhere useful,
  // the archive is unreadable.
  const buf = buildMinimalZip64Archive({
    locator: { recordOffset: 999_999n },
    eocd: { totalRecords: 0xffff, cdDiskRecords: 0xffff },
  });
  buf.writeUInt32LE(0xdeadbeef, 0); // Also corrupt the record actually present
  assert.throws(() => [...zlib.ZipEntry.read(buf)], {
    code: 'ERR_ZIP_INVALID_ARCHIVE',
    message: /Zip64 end of central directory record not found/,
  });
});

test('a missing Zip64 record falls back to valid classic EOCD fields', () => {
  // Same corrupted record, but no EOCD field carries an overflow sentinel:
  // the classic fields fully describe the (empty) archive, so the stray
  // locator signature - indistinguishable from comment bytes that happen to
  // contain it - must not reject an otherwise valid archive.
  const buf = buildMinimalZip64Archive({ locator: { recordOffset: 999_999n } });
  buf.writeUInt32LE(0xdeadbeef, 0);
  assert.strictEqual([...zlib.ZipEntry.read(buf)].length, 0);
});

// Patches a single-entry, single-record archive's central header to carry a
// synthetic Zip64 extra field for whichever of {uncompressedSize,
// compressedSize, localFileHeaderOffset, diskNumber} are provided, setting
// the corresponding 32-/16-bit field to the sentinel value that tells
// CentralFileHeader to resolve it from the extra field instead. A leading,
// unrelated TLV record is always included ahead of the Zip64 one, so the
// "skip past a foreign record" loop iteration is exercised too.
function injectZip64Extra(archive, name, contentLength, fields) {
  const centralHeaderStart = 30 + name.length + contentLength;
  const nameStart = centralHeaderStart + 46;
  const extraLengthOffset = centralHeaderStart + 30;
  assert.strictEqual(archive.readUInt16LE(extraLengthOffset), 0); // sanity: no existing extra

  const dummyTlv = Buffer.from([0x99, 0x99, 0x04, 0x00, 0xde, 0xad, 0xbe, 0xef]);
  const order = ['uncompressedSize', 'compressedSize', 'localFileHeaderOffset', 'diskNumber'];
  const dataLength = order.reduce(
    (total, key) => total + (key in fields ? (key === 'diskNumber' ? 4 : 8) : 0), 0);
  const zip64Tlv = Buffer.allocUnsafe(4 + dataLength);
  zip64Tlv.writeUInt16LE(0x0001, 0);
  zip64Tlv.writeUInt16LE(dataLength, 2);
  let pos = 4;
  for (const key of order) {
    if (!(key in fields)) continue;
    if (key === 'diskNumber') {
      zip64Tlv.writeUInt32LE(Number(fields[key]), pos);
      pos += 4;
    } else {
      zip64Tlv.writeBigUInt64LE(BigInt(fields[key]), pos);
      pos += 8;
    }
  }
  const extra = Buffer.concat([dummyTlv, zip64Tlv]);

  const before = archive.subarray(0, nameStart + name.length);
  const after = archive.subarray(nameStart + name.length); // comment + EOCD
  const patched = Buffer.concat([before, extra, after]);

  patched.writeUInt16LE(extra.length, extraLengthOffset);
  if ('uncompressedSize' in fields) patched.writeUInt32LE(0xffffffff, centralHeaderStart + 24);
  if ('compressedSize' in fields) patched.writeUInt32LE(0xffffffff, centralHeaderStart + 20);
  if ('localFileHeaderOffset' in fields) patched.writeUInt32LE(0xffffffff, centralHeaderStart + 42);
  if ('diskNumber' in fields) patched.writeUInt16LE(0xffff, centralHeaderStart + 34);

  const eocdOffset = patched.length - 22;
  assert.strictEqual(patched.readUInt32LE(eocdOffset), 0x06054b50);
  const oldCdSize = patched.readUInt32LE(eocdOffset + 12);
  patched.writeUInt32LE(oldCdSize + extra.length, eocdOffset + 12);

  return patched;
}

test('a foreign Zip64 extra field naming only the fields it needs still resolves', async () => {
  const name = 'f.txt';
  const content = Buffer.from('hello');
  const entry = await zlib.ZipEntry.create(name, content, { method: 'store' });
  const archive = await buildArchive([entry]);

  const patched = injectZip64Extra(archive, name, content.length, {
    uncompressedSize: content.length,
    compressedSize: content.length,
    localFileHeaderOffset: 0,
    diskNumber: 0,
  });

  const [read] = zlib.ZipEntry.read(patched);
  assert.strictEqual(read.size, content.length);
  assert.strictEqual(read.compressedSize, content.length);
  assert.strictEqual((await read.content()).toString(), 'hello');
});

test('a foreign Zip64 extra field carrying every field regardless of sentinels still resolves', async () => {
  const name = 'f.txt';
  const content = Buffer.from('hello');
  const entry = await zlib.ZipEntry.create(name, content, { method: 'store' });
  const archive = await buildArchive([entry]);

  // APPNOTE 4.5.3 says Zip64 fields MUST appear only for the classic fields
  // holding the overflow sentinel, but plenty of real writers emit all four
  // regardless. Only the compressed size is a sentinel here, so a strictly
  // packed parse would misread the uncompressed-size slot as the compressed
  // size; the parser must fall back to the full fixed layout instead.
  const centralHeaderStart = 30 + name.length + content.length;
  const nameStart = centralHeaderStart + 46;
  const tlv = Buffer.allocUnsafe(4 + 28);
  tlv.writeUInt16LE(0x0001, 0);
  tlv.writeUInt16LE(28, 2);
  tlv.writeBigUInt64LE(0x1111n, 4); // Uncompressed size (classic field wins)
  tlv.writeBigUInt64LE(BigInt(content.length), 12); // Compressed size
  tlv.writeBigUInt64LE(0n, 20); // Local header offset (classic field wins)
  tlv.writeUInt32LE(0, 28); // Disk number (classic field wins)
  const before = archive.subarray(0, nameStart + name.length);
  const after = archive.subarray(nameStart + name.length);
  const patched = Buffer.concat([before, tlv, after]);
  patched.writeUInt16LE(tlv.length, centralHeaderStart + 30);
  patched.writeUInt32LE(0xffffffff, centralHeaderStart + 20); // compressedSize sentinel only
  const eocdOffset = patched.length - 22;
  patched.writeUInt32LE(patched.readUInt32LE(eocdOffset + 12) + tlv.length, eocdOffset + 12);

  const [read] = zlib.ZipEntry.read(patched);
  assert.strictEqual(read.compressedSize, content.length);
  assert.strictEqual(read.size, content.length);
  assert.strictEqual((await read.content()).toString(), 'hello');

  // Re-serialization drops the (now stale) Zip64 record - Zip64 data is
  // regenerated from the final sizes - and the entry stays readable.
  const rewritten = await buildArchive([read]);
  const [reread] = zlib.ZipEntry.read(rewritten);
  assert.strictEqual((await reread.content()).toString(), 'hello');
});

test('a Zip64 extra field value beyond Number.MAX_SAFE_INTEGER is rejected', async () => {
  const name = 'f.txt';
  const content = Buffer.from('hello');
  const entry = await zlib.ZipEntry.create(name, content, { method: 'store' });
  const archive = await buildArchive([entry]);

  const patched = injectZip64Extra(archive, name, content.length, {
    uncompressedSize: 0xffffffffffffffffn,
  });

  const [read] = zlib.ZipEntry.read(patched);
  assert.throws(() => read.size, {
    code: 'ERR_ZIP_INVALID_ARCHIVE',
    message: /exceeds the safe integer range/,
  });
});

// Injects a raw, already-built Zip64 extra-field TLV (rather than one built
// via injectZip64Extra()'s field map), to exercise the parser's own
// malformed/truncated-input rejections.
function injectRawZip64Extra(archive, name, contentLength, extraBytes) {
  const centralHeaderStart = 30 + name.length + contentLength;
  const nameStart = centralHeaderStart + 46;
  const extraLengthOffset = centralHeaderStart + 30;
  assert.strictEqual(archive.readUInt16LE(extraLengthOffset), 0);
  const before = archive.subarray(0, nameStart + name.length);
  const after = archive.subarray(nameStart + name.length);
  const patched = Buffer.concat([before, extraBytes, after]);
  patched.writeUInt16LE(extraBytes.length, extraLengthOffset);
  patched.writeUInt32LE(0xffffffff, centralHeaderStart + 24); // uncompressedSize sentinel
  const eocdOffset = patched.length - 22;
  const oldCdSize = patched.readUInt32LE(eocdOffset + 12);
  patched.writeUInt32LE(oldCdSize + extraBytes.length, eocdOffset + 12);
  return patched;
}

test('a Zip64 extra-field TLV whose declared size overflows the extra field is rejected', async () => {
  const name = 'f.txt';
  const content = Buffer.from('hello');
  const entry = await zlib.ZipEntry.create(name, content, { method: 'store' });
  const archive = await buildArchive([entry]);

  const tlv = Buffer.allocUnsafe(8);
  tlv.writeUInt16LE(0x0001, 0);
  tlv.writeUInt16LE(100, 2); // claims 100 bytes of data, but none follow
  const patched = injectRawZip64Extra(archive, name, content.length, tlv);

  const [read] = zlib.ZipEntry.read(patched);
  assert.throws(() => read.size, {
    code: 'ERR_ZIP_INVALID_ARCHIVE',
    message: /extra field is malformed/,
  });
});

test('a Zip64 extra-field TLV too short for the field it claims to carry is rejected', async () => {
  const name = 'f.txt';
  const content = Buffer.from('hello');
  const entry = await zlib.ZipEntry.create(name, content, { method: 'store' });
  const archive = await buildArchive([entry]);

  // Declares only 4 bytes of data, but a sentinel uncompressedSize needs 8.
  const tlv = Buffer.allocUnsafe(8);
  tlv.writeUInt16LE(0x0001, 0);
  tlv.writeUInt16LE(4, 2);
  tlv.writeUInt32LE(123, 4);
  const patched = injectRawZip64Extra(archive, name, content.length, tlv);

  const [read] = zlib.ZipEntry.read(patched);
  assert.throws(() => read.size, {
    code: 'ERR_ZIP_INVALID_ARCHIVE',
    message: /Zip64 extended information extra field is truncated/,
  });
});

// -- buffer coercion -----------------------------------------------------------

test('create() accepts a DataView, a non-Uint8Array TypedArray, and an ArrayBuffer', async () => {
  const ab = new ArrayBuffer(4);
  new Uint8Array(ab).set([1, 2, 3, 4]);

  const dv = new DataView(ab, 1, 2);
  const fromDataView = await zlib.ZipEntry.create('dv.bin', dv);
  assert.strictEqual((await fromDataView.content()).length, 2);

  const i32 = new Int32Array([10, 20, 30]);
  const fromTypedArray = await zlib.ZipEntry.create('i32.bin', i32);
  assert.strictEqual((await fromTypedArray.content()).length, 12);

  const fromArrayBuffer = await zlib.ZipEntry.create('ab.bin', ab);
  assert.strictEqual((await fromArrayBuffer.content()).length, 4);
});

// -- entry-metadata validation -------------------------------------------------

test('create() validates comment length, modified type, and method value', async () => {
  await assert.rejects(
    zlib.ZipEntry.create('f.txt', Buffer.alloc(0), { comment: 'x'.repeat(70000) }),
    { code: 'ERR_ZIP_ENTRY_TOO_LARGE' },
  );
  await assert.rejects(
    zlib.ZipEntry.create('f.txt', Buffer.alloc(0), { modified: 123 }),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  await assert.rejects(
    zlib.ZipEntry.create('f.txt', Buffer.alloc(0), { method: 'bogus' }),
    { code: 'ERR_INVALID_ARG_VALUE' },
  );
});

test('a directory entry must have empty content, for create(), createSync(), and createStream()', async () => {
  await assert.rejects(
    zlib.ZipEntry.create('dir/', Buffer.from('x')), { code: 'ERR_INVALID_ARG_VALUE' });
  assert.throws(
    () => zlib.ZipEntry.createSync('dir/', Buffer.from('x')), { code: 'ERR_INVALID_ARG_VALUE' });
  assert.throws(
    () => zlib.ZipEntry.createStream('dir/', (async function* () {})()), { code: 'ERR_INVALID_ARG_VALUE' });
});

test('createZipArchive()/createZipArchiveSync() validate the archive comment length', async () => {
  await assert.rejects(drain(zlib.createZipArchive([], 'x'.repeat(70000))),
                       { code: 'ERR_ZIP_ARCHIVE_TOO_LARGE' });
  assert.throws(() => [...zlib.createZipArchiveSync([], 'x'.repeat(70000))],
                { code: 'ERR_ZIP_ARCHIVE_TOO_LARGE' });
});

// -- streaming-entry state guards ----------------------------------------------

test('a pending streaming entry rejects size/crc32/compressedSize/content access', () => {
  const streaming = zlib.ZipEntry.createStream(
    'big.bin', (async function* () { yield Buffer.from('x'); })());
  assert.throws(() => streaming.size, { code: 'ERR_INVALID_STATE' });
  assert.throws(() => streaming.crc32, { code: 'ERR_INVALID_STATE' });
  assert.throws(() => streaming.compressedSize, { code: 'ERR_INVALID_STATE' });
  assert.throws(() => streaming.contentIterator(), { code: 'ERR_INVALID_STATE' });
  assert.throws(() => streaming.contentSync(), { code: 'ERR_INVALID_STATE' });
});

// Once a streaming entry has been serialized on its own (via createZipArchive,
// not into a writable ZipFile that would promote it), its source is spent and
// there is nothing to read back. Reads must fail with a clean state error, not
// silently decode an empty buffer and report ERR_ZIP_ENTRY_CORRUPT.
test('a spent (serialized-but-unpromoted) streaming entry rejects reads cleanly', async () => {
  const entry = zlib.ZipEntry.createStream('s.txt', (async function* () { yield Buffer.from('hello'); })());
  await drain(entry);
  await assert.rejects(entry.content(), { code: 'ERR_INVALID_STATE' });
  assert.throws(() => entry.contentSync(), { code: 'ERR_INVALID_STATE' });
  assert.throws(() => entry.contentIterator(), { code: 'ERR_INVALID_STATE' });
});

test('a streaming entry can only be serialized once', async () => {
  const entry = zlib.ZipEntry.createStream('a.bin', (async function* () { yield Buffer.from('x'); })());
  await drain(entry);
  await assert.rejects(drain(entry), { code: 'ERR_INVALID_STATE' });
});

test('a streaming entry rejects a non-Uint8Array chunk from its source', async () => {
  async function* badSource() {
    yield Buffer.alloc(0); // An empty chunk is silently skipped
    yield 'not a buffer';
  }
  const entry = zlib.ZipEntry.createStream('b.bin', badSource());
  await assert.rejects(drain(entry), { code: 'ERR_INVALID_ARG_TYPE' });
});

test('a streaming entry can use zstd compression end-to-end', async () => {
  const payload = 'zstd stream content '.repeat(50);
  async function* source() { yield Buffer.from(payload); }
  const entry = zlib.ZipEntry.createStream('c.bin', source(), { method: 'zstd' });
  const archive = await buildArchive([entry]);

  const [read] = zlib.ZipEntry.read(archive);
  assert.strictEqual(read.method, 93);
  assert.strictEqual((await read.content()).toString(), payload);
});

test('an error from a streaming entry\'s source propagates and cleans up (deflate and zstd)', async () => {
  for (const method of ['deflate', 'zstd']) {
    async function* badSource() {
      yield Buffer.from('some data before the error');
      throw new Error(`source blew up (${method})`);
    }
    const entry = zlib.ZipEntry.createStream('big.bin', badSource(), { method });
    await assert.rejects(drain(entry), { message: `source blew up (${method})` });
  }
});

// -- contentIterator() / decodeMemberStream() error paths ------------------------

test('contentIterator() enforces the same guards as content() and contentSync()', async () => {
  // Encrypted.
  {
    const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('secret'), { method: 'store' });
    const archive = await buildArchive([entry]);
    const tampered = Buffer.from(archive);
    tampered.writeUInt16LE(tampered.readUInt16LE(6) | 0x0001, 6);
    const centralStart = 30 + 'f.txt'.length + 'secret'.length;
    tampered.writeUInt16LE(tampered.readUInt16LE(centralStart + 8) | 0x0001, centralStart + 8);
    const [read] = zlib.ZipEntry.read(tampered);
    await assert.rejects(drain(read.contentIterator()), { code: 'ERR_ZIP_UNSUPPORTED_FEATURE' });
  }
  // Unsupported compression method.
  {
    const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('hi'), { method: 'store' });
    const archive = await buildArchive([entry]);
    const tampered = Buffer.from(archive);
    tampered.writeUInt16LE(1, 8);
    const centralStart = 30 + 'f.txt'.length + 'hi'.length;
    tampered.writeUInt16LE(1, centralStart + 10);
    const [read] = zlib.ZipEntry.read(tampered);
    await assert.rejects(drain(read.contentIterator()), { code: 'ERR_ZIP_UNSUPPORTED_FEATURE' });
  }
  // maxSize enforced up front.
  {
    const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('hello world'));
    await assert.rejects(drain(entry.contentIterator({ maxSize: 1 })), { code: 'ERR_ZIP_ENTRY_TOO_LARGE' });
  }
  // Declared-size mismatch.
  {
    const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('hello world'), { method: 'store' });
    const archive = await buildArchive([entry]);
    const tampered = Buffer.from(archive);
    const centralStart = 30 + 'f.txt'.length + 'hello world'.length;
    tampered.writeUInt32LE(1, centralStart + 24);
    const [read] = zlib.ZipEntry.read(tampered);
    await assert.rejects(drain(read.contentIterator()), { code: 'ERR_ZIP_ENTRY_CORRUPT' });
  }
  // CRC-32 mismatch, and disabling verification.
  {
    const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('hello world'), { method: 'store' });
    const archive = await buildArchive([entry]);
    const tampered = Buffer.from(archive);
    tampered[30 + 'f.txt'.length] ^= 0xff;
    const [read] = zlib.ZipEntry.read(tampered);
    await assert.rejects(drain(read.contentIterator()), { code: 'ERR_ZIP_ENTRY_CORRUPT' });
    const unverified = await drain(read.contentIterator({ verify: false }));
    assert.strictEqual(unverified.length, 'hello world'.length);
  }
});

// -- content()/contentSync() zstd-specific branches ----------------------------

test('content() and contentSync() enforce maxSize and detect corruption for zstd entries', async () => {
  {
    const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('y'.repeat(5000)), { method: 'zstd' });
    const archive = await buildArchive([entry]);
    const [read] = zlib.ZipEntry.read(archive);
    assert.strictEqual(read.method, 93);
    await assert.rejects(read.content({ maxSize: 10 }), { code: 'ERR_ZIP_ENTRY_TOO_LARGE' });
  }
  {
    const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('z'.repeat(200)), { method: 'zstd' });
    const archive = await buildArchive([entry]);
    const tampered = Buffer.from(archive);
    const contentStart = 30 + 'f.txt'.length;
    tampered.fill(0xff, contentStart, contentStart + 4); // Break the zstd frame itself
    const [read] = zlib.ZipEntry.read(tampered);
    await assert.rejects(read.content(), { code: 'ERR_ZIP_ENTRY_CORRUPT' });
    assert.throws(() => read.contentSync(), { code: 'ERR_ZIP_ENTRY_CORRUPT' });
  }
});

// `decodeMemberSync()` (used by `ZipEntry.prototype.contentSync()`) duplicates
// `decodeMemberStream()`'s guards for its own, separate synchronous code
// path; exercise them through a disk-backed ZipFile.
test('ZipFile getSync().contentSync() enforces the same guards via decodeMemberSync()', async () => {
  async function writeTempArchive(archive, suffix) {
    const filePath = tmpdir.resolve(`zip-coverage-contentsync-${suffix}.zip`);
    await fs.writeFile(filePath, archive);
    return filePath;
  }

  // Encrypted.
  {
    const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('secret'), { method: 'store' });
    const archive = await buildArchive([entry]);
    const tampered = Buffer.from(archive);
    tampered.writeUInt16LE(tampered.readUInt16LE(6) | 0x0001, 6);
    const centralStart = 30 + 'f.txt'.length + 'secret'.length;
    tampered.writeUInt16LE(tampered.readUInt16LE(centralStart + 8) | 0x0001, centralStart + 8);
    const filePath = await writeTempArchive(tampered, 'encrypted');
    const zf = zlib.ZipFile.openSync(filePath);
    assert.throws(() => zf.getSync('f.txt').contentSync(), { code: 'ERR_ZIP_UNSUPPORTED_FEATURE' });
    zf.closeSync();
    await fs.unlink(filePath);
  }
  // maxSize, for both the deflate and the zstd decode branch.
  for (const method of ['deflate', 'zstd']) {
    const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('y'.repeat(5000)), { method });
    const archive = await buildArchive([entry]);
    const filePath = await writeTempArchive(archive, `maxsize-${method}`);
    const zf = zlib.ZipFile.openSync(filePath);
    assert.throws(() => zf.getSync('f.txt').contentSync({ maxSize: 10 }), { code: 'ERR_ZIP_ENTRY_TOO_LARGE' });
    zf.closeSync();
    await fs.unlink(filePath);
  }
  // A genuine decompression failure (not just a CRC mismatch after a
  // successful decode).
  {
    const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('y'.repeat(500)), { method: 'deflate' });
    const archive = await buildArchive([entry]);
    const tampered = Buffer.from(archive);
    const contentStart = 30 + 'f.txt'.length;
    tampered.fill(0xff, contentStart, contentStart + 4);
    const filePath = await writeTempArchive(tampered, 'deflate-corrupt');
    const zf = zlib.ZipFile.openSync(filePath);
    assert.throws(() => zf.getSync('f.txt').contentSync(), { code: 'ERR_ZIP_ENTRY_CORRUPT' });
    zf.closeSync();
    await fs.unlink(filePath);
  }
  // Declared-size mismatch ("produced N bytes, expected M").
  {
    const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('hello world'), { method: 'store' });
    const archive = await buildArchive([entry]);
    const tampered = Buffer.from(archive);
    const centralStart = 30 + 'f.txt'.length + 'hello world'.length;
    tampered.writeUInt32LE(1, centralStart + 24);
    const filePath = await writeTempArchive(tampered, 'size-mismatch');
    const zf = zlib.ZipFile.openSync(filePath);
    assert.throws(() => zf.getSync('f.txt').contentSync(), { code: 'ERR_ZIP_ENTRY_CORRUPT' });
    zf.closeSync();
    await fs.unlink(filePath);
  }
});

test('contentIterator() rejects an entry that inflates to less than its declared size', async () => {
  const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('hi'), { method: 'store' });
  const archive = await buildArchive([entry]);
  const tampered = Buffer.from(archive);
  const centralStart = 30 + 'f.txt'.length + 'hi'.length;
  tampered.writeUInt32LE(1000, centralStart + 24); // Declared size grown beyond reality
  const [read] = zlib.ZipEntry.read(tampered);
  await assert.rejects(drain(read.contentIterator()), {
    code: 'ERR_ZIP_ENTRY_CORRUPT',
    message: /is truncated/,
  });
});

// -- ZipBuffer / ZipFile iteration protocols -----------------------------------

test('ZipBuffer exposes Map-like forEach/values/entries/iteration/toStringTag', async () => {
  const archive = await buildArchive([
    await zlib.ZipEntry.create('a.txt', Buffer.from('1')),
    await zlib.ZipEntry.create('b.txt', Buffer.from('2')),
  ]);
  const zip = new zlib.ZipBuffer(archive);

  const seen = [];
  zip.forEach((entry, key, self) => {
    seen.push(key);
    assert.strictEqual(self, zip);
    assert.strictEqual(entry.name, key);
  });
  assert.deepStrictEqual(seen.sort(), ['a.txt', 'b.txt']);

  assert.deepStrictEqual([...zip.values()].map((e) => e.name).sort(), ['a.txt', 'b.txt']);
  assert.deepStrictEqual([...zip.entries()].map(([k]) => k).sort(), ['a.txt', 'b.txt']);
  assert.deepStrictEqual([...zip].map(([k]) => k).sort(), ['a.txt', 'b.txt']);
  assert.strictEqual(Object.prototype.toString.call(zip), '[object ZipBuffer]');
  assert.throws(() => zip.addEntry({}), { code: 'ERR_INVALID_ARG_TYPE' });
});

test('ZipFile exposes the same iteration protocol, plus its Sync counterparts', async () => {
  const archive = await buildArchive([
    await zlib.ZipEntry.create('a.txt', Buffer.from('1')),
    await zlib.ZipEntry.create('b.txt', Buffer.from('2')),
  ]);
  const filePath = tmpdir.resolve('zip-coverage-iteration.zip');
  await fs.writeFile(filePath, archive);

  const zf = await zlib.ZipFile.open(filePath);
  try {
    const pending = [];
    zf.forEach((valuePromise) => pending.push(valuePromise));
    await Promise.all(pending); // Let every dangling get() settle before closing

    zf.forEachSync(() => {});
    assert.deepStrictEqual([...zf.valuesSync()].map((e) => e.name).sort(), ['a.txt', 'b.txt']);
    assert.deepStrictEqual([...zf.entriesSync()].map(([k]) => k).sort(), ['a.txt', 'b.txt']);
    assert.deepStrictEqual([...zf.keys()].sort(), ['a.txt', 'b.txt']);
    assert.strictEqual(zf.size, 2);
    assert.strictEqual(Object.prototype.toString.call(zf), '[object ZipFile]');

    const names = [];
    for await (const entry of zf) names.push(entry.name);
    assert.deepStrictEqual(names.sort(), ['a.txt', 'b.txt']);

    // Synchronous iteration (Symbol.iterator) yields [name, Promise<ZipEntry>].
    const syncPairs = [...zf];
    assert.deepStrictEqual(syncPairs.map(([k]) => k).sort(), ['a.txt', 'b.txt']);
    const resolved = await Promise.all(syncPairs.map(([, v]) => v));
    assert.deepStrictEqual(resolved.map((e) => e.name).sort(), ['a.txt', 'b.txt']);
  } finally {
    await zf[Symbol.asyncDispose]();
  }

  const writable = await zlib.ZipFile.open(filePath, { writable: true });
  await assert.rejects(writable.addEntry({}), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => writable.addEntrySync({}), { code: 'ERR_INVALID_ARG_TYPE' });
  await writable.close();

  const zf2 = zlib.ZipFile.openSync(filePath);
  zf2[Symbol.dispose]();

  await fs.unlink(filePath);
});

// -- ZipFile (on-disk) error paths ---------------------------------------------

test('ZipFile.open()/openSync() reject a file with no end-of-central-directory record', async () => {
  const filePath = tmpdir.resolve('zip-coverage-garbage.zip');
  await fs.writeFile(filePath, Buffer.from('not a zip file, just garbage bytes'));
  try {
    await assert.rejects(zlib.ZipFile.open(filePath), { code: 'ERR_ZIP_INVALID_ARCHIVE' });
    assert.throws(() => zlib.ZipFile.openSync(filePath), { code: 'ERR_ZIP_INVALID_ARCHIVE' });
  } finally {
    await fs.unlink(filePath);
  }
});

test('ZipFile get()/getSync()/stream() reject a missing entry name', async () => {
  const archive = await buildArchive([await zlib.ZipEntry.create('a.txt', Buffer.from('x'))]);
  const filePath = tmpdir.resolve('zip-coverage-notfound.zip');
  await fs.writeFile(filePath, archive);
  const zf = await zlib.ZipFile.open(filePath);
  try {
    await assert.rejects(zf.get('missing'), { code: 'ERR_ZIP_ENTRY_NOT_FOUND' });
    assert.throws(() => zf.getSync('missing'), { code: 'ERR_ZIP_ENTRY_NOT_FOUND' });
    await assert.rejects(zf.stream('missing'), { code: 'ERR_ZIP_ENTRY_NOT_FOUND' });
  } finally {
    await zf.close();
    await fs.unlink(filePath);
  }
});

test('a local file header offset pointing into the central directory is rejected at open', async () => {
  const name = 'a.txt';
  const content = Buffer.from('hello');
  const archive = await buildArchive([await zlib.ZipEntry.create(name, content, { method: 'store' })]);
  const centralHeaderStart = 30 + name.length + content.length;
  const tampered = Buffer.from(archive);
  // Point the local file header offset at the central directory itself: the
  // member's claimed data range then crosses the directory, which the
  // open-time overlap check rejects before anything is read.
  tampered.writeUInt32LE(centralHeaderStart, centralHeaderStart + 42);
  const filePath = tmpdir.resolve('zip-coverage-cd-offset.zip');
  await fs.writeFile(filePath, tampered);
  try {
    await assert.rejects(zlib.ZipFile.open(filePath),
                         { code: 'ERR_ZIP_INVALID_ARCHIVE', message: /overlaps/ });
    assert.throws(() => zlib.ZipFile.openSync(filePath),
                  { code: 'ERR_ZIP_INVALID_ARCHIVE', message: /overlaps/ });
  } finally {
    await fs.unlink(filePath);
  }
});

test('a declared compressed size reaching past the end of the file is rejected', async () => {
  const name = 'a.txt';
  const content = Buffer.from('hello');
  const archive = await buildArchive([await zlib.ZipEntry.create(name, content, { method: 'store' })]);
  const centralHeaderStart = 30 + name.length + content.length;
  const tampered = Buffer.from(archive);
  tampered.writeUInt32LE(archive.length * 10, centralHeaderStart + 20); // compressedSize
  const filePath = tmpdir.resolve('zip-coverage-eof.zip');
  await fs.writeFile(filePath, tampered);

  try {
    // Member bounds are validated against the file size up front, so the lie
    // is caught at open time - before any buffered read path could allocate
    // `compressedSize` bytes for it.
    await assert.rejects(zlib.ZipFile.open(filePath),
                         { code: 'ERR_ZIP_INVALID_ARCHIVE', message: /out of bounds/ });
    assert.throws(() => zlib.ZipFile.openSync(filePath),
                  { code: 'ERR_ZIP_INVALID_ARCHIVE', message: /out of bounds/ });
  } finally {
    await fs.unlink(filePath);
  }
});

test('a Zip64-declared compressed size of kMaxLength is rejected at open', async () => {
  const name = 'f.txt';
  const content = Buffer.from('hello');
  const archive = await buildArchive([await zlib.ZipEntry.create(name, content, { method: 'store' })]);

  // A Zip64-declared compressedSize equal to kMaxLength is the largest value
  // that still parses (readSafeUint64 caps at the safe-integer ceiling, which
  // is kMaxLength on 64-bit), but such a member cannot lie inside this tiny
  // file, so the open-time member bounds check rejects the archive before
  // the buffering read paths (whose own kMaxLength guard remains as defense
  // in depth for 32-bit, where kMaxLength is smaller than real file sizes)
  // could ever allocate for it.
  const centralHeaderStart = 30 + name.length + content.length;
  const nameStart = centralHeaderStart + 46;
  const tlv = Buffer.allocUnsafe(4 + 8);
  tlv.writeUInt16LE(0x0001, 0);
  tlv.writeUInt16LE(8, 2);
  tlv.writeBigUInt64LE(BigInt(require('node:buffer').kMaxLength), 4);
  const before = archive.subarray(0, nameStart + name.length);
  const after = archive.subarray(nameStart + name.length);
  const patched = Buffer.concat([before, tlv, after]);
  patched.writeUInt16LE(tlv.length, centralHeaderStart + 30);
  patched.writeUInt32LE(0xffffffff, centralHeaderStart + 20); // compressedSize sentinel
  const eocdOffset = patched.length - 22;
  patched.writeUInt32LE(patched.readUInt32LE(eocdOffset + 12) + tlv.length, eocdOffset + 12);

  const toolargeFilePath = tmpdir.resolve('zip-coverage-toolarge.zip');
  await fs.writeFile(toolargeFilePath, patched);
  try {
    await assert.rejects(zlib.ZipFile.open(toolargeFilePath),
                         { code: 'ERR_ZIP_INVALID_ARCHIVE', message: /out of bounds/ });
    assert.throws(() => zlib.ZipFile.openSync(toolargeFilePath),
                  { code: 'ERR_ZIP_INVALID_ARCHIVE', message: /out of bounds/ });
  } finally {
    await fs.unlink(toolargeFilePath);
  }
});

// -- forcing Zip64 structures without a multi-gigabyte archive -----------------

test('createZipArchiveSync() also switches to Zip64 structures at 0xFFFF entries', () => {
  const ZIP64_EOCD_SIGNATURE = Buffer.from([0x50, 0x4b, 0x06, 0x06]);
  const entries = [];
  for (let i = 0; i < 0x10000; i++) {
    entries.push(zlib.ZipEntry.createSync(`entry-${i}`, Buffer.alloc(0), { method: 'store' }));
  }
  const chunks = [];
  for (const chunk of zlib.createZipArchiveSync(entries)) chunks.push(chunk);
  const archive = Buffer.concat(chunks);
  assert.ok(archive.includes(ZIP64_EOCD_SIGNATURE));
  assert.strictEqual([...zlib.ZipEntry.read(archive)].length, 0x10000);
}, { timeout: 120_000 });

// -- createZipArchive()'s single options argument, baseOffset, and Readable return --

const CENTRAL_FILE_HEADER_SIGNATURE = Buffer.from([0x50, 0x4b, 0x01, 0x02]);

test('createZipArchive() returns a pipeable, async-iterable, non-object-mode Readable', async () => {
  const { Readable } = require('node:stream');
  const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('hi'));
  const stream = zlib.createZipArchive([entry]);
  assert.ok(stream instanceof Readable);
  assert.strictEqual(stream.readableObjectMode, false);
  const chunks = [];
  for await (const chunk of stream) chunks.push(chunk);
  assert.strictEqual([...zlib.ZipEntry.read(Buffer.concat(chunks))][0].name, 'f.txt');
});

test('createZipArchive()/createZipArchiveSync() take a plain string as comment shorthand', async () => {
  const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('hi'));
  const zip = new zlib.ZipBuffer(await drain(zlib.createZipArchive([entry], 'hello')));
  assert.strictEqual(zip.comment, 'hello');

  const entrySync = zlib.ZipEntry.createSync('f.txt', Buffer.from('hi'));
  const chunks = [...zlib.createZipArchiveSync([entrySync], 'hello-sync')];
  const zipSync = new zlib.ZipBuffer(Buffer.concat(chunks));
  assert.strictEqual(zipSync.comment, 'hello-sync');
});

test('createZipArchive()/createZipArchiveSync() take an { comment, baseOffset } options object', async () => {
  const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('hi'));
  const zip = new zlib.ZipBuffer(await drain(zlib.createZipArchive([entry], { comment: 'hi there' })));
  assert.strictEqual(zip.comment, 'hi there');
});

test('createZipArchive()/createZipArchiveSync() reject a non-string, non-object options argument', async () => {
  await assert.rejects(drain(zlib.createZipArchive([], 123)), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => [...zlib.createZipArchiveSync([], 123)], { code: 'ERR_INVALID_ARG_TYPE' });
  await assert.rejects(drain(zlib.createZipArchive([], null)), { code: 'ERR_INVALID_ARG_TYPE' });
});

test('createZipArchive()/createZipArchiveSync() validate options.baseOffset', async () => {
  await assert.rejects(drain(zlib.createZipArchive([], { baseOffset: -1 })), { code: 'ERR_OUT_OF_RANGE' });
  await assert.rejects(drain(zlib.createZipArchive([], { baseOffset: 1.5 })), { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => [...zlib.createZipArchiveSync([], { baseOffset: -1 })], { code: 'ERR_OUT_OF_RANGE' });
});

test('options.baseOffset shifts every recorded offset, so a prefixed archive is ' +
     'self-describing without relying on prefix auto-detection', async () => {
  const entry = await zlib.ZipEntry.create('f.txt', Buffer.from('hello offset'));
  const prefix = Buffer.from('#!/bin/sh\nexit 0\n');

  const shifted = await drain(zlib.createZipArchive([entry], { baseOffset: prefix.byteLength }));
  const shiftedCentral = shifted.indexOf(CENTRAL_FILE_HEADER_SIGNATURE);
  assert.strictEqual(shifted.readUInt32LE(shiftedCentral + 42), prefix.byteLength);

  const entryUnshifted = await zlib.ZipEntry.create('f.txt', Buffer.from('hello offset'));
  const unshifted = await drain(zlib.createZipArchive([entryUnshifted]));
  const unshiftedCentral = unshifted.indexOf(CENTRAL_FILE_HEADER_SIGNATURE);
  assert.strictEqual(unshifted.readUInt32LE(unshiftedCentral + 42), 0);

  const combined = Buffer.concat([prefix, shifted]);
  const zip = new zlib.ZipBuffer(combined);
  assert.strictEqual((await zip.get('f.txt').content()).toString(), 'hello offset');
});

test('zipBuffer.toBuffer()/toBufferSync() forward the same string/options-object shorthand', async () => {
  const zip = new zlib.ZipBuffer(await drain(zlib.createZipArchive([])));
  await zip.add('f.txt', Buffer.from('hi'));

  assert.strictEqual(new zlib.ZipBuffer(await zip.toBuffer('a comment')).comment, 'a comment');
  assert.strictEqual(new zlib.ZipBuffer(await zip.toBuffer({ comment: 'an object comment' })).comment,
                     'an object comment');
  assert.strictEqual(new zlib.ZipBuffer(zip.toBufferSync('sync comment')).comment, 'sync comment');

  const prefix = Buffer.from('junk\n');
  const shifted = await zip.toBuffer({ baseOffset: prefix.byteLength });
  const shiftedCentral = shifted.indexOf(CENTRAL_FILE_HEADER_SIGNATURE);
  assert.strictEqual(shifted.readUInt32LE(shiftedCentral + 42), prefix.byteLength);
});

// -- round-trip fidelity of foreign metadata ----------------------------------

test('a non-Unix "version made by" host survives re-serialization', async () => {
  const name = 'f.txt';
  const content = Buffer.from('hello');
  const entry = await zlib.ZipEntry.create(name, content, { method: 'store' });
  const archive = Buffer.from(await buildArchive([entry]));

  // Rewrite the entry as DOS-made (host byte 0, sec. 4.4.2) with DOS-style
  // external attributes (0x20, the archive bit): the high 16 bits are NOT
  // Unix permissions for this host, so `mode` must read 0, and the host byte
  // must survive re-serialization - stamping it as Unix would turn the
  // zeroed high bits into Unix mode 0000 for downstream extractors.
  const centralHeaderStart = 30 + name.length + content.length;
  archive[centralHeaderStart + 5] = 0; // "version made by" host byte
  archive.writeUInt32LE(0x20, centralHeaderStart + 38); // external attributes

  const [read] = zlib.ZipEntry.read(archive);
  assert.strictEqual(read.mode, 0);
  assert.strictEqual(read.isSymlink, false);

  const rewritten = await buildArchive([read]);
  const rewrittenCentral = rewritten.indexOf(CENTRAL_FILE_HEADER_SIGNATURE);
  assert.strictEqual(rewritten[rewrittenCentral + 5], 0); // host byte preserved
  assert.strictEqual(rewritten.readUInt32LE(rewrittenCentral + 38), 0x20);
  const [reread] = zlib.ZipEntry.read(rewritten);
  assert.strictEqual(reread.mode, 0);
  // The finalized entry itself now answers from its snapshotted metadata,
  // which must apply the same made-by gate.
  assert.strictEqual(read.mode, 0);
  assert.strictEqual(read.isSymlink, false);
});

// -- content() ownership -------------------------------------------------------

test('content()/contentSync() of a stored in-memory entry return caller-owned memory', async () => {
  const archive = await buildArchive(
    [await zlib.ZipEntry.create('s.txt', Buffer.from('hello'), { method: 'store' })]);
  const zip = new zlib.ZipBuffer(archive);
  const entry = zip.get('s.txt');

  // If content() returned a view into the archive, zeroing it would corrupt
  // the entry and the next read would fail CRC-32 verification.
  (await entry.content()).fill(0);
  assert.deepStrictEqual(await entry.content(), Buffer.from('hello'));
  entry.contentSync().fill(0);
  assert.deepStrictEqual(entry.contentSync(), Buffer.from('hello'));
});

// Injects a raw extra-field blob into a single-entry archive's central
// header without touching any size/offset field (unlike injectRawZip64Extra
// above, which also plants an overflow sentinel).
function injectCentralExtra(archive, name, contentLength, extraBytes) {
  const centralHeaderStart = 30 + name.length + contentLength;
  const nameStart = centralHeaderStart + 46;
  const extraLengthOffset = centralHeaderStart + 30;
  assert.strictEqual(archive.readUInt16LE(extraLengthOffset), 0);
  const before = archive.subarray(0, nameStart + name.length);
  const after = archive.subarray(nameStart + name.length);
  const patched = Buffer.concat([before, extraBytes, after]);
  patched.writeUInt16LE(extraBytes.length, extraLengthOffset);
  const eocdOffset = patched.length - 22;
  patched.writeUInt32LE(patched.readUInt32LE(eocdOffset + 12) + extraBytes.length, eocdOffset + 12);
  return patched;
}

// -- input coercion ------------------------------------------------------------

test('a plain Uint8Array is accepted anywhere binary input is, and non-binary input is rejected', async () => {
  const archive = await buildArchive([await zlib.ZipEntry.create('u.txt', Buffer.from('hi'))]);
  const viaU8 = new zlib.ZipBuffer(new Uint8Array(archive));
  assert.strictEqual((await viaU8.get('u.txt').content()).toString(), 'hi');
  const entry = await zlib.ZipEntry.create('v.txt', new Uint8Array([1, 2, 3]), { method: 'store' });
  assert.deepStrictEqual(await entry.content(), Buffer.from([1, 2, 3]));
  assert.throws(() => new zlib.ZipBuffer('not binary'), { code: 'ERR_INVALID_ARG_TYPE' });
  await assert.rejects(zlib.ZipEntry.create('w.txt', 'not binary'), { code: 'ERR_INVALID_ARG_TYPE' });
});

test('forEach() passes thisArg through on ZipBuffer and ZipFile', async () => {
  const archive = await buildArchive([await zlib.ZipEntry.create('t.txt', Buffer.from('x'))]);
  const zip = new zlib.ZipBuffer(archive);
  const ctx = { hits: 0 };
  zip.forEach(function() { this.hits++; }, ctx);
  assert.strictEqual(ctx.hits, 1);

  const filePath = tmpdir.resolve('zip-coverage-foreach.zip');
  await fs.writeFile(filePath, archive);
  const zf = await zlib.ZipFile.open(filePath);
  try {
    const fileCtx = { hits: 0 };
    zf.forEach(function() { this.hits++; }, fileCtx);
    zf.forEachSync(function() { this.hits++; }, fileCtx);
    assert.strictEqual(fileCtx.hits, 2);
  } finally {
    await zf.close();
    await fs.unlink(filePath);
  }
});

// -- entry metadata before serialization ----------------------------------------

test('freshly created entries expose their metadata before serialization', async () => {
  const sym = zlib.ZipEntry.createSymlink('link', 'target');
  assert.strictEqual(sym.mode, 0o777); // The symlink default
  assert.strictEqual(sym.isSymlink, true);
  assert.strictEqual(sym.isFile, false);

  const entry = await zlib.ZipEntry.create('m.txt', Buffer.from('x'), {
    comment: 'note', mode: 0o640, modified: new Date(1700000000000),
  });
  assert.deepStrictEqual(entry.nameBuffer, Buffer.from('m.txt'));
  assert.strictEqual(entry.comment, 'note');
  assert.strictEqual(entry.modified.getTime(), 1700000000000);
  assert.strictEqual(entry.mode, 0o640);
  assert.strictEqual(entry.isSymlink, false);
});

// -- extra-field edge cases ------------------------------------------------------

test('a Unicode Path extra field with an unknown version is ignored', async () => {
  const name = 'plain.txt';
  const content = Buffer.from('hi');
  const archive = await buildArchive([await zlib.ZipEntry.create(name, content, { method: 'store' })]);
  const utf8Name = Buffer.from('other.txt');
  const up = Buffer.allocUnsafe(4 + 5 + utf8Name.length);
  up.writeUInt16LE(0x7075, 0);
  up.writeUInt16LE(5 + utf8Name.length, 2);
  up.writeUInt8(2, 4); // Unsupported version: only version 1 is defined
  up.writeUInt32LE(0, 5); // CRC-32 (irrelevant; the version check comes first)
  utf8Name.copy(up, 9);
  const patched = injectCentralExtra(archive, name, content.length, up);
  const [read] = zlib.ZipEntry.read(patched);
  assert.strictEqual(read.name, 'plain.txt');
});

test('malformed or empty timestamp extra fields fall back to the DOS time', async () => {
  const name = 't.txt';
  const content = Buffer.from('x');
  const modified = new Date(2024, 3, 5, 10, 20, 30);
  const archive = await buildArchive(
    [await zlib.ZipEntry.create(name, content, { method: 'store', modified })]);
  const ntfs = Buffer.allocUnsafe(4 + 12);
  ntfs.writeUInt16LE(0x000a, 0);
  ntfs.writeUInt16LE(12, 2);
  ntfs.writeUInt32LE(0, 4); // Reserved dword
  ntfs.writeUInt16LE(1, 8); // Tag 1...
  ntfs.writeUInt16LE(200, 10); // ...claiming 200 bytes: overruns its record
  ntfs.writeUInt32LE(0, 12);
  const ut = Buffer.from([0x55, 0x54, 0x01, 0x00, 0x00]); // "UT", flags 0: no mtime present
  const ux = Buffer.allocUnsafe(4 + 4);
  ux.writeUInt16LE(0x5855, 0);
  ux.writeUInt16LE(4, 2); // Too short for the atime+mtime pair
  ux.writeUInt32LE(123, 4);
  const patched = injectCentralExtra(
    archive, name, content.length, Buffer.concat([ntfs, ut, ux]));
  const [read] = zlib.ZipEntry.read(patched);
  assert.strictEqual(read.modified.getTime(), modified.getTime());
});

test('a preserved timestamp extra survives re-serialization without duplication', async () => {
  const odd = new Date(1700000001000); // An odd second: gets a "UT" extra
  const name = 'odd.txt';
  const content = Buffer.from('x');
  const first = await buildArchive(
    [await zlib.ZipEntry.create(name, content, { modified: odd, method: 'store' })]);
  const [read] = zlib.ZipEntry.read(first);
  const second = await buildArchive([read]);
  const [reread] = zlib.ZipEntry.read(second);
  assert.strictEqual(reread.modified.getTime(), odd.getTime());
  // Exactly one extended-timestamp record: the preserved one, no new copy.
  const central = second.indexOf(CENTRAL_FILE_HEADER_SIGNATURE);
  const nameLength = second.readUInt16LE(central + 28);
  const extraLength = second.readUInt16LE(central + 30);
  const extra = second.subarray(
    central + 46 + nameLength, central + 46 + nameLength + extraLength);
  let utRecords = 0;
  for (let pos = 0; pos + 4 <= extra.length;) {
    if (extra.readUInt16LE(pos) === 0x5455) utRecords++;
    pos += 4 + extra.readUInt16LE(pos + 2);
  }
  assert.strictEqual(utRecords, 1);
});

// -- file-backed metadata and I/O failure paths ----------------------------------

test('modified falls back to the central directory when the local header is malformed', async () => {
  const name = 'm.txt';
  const content = Buffer.from('hello');
  const modified = new Date(2024, 3, 5, 10, 20, 30);
  const archive = Buffer.from(await buildArchive(
    [await zlib.ZipEntry.create(name, content, { method: 'store', modified })]));
  archive.writeUInt32LE(0xdeadbeef, 0); // Corrupt the local header signature
  const filePath = tmpdir.resolve('zip-coverage-badlocal.zip');
  await fs.writeFile(filePath, archive);
  const zf = await zlib.ZipFile.open(filePath);
  try {
    const entry = await zf.get(name);
    // Metadata resolution swallows the local-header failure...
    assert.strictEqual(entry.modified.getTime(), modified.getTime());
    // ...but content reads fail loudly on it.
    await assert.rejects(entry.content(), { code: 'ERR_ZIP_INVALID_ARCHIVE' });
  } finally {
    await zf.close();
    await fs.unlink(filePath);
  }
});

test('a file that shrinks after open is rejected as unexpected EOF when read', async () => {
  // The open-time bounds and overlap checks guarantee every member lies
  // inside the file *as it was at open time*; a file truncated afterwards
  // (the only way a positioned read can now hit EOF) must fail cleanly.
  const name = 'a.txt';
  const content = Buffer.from('hello');
  const archive = await buildArchive(
    [await zlib.ZipEntry.create(name, content, { method: 'store' })]);
  const filePath = tmpdir.resolve('zip-coverage-eof-local.zip');
  await fs.writeFile(filePath, archive);
  const zf = await zlib.ZipFile.open(filePath);
  const zfSync = zlib.ZipFile.openSync(filePath);
  try {
    await fs.truncate(filePath, 10); // Cuts into the local file header
    await assert.rejects((await zf.get(name)).content(),
                         { code: 'ERR_ZIP_INVALID_ARCHIVE', message: /unexpected end of file/ });
    assert.throws(() => zfSync.getSync(name).contentSync(),
                  { code: 'ERR_ZIP_INVALID_ARCHIVE', message: /unexpected end of file/ });
  } finally {
    await zf.close();
    zfSync.closeSync();
    await fs.unlink(filePath);
  }
});

test('a failed streaming addEntry() restores the archive and the queue continues', async () => {
  const filePath = tmpdir.resolve('zip-coverage-rollback.zip');
  await fs.writeFile(filePath, await buildArchive(
    [await zlib.ZipEntry.create('seed.txt', Buffer.from('seed'))]));
  const zip = await zlib.ZipFile.open(filePath, { writable: true });
  try {
    async function* failing() {
      yield Buffer.from('partial data that overwrote the central directory');
      throw new Error('source failed');
    }
    await assert.rejects(
      zip.addEntry(zlib.ZipEntry.createStream('bad.txt', failing())),
      /source failed/);
    // The rewrite restored the directory in place, and the mutation queue
    // keeps accepting work after a failure.
    await zip.add('good.txt', Buffer.from('ok'));
  } finally {
    await zip.close();
  }
  const reread = await zlib.ZipFile.open(filePath);
  try {
    assert.deepStrictEqual([...reread.keys()].sort(), ['good.txt', 'seed.txt']);
    assert.strictEqual((await (await reread.get('seed.txt')).content()).toString(), 'seed');
    assert.strictEqual((await (await reread.get('good.txt')).content()).toString(), 'ok');
  } finally {
    await reread.close();
    await fs.unlink(filePath);
  }
});

test('opening a missing archive surfaces the file-system error', async () => {
  const missing = tmpdir.resolve('zip-coverage-missing.zip');
  await assert.rejects(zlib.ZipFile.open(missing), { code: 'ENOENT' });
  assert.throws(() => zlib.ZipFile.openSync(missing), { code: 'ENOENT' });
  await assert.rejects(
    drain(zlib.zipFiles([[tmpdir.resolve('missing-src.txt'), 'a.txt']], { followSymlinks: false })),
    { code: 'ENOENT' });
});

test('using a ZipFile after close() fails with system-level errors', async () => {
  const filePath = tmpdir.resolve('zip-coverage-after-close.zip');
  await fs.writeFile(filePath, await buildArchive(
    [await zlib.ZipEntry.create('a.txt', Buffer.from('data'))]));

  const zip = await zlib.ZipFile.open(filePath, { writable: true });
  const entry = await zip.get('a.txt');
  await zip.close();
  // Reads through a retained entry hit the closed descriptor...
  await assert.rejects(entry.content(), { code: 'EBADF' });
  // ...as do further mutations and a second close().
  await assert.rejects(
    zip.addEntry(await zlib.ZipEntry.create('b.txt', Buffer.from('x'))), { code: 'EBADF' });
  await assert.rejects(zip.close(), { code: 'EBADF' });

  const zipSync = zlib.ZipFile.openSync(filePath, { writable: true });
  const entrySync = zipSync.getSync('a.txt');
  zipSync.closeSync();
  assert.throws(() => entrySync.contentSync(), { code: 'EBADF' });
  assert.throws(
    () => zipSync.addEntrySync(zlib.ZipEntry.createSync('b.txt', Buffer.from('x'))),
    { code: 'EBADF' });
  await fs.unlink(filePath);
});

// -- writer-side Zip64 fields ----------------------------------------------------

test('an entry starting beyond 4 GiB records its offset in a Zip64 extra field', async () => {
  const BASE = 0x1_0000_0000; // 4 GiB
  const entry = await zlib.ZipEntry.create('far.txt', Buffer.from('hello'), { method: 'store' });
  const archive = await drain(zlib.createZipArchive([entry], { baseOffset: BASE }));
  const central = archive.indexOf(CENTRAL_FILE_HEADER_SIGNATURE);
  assert.notStrictEqual(central, -1);
  assert.strictEqual(archive.readUInt16LE(central + 6), 45); // Version needed: Zip64
  assert.strictEqual(archive.readUInt32LE(central + 42), 0xffffffff); // Offset sentinel
  const nameLength = archive.readUInt16LE(central + 28);
  const extraStart = central + 46 + nameLength;
  assert.strictEqual(archive.readUInt16LE(extraStart), 0x0001);
  assert.strictEqual(archive.readUInt16LE(extraStart + 2), 8);
  assert.strictEqual(archive.readBigUInt64LE(extraStart + 4), BigInt(BASE));
  // The trailer is promoted to Zip64 too: the directory offset overflows.
  assert.notStrictEqual(archive.indexOf(Buffer.from([0x50, 0x4b, 0x06, 0x06])), -1);
});

test('extra fields that no longer fit their 16-bit length field are rejected on write', async () => {
  const name = 'big-extra.txt';
  const content = Buffer.from('hi');
  const archive = await buildArchive(
    [await zlib.ZipEntry.create(name, content, { method: 'store' })]);
  // Give the entry a preserved (unknown-ID) extra field near the 65,535-byte
  // cap; serializing it at an offset beyond 4 GiB must add a 12-byte Zip64
  // record, pushing the total over the cap.
  const filler = Buffer.allocUnsafe(4 + 65526);
  filler.writeUInt16LE(0x9999, 0);
  filler.writeUInt16LE(65526, 2);
  filler.fill(0xab, 4);
  const patched = injectCentralExtra(archive, name, content.length, filler);
  const [read] = zlib.ZipEntry.read(patched);
  await assert.rejects(
    drain(zlib.createZipArchive([read], { baseOffset: 0x1_0000_0000 })),
    { code: 'ERR_ZIP_ENTRY_TOO_LARGE', message: /extra fields/ });
});

// -- archive-end discovery edge cases --------------------------------------------

test('a Zip64 EOCD record pushed out of the tail by its data sector is found by re-reading', async () => {
  const PAD = 80000; // Larger than the fixed-size tail read
  const record = Buffer.alloc(56 + PAD);
  record.writeUInt32LE(0x06064b50, 0);
  record.writeBigUInt64LE(BigInt(44 + PAD), 4); // Remainder size, incl. the sector
  record.writeUInt16LE((3 << 8) | 45, 12);
  record.writeUInt16LE(45, 14);
  const locator = Buffer.alloc(20);
  locator.writeUInt32LE(0x07064b50, 0);
  locator.writeBigUInt64LE(0n, 8); // The record starts at offset 0
  locator.writeUInt32LE(1, 16);
  const comment = Buffer.from('sector');
  const eocd = Buffer.alloc(22 + comment.length);
  eocd.writeUInt32LE(0x06054b50, 0);
  eocd.writeUInt32LE(0xffffffff, 12); // Size sentinel: the Zip64 record is required
  eocd.writeUInt16LE(comment.length, 20);
  comment.copy(eocd, 22);
  const bytes = Buffer.concat([record, locator, eocd]);

  // In one buffer the record is directly reachable...
  assert.strictEqual(new zlib.ZipBuffer(bytes).size, 0);

  // ...but a tail read must notice it starts before the tail and re-read.
  const filePath = tmpdir.resolve('zip-coverage-sector.zip');
  await fs.writeFile(filePath, bytes);
  const zf = await zlib.ZipFile.open(filePath);
  try {
    assert.strictEqual(zf.size, 0);
    assert.strictEqual(zf.comment, 'sector');
  } finally {
    await zf.close();
  }
  const zfSync = zlib.ZipFile.openSync(filePath);
  try {
    assert.strictEqual(zfSync.size, 0);
  } finally {
    zfSync.closeSync();
  }
  await fs.unlink(filePath);
});

test('a locator offset beyond the safe-integer range falls back to classic fields', () => {
  const buf = buildMinimalZip64Archive({ locator: { recordOffset: 0xffffffffffffffffn } });
  buf.writeUInt32LE(0xdeadbeef, 0); // Corrupt the record so only the fallback can succeed
  assert.strictEqual([...zlib.ZipEntry.read(buf)].length, 0);
});

test('ZipFile rejects a central header on another disk', async () => {
  const name = 'a.txt';
  const content = Buffer.from('x');
  const archive = Buffer.from(await buildArchive(
    [await zlib.ZipEntry.create(name, content, { method: 'store' })]));
  const centralHeaderStart = 30 + name.length + content.length;
  archive.writeUInt16LE(1, centralHeaderStart + 34); // Disk number
  const filePath = tmpdir.resolve('zip-coverage-disk.zip');
  await fs.writeFile(filePath, archive);
  await assert.rejects(zlib.ZipFile.open(filePath), { code: 'ERR_ZIP_UNSUPPORTED_FEATURE' });
  assert.throws(() => zlib.ZipFile.openSync(filePath), { code: 'ERR_ZIP_UNSUPPORTED_FEATURE' });
  await fs.unlink(filePath);
});

test('zipFiles() keeps a directory mapping name that already ends in a slash', async () => {
  const dir = tmpdir.resolve('zip-coverage-dir');
  await fs.mkdir(dir, { recursive: true });
  const archive = await drain(zlib.zipFiles([[dir, 'mapped/']]));
  const [entry] = zlib.ZipEntry.read(archive);
  assert.strictEqual(entry.name, 'mapped/');
  assert.strictEqual(entry.isDirectory, true);
});

test('a streamed entry with the store method round-trips', async () => {
  async function* source() {
    yield Buffer.from('stored ');
    yield Buffer.from('stream');
  }
  const entry = zlib.ZipEntry.createStream('s.txt', source(), { method: 'store' });
  const archive = await buildArchive([entry]);
  const [read] = zlib.ZipEntry.read(archive);
  assert.strictEqual(read.method, 0);
  assert.strictEqual((await read.content()).toString(), 'stored stream');
});

test('createSync() honors an explicit store method', () => {
  const entry = zlib.ZipEntry.createSync('s.bin', Buffer.from([1, 2, 3]), { method: 'store' });
  assert.strictEqual(entry.method, 0);
  assert.strictEqual(entry.compressed, false);
  assert.deepStrictEqual(entry.contentSync(), Buffer.from([1, 2, 3]));
});
