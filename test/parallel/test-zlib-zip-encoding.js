'use strict';

// Reading foreign-encoder edge cases faithfully: non-UTF-8 (CP437) and
// Unicode-Path-extra filenames, raw name bytes, full Unix mode bits and
// symlink type, and modification times carried in extra fields rather than
// the coarse DOS date/time. These synthesize the exact header bytes real
// tools (Windows Explorer, Info-ZIP, 7-Zip, WinRAR) emit, since our own
// writer only ever produces UTF-8 names and DOS timestamps.

require('../common');

const assert = require('node:assert');
const zlib = require('node:zlib');
const { test } = require('node:test');

const SIG_LOCAL = 0x04034b50;
const SIG_CENTRAL = 0x02014b50;
const SIG_EOCD = 0x06054b50;

// Build a minimal one-entry (stored) archive with full control over every
// header field, so tests can reproduce exactly what a foreign encoder writes.
function buildEntryArchive(opts) {
  const name = opts.nameBuffer;
  const content = opts.content ?? Buffer.alloc(0);
  const flags = opts.flags ?? 0;
  const localExtra = opts.localExtra ?? Buffer.alloc(0);
  const centralExtra = opts.centralExtra ?? Buffer.alloc(0);
  const external = (opts.externalAttrs ?? 0) >>> 0;
  const versionMadeBy = opts.versionMadeBy ?? 0x0314; // host 3 (Unix), v2.0
  const dosTime = opts.dosTime ?? 0;
  const dosDate = opts.dosDate ?? 0x21; // 1980-01-01
  const crc = zlib.crc32(content) >>> 0;

  const local = Buffer.alloc(30);
  local.writeUInt32LE(SIG_LOCAL, 0);
  local.writeUInt16LE(20, 4);
  local.writeUInt16LE(flags, 6);
  local.writeUInt16LE(0, 8); // store
  local.writeUInt16LE(dosTime, 10);
  local.writeUInt16LE(dosDate, 12);
  local.writeUInt32LE(crc, 14);
  local.writeUInt32LE(content.length, 18);
  local.writeUInt32LE(content.length, 22);
  local.writeUInt16LE(name.length, 26);
  local.writeUInt16LE(localExtra.length, 28);
  const localRecord = Buffer.concat([local, name, localExtra, content]);

  const central = Buffer.alloc(46);
  central.writeUInt32LE(SIG_CENTRAL, 0);
  central.writeUInt16LE(versionMadeBy, 4);
  central.writeUInt16LE(20, 6);
  central.writeUInt16LE(flags, 8);
  central.writeUInt16LE(0, 10);
  central.writeUInt16LE(dosTime, 12);
  central.writeUInt16LE(dosDate, 14);
  central.writeUInt32LE(crc, 16);
  central.writeUInt32LE(content.length, 20);
  central.writeUInt32LE(content.length, 24);
  central.writeUInt16LE(name.length, 28);
  central.writeUInt16LE(centralExtra.length, 30);
  central.writeUInt16LE(0, 32); // comment length
  central.writeUInt16LE(0, 34);
  central.writeUInt16LE(0, 36);
  central.writeUInt32LE(external, 38);
  central.writeUInt32LE(0, 42); // local header offset
  const centralRecord = Buffer.concat([central, name, centralExtra]);

  const eocd = Buffer.alloc(22);
  eocd.writeUInt32LE(SIG_EOCD, 0);
  eocd.writeUInt16LE(1, 8);
  eocd.writeUInt16LE(1, 10);
  eocd.writeUInt32LE(centralRecord.length, 12);
  eocd.writeUInt32LE(localRecord.length, 16);
  return Buffer.concat([localRecord, centralRecord, eocd]);
}

// -- filename encoding --------------------------------------------------------

test('a bit-11-clear name is decoded as CP437, not mangled as UTF-8', () => {
  // 0x81 is "ü" in CP437; as a lone UTF-8 byte it is invalid (would be U+FFFD).
  const nameBuffer = Buffer.from([0x63, 0x61, 0x66, 0x81]); // "caf" + ü
  const [entry] = zlib.ZipEntry.read(buildEntryArchive({ nameBuffer }));
  assert.strictEqual(entry.name, 'cafü');
  assert.deepStrictEqual(entry.nameBuffer, nameBuffer);
});

