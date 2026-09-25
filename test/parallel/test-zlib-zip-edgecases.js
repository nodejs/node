'use strict';

// Malformed / adversarial / boundary archives, drawn from the ZIP
// parser-differential and torture-test literature (Go archive/zip testdata,
// USENIX'25 semantic-gap paper, PyPI/uv ZIP-confusion advisories, Info-ZIP).
// Node core does not extract to disk, so path-traversal safety is the caller's
// job - these tests pin that our reader surfaces names verbatim and resolves
// every ambiguity from the authoritative central directory.

require('../common');

const assert = require('node:assert');
const zlib = require('node:zlib');
const { test } = require('node:test');

const SIG_LOCAL = 0x04034b50;
const SIG_CENTRAL = 0x02014b50;
const SIG_EOCD = 0x06054b50;

async function buildArchive(entries, comment) {
  const chunks = [];
  for await (const c of zlib.createZipArchive(entries, comment)) chunks.push(c);
  return Buffer.concat(chunks);
}

// One-entry (stored) archive with independent control of the local vs central
// name, flags, method and sizes - the levers ZIP-confusion attacks pull.
function buildEntryArchive(opts) {
  const name = opts.nameBuffer;
  const localName = opts.localNameBuffer ?? name;
  const content = opts.content ?? Buffer.alloc(0);
  const flags = opts.flags ?? 0;
  const method = opts.method ?? 0;
  const localExtra = opts.localExtra ?? Buffer.alloc(0);
  const centralExtra = opts.centralExtra ?? Buffer.alloc(0);
  const crc = zlib.crc32(content) >>> 0;

  const local = Buffer.alloc(30);
  local.writeUInt32LE(SIG_LOCAL, 0);
  local.writeUInt16LE(20, 4);
  local.writeUInt16LE(flags, 6);
  local.writeUInt16LE(method, 8);
  local.writeUInt16LE(0x21, 12);
  local.writeUInt32LE(crc, 14);
  local.writeUInt32LE(content.length, 18);
  local.writeUInt32LE(content.length, 22);
  local.writeUInt16LE(localName.length, 26);
  local.writeUInt16LE(localExtra.length, 28);
  const localRecord = Buffer.concat([local, localName, localExtra, content]);

  const central = Buffer.alloc(46);
  central.writeUInt32LE(SIG_CENTRAL, 0);
  central.writeUInt16LE(0x0314, 4);
  central.writeUInt16LE(20, 6);
  central.writeUInt16LE(flags, 8);
  central.writeUInt16LE(method, 10);
  central.writeUInt16LE(0x21, 14);
  central.writeUInt32LE(crc, 16);
  central.writeUInt32LE(content.length, 20);
  central.writeUInt32LE(content.length, 24);
  central.writeUInt16LE(name.length, 28);
  central.writeUInt16LE(centralExtra.length, 30);
  central.writeUInt32LE(0, 42);
  const centralRecord = Buffer.concat([central, name, centralExtra]);

  const eocd = Buffer.alloc(22);
  eocd.writeUInt32LE(SIG_EOCD, 0);
  eocd.writeUInt16LE(1, 8);
  eocd.writeUInt16LE(1, 10);
  eocd.writeUInt32LE(centralRecord.length, 12);
  eocd.writeUInt32LE(localRecord.length, 16);
  return Buffer.concat([localRecord, centralRecord, eocd]);
}

// -- parser-confusion / semantic-gap defenses --------------------------------

test('the central directory is authoritative when the local header disagrees', () => {
  // Local header claims "fake.txt"; central directory says "real.txt".
  const archive = buildEntryArchive({
    nameBuffer: Buffer.from('real.txt'),
    localNameBuffer: Buffer.from('fake.txt'),
    content: Buffer.from('payload'),
  });
  const [entry] = zlib.ZipEntry.read(archive);
  assert.strictEqual(entry.name, 'real.txt');
  assert.strictEqual(entry.contentSync().toString(), 'payload');
});

test('duplicate central-directory names: read() yields all, get() takes the last', async () => {
  const archive = await buildArchive([
    await zlib.ZipEntry.create('dup.txt', Buffer.from('first')),
    await zlib.ZipEntry.create('dup.txt', Buffer.from('second')),
  ]);
  assert.deepStrictEqual([...zlib.ZipEntry.read(archive)].map((e) => e.name), ['dup.txt', 'dup.txt']);
  using zip = new zlib.ZipBuffer(archive);
  assert.strictEqual(zip.size, 1);
  assert.strictEqual(zip.get('dup.txt').contentSync().toString(), 'second');
});

