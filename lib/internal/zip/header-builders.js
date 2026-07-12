'use strict';

// Write-path header builders: local/central file headers, the Zip64 data
// descriptor, and the (Zip64) end-of-central-directory records, plus the
// version-needed and extra-length helpers they share.

const {
  ArrayPrototypePush,
  MathMax,
  MathMin,
} = primordials;

const {
  codes: {
    ERR_ZIP_ENTRY_TOO_LARGE,
  },
} = require('internal/errors');
const { Buffer } = require('buffer');
const {
  EMPTY_BUFFER,
  SIG_LOCAL_FILE_HEADER,
  SIG_DATA_DESCRIPTOR,
  SIG_CENTRAL_FILE_HEADER,
  SIG_ZIP64_EOCD_RECORD,
  SIG_ZIP64_EOCD_LOCATOR,
  SIG_EOCD,
  MADE_BY_UNIX,
  ZIP64_EXTRA_ID,
  SENTINEL16,
  SENTINEL32,
  FLAG_DATA_DESCRIPTOR,
  METHOD_ZSTD,
  VERSION_DEFAULT,
  VERSION_ZIP64,
  VERSION_ZSTD,
} = require('internal/zip/constants');
const { encodeDosDateTime } = require('internal/zip/dos');
const { writeSafeUint64 } = require('internal/zip/binary');
const { buildExtTimestampExtra } = require('internal/zip/extra-fields');

// The "version needed to extract" (sec. 4.4.3): the highest version any
// feature of the member demands - 6.3 for Zstandard, 4.5 for Zip64
// structures, 2.0 otherwise.
function versionNeeded(meta, zip64) {
  return MathMax(
    zip64 ? VERSION_ZIP64 : VERSION_DEFAULT,
    meta.method === METHOD_ZSTD ? VERSION_ZSTD : 0);
}

// Guard the combined extra-field length against the 16-bit field it is stored
// in (sec. 4.4.11).
function checkExtraLength(extraLength) {
  if (extraLength > SENTINEL16) {
    throw new ERR_ZIP_ENTRY_TOO_LARGE(
      'the entry extra fields must not exceed 65535 bytes');
  }
}

// Build a local file header (sec. 4.3.7). Streamed entries (unknown sizes/CRC)
// zero those fields and always carry a Zip64 extra so the trailing data
// descriptor can hold 64-bit sizes; oversized non-streamed entries get one too.
function buildLocalHeader(meta) {
  const streaming = (meta.flags & FLAG_DATA_DESCRIPTOR) !== 0;
  const zip64 =
    streaming ||
    meta.compressedSize >= SENTINEL32 ||
    meta.uncompressedSize >= SENTINEL32;
  const ts = meta.extendedMtime !== null ? buildExtTimestampExtra(meta.extendedMtime) : EMPTY_BUFFER;
  const extraLength = (zip64 ? 20 : 0) + ts.length + meta.extra.length;
  checkExtraLength(extraLength);
  const buffer = Buffer.allocUnsafe(30 + meta.name.length + extraLength);
  buffer.writeUInt32LE(SIG_LOCAL_FILE_HEADER, 0);
  buffer.writeUInt16LE(versionNeeded(meta, zip64), 4);
  buffer.writeUInt16LE(meta.flags, 6);
  buffer.writeUInt16LE(meta.method, 8);
  const { time, date } = encodeDosDateTime(meta.modified);
  buffer.writeUInt16LE(time, 10);
  buffer.writeUInt16LE(date, 12);
  buffer.writeUInt32LE(streaming ? 0 : meta.crc, 14);
  buffer.writeUInt32LE(zip64 ? SENTINEL32 : meta.compressedSize, 18);
  buffer.writeUInt32LE(zip64 ? SENTINEL32 : meta.uncompressedSize, 22);
  buffer.writeUInt16LE(meta.name.length, 26);
  buffer.writeUInt16LE(extraLength, 28);
  meta.name.copy(buffer, 30);
  let pos = 30 + meta.name.length;
  if (zip64) {
    buffer.writeUInt16LE(ZIP64_EXTRA_ID, pos);
    buffer.writeUInt16LE(16, pos + 2);
    writeSafeUint64(buffer, pos + 4, streaming ? 0 : meta.uncompressedSize);
    writeSafeUint64(buffer, pos + 12, streaming ? 0 : meta.compressedSize);
    pos += 20;
  }
  ts.copy(buffer, pos);
  pos += ts.length;
  meta.extra.copy(buffer, pos);
  return buffer;
}