test('a bit-11-set name is decoded as UTF-8, with raw bytes on nameBuffer', () => {
  const nameBuffer = Buffer.from('café-名前.txt', 'utf8');
  const [entry] = zlib.ZipEntry.read(buildEntryArchive({ nameBuffer, flags: 0x0800 }));
  assert.strictEqual(entry.name, 'café-名前.txt');
  assert.deepStrictEqual(entry.nameBuffer, nameBuffer);
});

test('a valid Unicode Path extra field (0x7075) overrides the CP437 name', () => {
  const cp437Name = Buffer.from([0x63, 0x61, 0x66, 0x81]); // "cafü" in CP437
  const utf8 = Buffer.from('café.txt', 'utf8');
  const up = Buffer.concat([
    Buffer.from([0x75, 0x70]),                 // id 0x7075
    (() => { const b = Buffer.alloc(2); b.writeUInt16LE(5 + utf8.length, 0); return b; })(),
    Buffer.from([0x01]),                       // version 1
    (() => { const b = Buffer.alloc(4); b.writeUInt32LE(zlib.crc32(cp437Name) >>> 0, 0); return b; })(),
    utf8,
  ]);
  const [entry] = zlib.ZipEntry.read(buildEntryArchive({ nameBuffer: cp437Name, centralExtra: up }));
  assert.strictEqual(entry.name, 'café.txt');
});

test('a Unicode Path extra field with a stale CRC is ignored (falls back to CP437)', () => {
  const cp437Name = Buffer.from([0x63, 0x61, 0x66, 0x81]);
  const utf8 = Buffer.from('renamed.txt', 'utf8');
  const up = Buffer.concat([
    Buffer.from([0x75, 0x70]),
    (() => { const b = Buffer.alloc(2); b.writeUInt16LE(5 + utf8.length, 0); return b; })(),
    Buffer.from([0x01]),
    Buffer.from([0xde, 0xad, 0xbe, 0xef]),     // wrong CRC of the standard name
    utf8,
  ]);
  const [entry] = zlib.ZipEntry.read(buildEntryArchive({ nameBuffer: cp437Name, centralExtra: up }));
  assert.strictEqual(entry.name, 'cafü'); // stale extra ignored
});

test('ZipBuffer.get() keys by the decoded (CP437) name', () => {
  const nameBuffer = Buffer.from([0x81, 0x2e, 0x74, 0x78, 0x74]); // "ü.txt"
  using zip = new zlib.ZipBuffer(buildEntryArchive({ nameBuffer, content: Buffer.from('x') }));
  assert.strictEqual(zip.has('ü.txt'), true);
  assert.strictEqual(zip.get('ü.txt').contentSync().toString(), 'x');
});

// -- Unix mode + symlink ------------------------------------------------------

test('setuid/setgid/sticky bits round-trip through a read', async () => {
  for (const mode of [0o4755, 0o2750, 0o1777, 0o755, 0o640]) {
    const entry = await zlib.ZipEntry.create('f', Buffer.from('x'), { mode });
    const chunks = [];
    for await (const c of zlib.createZipArchive([entry])) chunks.push(c);
    const [read] = zlib.ZipEntry.read(Buffer.concat(chunks));
    assert.strictEqual(read.mode, mode, `0o${mode.toString(8)} -> 0o${read.mode.toString(8)}`);
  }
});

test('a symlink entry is reported as a symlink, not a file', () => {
  const target = Buffer.from('../target');
  const S_IFLNK = 0o120000;
  const archive = buildEntryArchive({
    nameBuffer: Buffer.from('link'),
    content: target,
    externalAttrs: ((S_IFLNK | 0o777) << 16) >>> 0,
  });
  const [entry] = zlib.ZipEntry.read(archive);
  assert.strictEqual(entry.isSymlink, true);
  assert.strictEqual(entry.isFile, false);
  assert.strictEqual(entry.isDirectory, false);
  assert.strictEqual(entry.mode, 0o777);
  assert.strictEqual(entry.contentSync().toString(), '../target');
});