test('path-unsafe names are surfaced verbatim, never normalized or rejected', () => {
  for (const name of ['../../etc/passwd', '/etc/passwd', 'a\\b\\c.txt', 'C:\\evil.dll']) {
    const [entry] = zlib.ZipEntry.read(buildEntryArchive({ nameBuffer: Buffer.from(name) }));
    assert.strictEqual(entry.name, name);
  }
});

// -- structural boundaries ----------------------------------------------------

test('data prepended before the archive (SFX stub) is tolerated via prefix detection', async () => {
  const inner = await buildArchive([
    await zlib.ZipEntry.create('a.txt', Buffer.from('hello')),
    await zlib.ZipEntry.create('b.txt', Buffer.from('world')),
  ]);
  const prefixed = Buffer.concat([Buffer.alloc(1000, 0x7f), inner]); // stub bytes
  using zip = new zlib.ZipBuffer(prefixed);
  assert.deepStrictEqual([...zip.keys()].sort(), ['a.txt', 'b.txt']);
  assert.strictEqual(zip.get('a.txt').contentSync().toString(), 'hello');
});

test('an empty archive (EOCD only) reads as zero entries', () => {
  const eocd = Buffer.alloc(22);
  eocd.writeUInt32LE(SIG_EOCD, 0);
  assert.deepStrictEqual([...zlib.ZipEntry.read(eocd)], []);
  using zip = new zlib.ZipBuffer(eocd);
  assert.strictEqual(zip.size, 0);
});

test('directory entries and zero-byte file entries round-trip', async () => {
  const archive = await buildArchive([
    await zlib.ZipEntry.create('dir/', Buffer.alloc(0)),
    await zlib.ZipEntry.create('empty.txt', Buffer.alloc(0)),
  ]);
  using zip = new zlib.ZipBuffer(archive);
  assert.strictEqual(zip.get('dir/').isDirectory, true);
  const empty = zip.get('empty.txt');
  assert.strictEqual(empty.isDirectory, false);
  assert.strictEqual(empty.contentSync().length, 0);
  assert.strictEqual(empty.crc32, 0);
});

test('an archive comment at the maximum length (65535 bytes) round-trips', async () => {
  const comment = 'z'.repeat(0xffff);
  const archive = await buildArchive([await zlib.ZipEntry.create('a.txt', Buffer.from('x'))], comment);
  using zip = new zlib.ZipBuffer(archive);
  assert.strictEqual(zip.comment.length, 0xffff);
  assert.strictEqual(zip.get('a.txt').contentSync().toString(), 'x');
});

// -- unsupported features are rejected, not misread --------------------------

test('a traditional-encrypted entry (bit 0) is rejected', () => {
  const [entry] = zlib.ZipEntry.read(buildEntryArchive({
    nameBuffer: Buffer.from('f'),
    content: Buffer.from('secret'),
    flags: 0x0001,
  }));
  assert.throws(() => entry.contentSync(), { code: 'ERR_ZIP_UNSUPPORTED_FEATURE' });
});

test('a WinZip-AES entry (method 99, bit 0, 0x9901 extra) is rejected as encrypted', () => {
  const aes = Buffer.concat([
    Buffer.from([0x01, 0x99]),             // extra id 0x9901
    Buffer.from([0x07, 0x00]),             // size 7
    Buffer.from([0x01, 0x00]),             // AE-1 version
    Buffer.from([0x41, 0x45]),             // "AE"
    Buffer.from([0x03]),                   // AES-256
    Buffer.from([0x00, 0x00]),             // Actual method (store)
  ]);
  const [entry] = zlib.ZipEntry.read(buildEntryArchive({
    nameBuffer: Buffer.from('f'),
    content: Buffer.from('cipher'),
    flags: 0x0001,
    method: 99,
    centralExtra: aes,
    localExtra: aes,
  }));
  assert.throws(() => entry.contentSync(), { code: 'ERR_ZIP_UNSUPPORTED_FEATURE' });
});

test('a Zip64 sentinel size with no Zip64 extra field is rejected', () => {
  const archive = buildEntryArchive({ nameBuffer: Buffer.from('f'), content: Buffer.from('hi') });
  // Force the central uncompressed-size field to the sentinel without adding a
  // Zip64 extra field, so "look in the Zip64 record" points at nothing.
  const cd = archive.readUInt32LE(archive.length - 22 + 16);
  archive.writeUInt32LE(0xffffffff, cd + 24);
  assert.throws(() => [...zlib.ZipEntry.read(archive)][0].size,
                { code: 'ERR_ZIP_INVALID_ARCHIVE' });
});