// Build a central directory file header (sec. 4.3.12). Each of the
// uncompressed size, compressed size, and local-header offset that overflows
// 32 bits is moved into the Zip64 extra field (sec. 4.5.3) in that order.
function buildCentralHeader(meta, localOffset) {
  const zip64Streaming = (meta.flags & FLAG_DATA_DESCRIPTOR) !== 0;
  const u64 = meta.uncompressedSize >= SENTINEL32;
  const c64 = meta.compressedSize >= SENTINEL32;
  const o64 = localOffset >= SENTINEL32;
  const zip64Fields = (u64 ? 1 : 0) + (c64 ? 1 : 0) + (o64 ? 1 : 0);
  const ts = meta.extendedMtime !== null ? buildExtTimestampExtra(meta.extendedMtime) : EMPTY_BUFFER;
  const extraLength = (zip64Fields ? 4 + 8 * zip64Fields : 0) + ts.length + meta.extra.length;
  checkExtraLength(extraLength);
  const zip64 = zip64Streaming || zip64Fields > 0;
  const version = versionNeeded(meta, zip64);
  const buffer = Buffer.allocUnsafe(
    46 + meta.name.length + extraLength + meta.comment.length);
  buffer.writeUInt32LE(SIG_CENTRAL_FILE_HEADER, 0);
  // "Version made by" (sec. 4.4.2): the upper byte is the creator's host
  // system, which determines how the external attributes (sec. 4.4.15) are
  // interpreted. Preserve the original host for round-tripped entries -
  // stamping everything as Unix would turn, say, a DOS entry's zeroed high
  // bits into Unix mode 0000. Entries built by `createEntryMeta()` are Unix.
  buffer.writeUInt16LE((meta.madeBy << 8) | version, 4);
  buffer.writeUInt16LE(version, 6);
  buffer.writeUInt16LE(meta.flags, 8);
  buffer.writeUInt16LE(meta.method, 10);
  const { time, date } = encodeDosDateTime(meta.modified);
  buffer.writeUInt16LE(time, 12);
  buffer.writeUInt16LE(date, 14);
  buffer.writeUInt32LE(meta.crc, 16);
  buffer.writeUInt32LE(c64 ? SENTINEL32 : meta.compressedSize, 20);
  buffer.writeUInt32LE(u64 ? SENTINEL32 : meta.uncompressedSize, 24);
  buffer.writeUInt16LE(meta.name.length, 28);
  buffer.writeUInt16LE(extraLength, 30);
  buffer.writeUInt16LE(meta.comment.length, 32);
  buffer.writeUInt16LE(0, 34); // disk number
  buffer.writeUInt16LE(meta.internal, 36);
  buffer.writeUInt32LE(meta.external, 38);
  buffer.writeUInt32LE(o64 ? SENTINEL32 : localOffset, 42);
  meta.name.copy(buffer, 46);
  let pos = 46 + meta.name.length;
  if (zip64Fields) {
    buffer.writeUInt16LE(ZIP64_EXTRA_ID, pos);
    buffer.writeUInt16LE(8 * zip64Fields, pos + 2);
    pos += 4;
    if (u64) {
      writeSafeUint64(buffer, pos, meta.uncompressedSize);
      pos += 8;
    }
    if (c64) {
      writeSafeUint64(buffer, pos, meta.compressedSize);
      pos += 8;
    }
    if (o64) {
      writeSafeUint64(buffer, pos, localOffset);
      pos += 8;
    }
  }
  ts.copy(buffer, pos);
  pos += ts.length;
  meta.extra.copy(buffer, pos);
  pos += meta.extra.length;
  meta.comment.copy(buffer, pos);
  return buffer;
}