test('external attributes from a non-Unix host expose no mode and no symlink', () => {
  const S_IFLNK = 0o120000;
  const archive = buildEntryArchive({
    nameBuffer: Buffer.from('x'),
    externalAttrs: ((S_IFLNK | 0o777) << 16) >>> 0, // Looks like a symlink...
    versionMadeBy: 0x0014, // ...but host 0 (FAT/DOS), so not Unix mode
  });
  const [entry] = zlib.ZipEntry.read(archive);
  assert.strictEqual(entry.mode, 0);
  assert.strictEqual(entry.isSymlink, false);
});

// -- modification time from extra fields --------------------------------------

// 2017-10-31T21:11:57Z, deliberately not representable in the 2-second DOS
// field so an extra-field time can be told apart from the DOS fallback.
const MTIME_SECS = 1509484317;

function extField(id, body) {
  const head = Buffer.alloc(4);
  head.writeUInt16LE(id, 0);
  head.writeUInt16LE(body.length, 2);
  return Buffer.concat([head, body]);
}

test('the extended-timestamp extra field (0x5455) sets the modification time', () => {
  const body = Buffer.alloc(5);
  body.writeUInt8(0x01, 0); // mtime present
  body.writeInt32LE(MTIME_SECS, 1);
  const [entry] = zlib.ZipEntry.read(buildEntryArchive({
    nameBuffer: Buffer.from('f'),
    centralExtra: extField(0x5455, body),
    dosDate: 0x4a21, // A different (2017) DOS date, to prove the extra wins
  }));
  assert.strictEqual(entry.modified.getTime(), MTIME_SECS * 1000);
});

test('the NTFS extra field (0x000a) sets a high-resolution modification time', () => {
  const ns100 = (BigInt(MTIME_SECS) + 11644473600n) * 10000000n;
  const times = Buffer.alloc(24);
  times.writeBigUInt64LE(ns100, 0);  // mtime
  times.writeBigUInt64LE(ns100, 8);  // atime
  times.writeBigUInt64LE(ns100, 16); // ctime
  const body = Buffer.concat([
    Buffer.alloc(4),               // reserved
    extField(0x0001, times),       // attribute tag 1
  ]);
  const [entry] = zlib.ZipEntry.read(buildEntryArchive({
    nameBuffer: Buffer.from('f'),
    localExtra: extField(0x000a, body), // NTFS times live in the local header
  }));
  assert.strictEqual(entry.modified.getTime(), MTIME_SECS * 1000);
});

test('the Info-ZIP Unix extra field (0x5855) sets the modification time', () => {
  const body = Buffer.alloc(8);
  body.writeInt32LE(1, 0);          // atime
  body.writeInt32LE(MTIME_SECS, 4); // mtime
  const [entry] = zlib.ZipEntry.read(buildEntryArchive({
    nameBuffer: Buffer.from('f'),
    centralExtra: extField(0x5855, body),
  }));
  assert.strictEqual(entry.modified.getTime(), MTIME_SECS * 1000);
});

test('NTFS time is preferred over the extended timestamp when both are present', () => {
  const extBody = Buffer.alloc(5);
  extBody.writeUInt8(0x01, 0);
  extBody.writeInt32LE(MTIME_SECS - 3600, 1); // an hour earlier
  const ns100 = (BigInt(MTIME_SECS) + 11644473600n) * 10000000n;
  const times = Buffer.alloc(24);
  times.writeBigUInt64LE(ns100, 0);
  const ntfsBody = Buffer.concat([Buffer.alloc(4), extField(0x0001, times)]);
  const [entry] = zlib.ZipEntry.read(buildEntryArchive({
    nameBuffer: Buffer.from('f'),
    localExtra: Buffer.concat([extField(0x000a, ntfsBody), extField(0x5455, extBody)]),
  }));
  assert.strictEqual(entry.modified.getTime(), MTIME_SECS * 1000);
});

test('with no timestamp extra field the DOS date/time is used', () => {
  // DOS date 0x4f21 = year 1980+((0x4f21>>9)&0x7f)=1980+39=2019, month 1, day 1.
  const [entry] = zlib.ZipEntry.read(buildEntryArchive({
    nameBuffer: Buffer.from('f'),
    dosDate: 0x4f21,
  }));
  assert.strictEqual(entry.modified.getFullYear(), 2019);
});