// Zip64 data descriptor (sec. 4.3.9): emitted after a streamed entry, whose
// local header always carries a Zip64 extra field.
function buildDataDescriptor64(crc, compressedSize, uncompressedSize) {
  const buffer = Buffer.allocUnsafe(24);
  buffer.writeUInt32LE(SIG_DATA_DESCRIPTOR, 0);
  buffer.writeUInt32LE(crc, 4);
  writeSafeUint64(buffer, 8, compressedSize);
  writeSafeUint64(buffer, 16, uncompressedSize);
  return buffer;
}

// Build the end of central directory record (sec. 4.3.16); fields that
// overflow their 16/32-bit slots are written as sentinels and the true values
// live in the Zip64 EOCD record.
function buildEndOfCentralDirectory(count, size, offset, comment) {
  const buffer = Buffer.allocUnsafe(22 + comment.length);
  buffer.writeUInt32LE(SIG_EOCD, 0);
  buffer.writeUInt16LE(0, 4); // disk number
  buffer.writeUInt16LE(0, 6); // Central directory disk number
  buffer.writeUInt16LE(MathMin(count, SENTINEL16), 8);
  buffer.writeUInt16LE(MathMin(count, SENTINEL16), 10);
  buffer.writeUInt32LE(MathMin(size, SENTINEL32), 12);
  buffer.writeUInt32LE(MathMin(offset, SENTINEL32), 16);
  buffer.writeUInt16LE(comment.length, 20);
  comment.copy(buffer, 22);
  return buffer;
}

// Build the Zip64 end of central directory record (sec. 4.3.14): the 64-bit
// counterpart to the EOCD, holding the real record count/size/offset.
function buildZip64EndRecord(count, size, offset) {
  const buffer = Buffer.allocUnsafe(56);
  buffer.writeUInt32LE(SIG_ZIP64_EOCD_RECORD, 0);
  writeSafeUint64(buffer, 4, 44); // Size of the remainder of this record
  buffer.writeUInt16LE((MADE_BY_UNIX << 8) | VERSION_ZIP64, 12);
  buffer.writeUInt16LE(VERSION_ZIP64, 14);
  buffer.writeUInt32LE(0, 16); // disk number
  buffer.writeUInt32LE(0, 20); // Central directory disk number
  writeSafeUint64(buffer, 24, count);
  writeSafeUint64(buffer, 32, count);
  writeSafeUint64(buffer, 40, size);
  writeSafeUint64(buffer, 48, offset);
  return buffer;
}

// Build the Zip64 end of central directory locator (sec. 4.3.15): points the
// reader from just before the EOCD to the Zip64 EOCD record.
function buildZip64EndLocator(recordOffset) {
  const buffer = Buffer.allocUnsafe(20);
  buffer.writeUInt32LE(SIG_ZIP64_EOCD_LOCATOR, 0);
  buffer.writeUInt32LE(0, 4); // Disk with the Zip64 EOCD record
  writeSafeUint64(buffer, 8, recordOffset);
  buffer.writeUInt32LE(1, 16); // total disks
  return buffer;
}

// The archive trailer: the Zip64 EOCD record and locator
// (sec. 4.3.14/4.3.15) when the record count, central directory offset, or
// central directory size overflows its classic 16-/32-bit field, followed
// by the end of central directory record (sec. 4.3.16). Shared by the
// streaming serializers and ZipFile's in-place rewrite so the Zip64
// switchover rule lives in exactly one place.
function buildArchiveTrailer(count, centralDirectorySize, centralDirectoryOffset, comment) {
  const chunks = [];
  const zip64 =
    count >= SENTINEL16 ||
    centralDirectoryOffset >= SENTINEL32 ||
    centralDirectorySize >= SENTINEL32;
  if (zip64) {
    const recordOffset = centralDirectoryOffset + centralDirectorySize;
    ArrayPrototypePush(chunks, buildZip64EndRecord(count, centralDirectorySize, centralDirectoryOffset));
    ArrayPrototypePush(chunks, buildZip64EndLocator(recordOffset));
  }
  ArrayPrototypePush(chunks,
                     buildEndOfCentralDirectory(count, centralDirectorySize, centralDirectoryOffset, comment));
  return chunks;
}

module.exports = {
  buildLocalHeader,
  buildCentralHeader,
  buildDataDescriptor64,
  buildEndOfCentralDirectory,
  buildZip64EndRecord,
  buildZip64EndLocator,
  buildArchiveTrailer,
};
