'use strict';

// TLV extra-field parsing and building (sec. 4.5): the generic record walker,
// the Zip64 extended-information field, modification-time extras
// (NTFS/UT/UX), the round-trip-preserving strip of Zip64 records, and the
// extended-timestamp builder used on the write path.

const {
  ArrayPrototypePush,
  Date,
  Number,
} = primordials;

const {
  codes: {
    ERR_ZIP_INVALID_ARCHIVE,
  },
} = require('internal/errors');
const { Buffer } = require('buffer');
const {
  EMPTY_BUFFER,
  ZIP64_EXTRA_ID,
  EXTRA_ID_NTFS,
  EXTRA_ID_EXT_TIMESTAMP,
  EXTRA_ID_UNIX_OLD,
} = require('internal/zip/constants');
const { readSafeUint64 } = require('internal/zip/binary');

// Zip64 extended information extra field (sec. 4.5.3). Walks the TLV records
// itself, and unlike forEachExtraField() below it throws on a malformed
// record: it only runs when a classic header field holds an overflow
// sentinel, so the Zip64 record is required and a malformed extra field
// hides required data.
function parseZip64Extra(extra, want) {
  const wanted =
    want.uncompressedSize ||
    want.compressedSize ||
    want.localFileHeaderOffset ||
    want.diskNumber;
  if (!wanted) return {};
  let pos = 0;
  while (pos + 4 <= extra.length) {
    const id = extra.readUInt16LE(pos);
    const size = extra.readUInt16LE(pos + 2);
    if (pos + 4 + size > extra.length) {
      throw new ERR_ZIP_INVALID_ARCHIVE('extra field is malformed');
    }
    if (id === ZIP64_EXTRA_ID) {
      const result = {};
      const body = pos + 4;
      const end = body + size;
      // APPNOTE 4.5.3: the field order is fixed - uncompressed size (8),
      // compressed size (8), local header offset (8), disk number (4) - and
      // each field MUST appear only when the corresponding classic field
      // holds its overflow sentinel. When the record length matches exactly
      // the fields the sentinels call for, parse them packed per spec.
      // Real-world writers violate the "only" rule and emit fields for
      // non-sentinel values too (commonly all four); such a record is longer
      // than the wanted set, and is instead parsed positionally against the
      // full layout - the fixed order makes both readings unambiguous.
      const wantedSize =
        (want.uncompressedSize ? 8 : 0) +
        (want.compressedSize ? 8 : 0) +
        (want.localFileHeaderOffset ? 8 : 0) +
        (want.diskNumber ? 4 : 0);
      const packed = size === wantedSize;
      let cursor = body;
      const take = (fullLayoutOffset, bytes) => {
        const at = packed ? cursor : body + fullLayoutOffset;
        if (at + bytes > end) {
          throw new ERR_ZIP_INVALID_ARCHIVE(
            'the Zip64 extended information extra field is truncated');
        }
        cursor += bytes;
        return bytes === 8 ? readSafeUint64(extra, at) : extra.readUInt32LE(at);
      };
      if (want.uncompressedSize) result.uncompressedSize = take(0, 8);
      if (want.compressedSize) result.compressedSize = take(8, 8);
      if (want.localFileHeaderOffset) result.localFileHeaderOffset = take(16, 8);
      if (want.diskNumber) result.diskNumber = take(24, 4);
      return result;
    }
    pos += 4 + size;
  }
  throw new ERR_ZIP_INVALID_ARCHIVE(
    'a field is 0xFFFFFFFF but the Zip64 extended information extra field is missing');
}

// Walk TLV extra-field records - id(2), size(2), body(size) - invoking `cb`
// per record. Stops silently at the first malformed/overrunning record -
// deliberately laxer than parseZip64Extra() above: this walker only feeds
// advisory metadata (timestamps, Unicode names), and a malformed extra
// should not make the archive unreadable.
function forEachExtraField(extra, cb) {
  let pos = 0;
  while (pos + 4 <= extra.length) {
    const id = extra.readUInt16LE(pos);
    const size = extra.readUInt16LE(pos + 2);
    if (pos + 4 + size > extra.length) break;
    cb(id, extra.subarray(pos + 4, pos + 4 + size));
    pos += 4 + size;
  }
}

// The mtime from an NTFS extra field (id 0x000a, sec. 4.5.5): a reserved
// dword then tagged sub-records; tag 1 carries the FILETIME mtime/atime/ctime.
function parseNtfsMtime(body) {
  let result = null;
  let pos = 4; // Skip the reserved dword.
  while (pos + 4 <= body.length) {
    const tag = body.readUInt16LE(pos);
    const size = body.readUInt16LE(pos + 2);
    if (pos + 4 + size > body.length) break;
    if (tag === 1 && size >= 8) {
      // Windows FILETIME: 100 ns ticks since 1601-01-01 UTC.
      const ticks = body.readBigUInt64LE(pos + 4);
      result = new Date(Number(ticks / 10000n) - 11644473600000);
    }
    pos += 4 + size;
  }
  return result;
}

// The mtime from an Info-ZIP extended timestamp extra field ("UT", 0x5455;
// listed in APPNOTE's third-party ID table sec. 4.6.1, format defined in
// Info-ZIP's extrafld.txt).
function parseExtTimestampMtime(body) {
  // flags(1) then present times; bit 0 => mtime present (signed Unix seconds).
  if (body.length < 5 || (body[0] & 1) === 0) return null;
  return new Date(body.readInt32LE(1) * 1000);
}

// The mtime from an Info-ZIP original Unix extra field ("UX", id 0x5855).
function parseUnixOldMtime(body) {
  // atime(4), mtime(4) as signed Unix seconds (uid/gid follow only locally).
  if (body.length < 8) return null;
  return new Date(body.readInt32LE(4) * 1000);
}

// The highest-fidelity modification time carried in the given extra fields, or
// null when none is present. NTFS (100 ns) beats the extended timestamp (1 s,
// UTC) beats Info-ZIP Unix (1 s); all are absolute instants, unlike the coarse
// local-time DOS date/time fields.
function extraFieldMtime(...extras) {
  let ntfs = null;
  let ext = null;
  let unix = null;
  for (const extra of extras) {
    if (!extra?.length) continue;
    forEachExtraField(extra, (id, body) => {
      if (id === EXTRA_ID_NTFS) ntfs ??= parseNtfsMtime(body);
      else if (id === EXTRA_ID_EXT_TIMESTAMP) ext ??= parseExtTimestampMtime(body);
      else if (id === EXTRA_ID_UNIX_OLD) unix ??= parseUnixOldMtime(body);
    });
  }
  return ntfs ?? ext ?? unix;
}

// A copy of `extra` without any Zip64 record (id 0x0001): Zip64 data is
// regenerated from the final sizes/offsets on serialization, but every other
// record (Unicode Path, NTFS/UT timestamps, ...) must survive a round trip -
// dropping them silently renames entries whose display name lives in the
// Unicode Path extra and discards high-fidelity timestamps.
function stripZip64Extra(extra) {
  if (!extra.length) return EMPTY_BUFFER;
  const parts = [];
  let total = 0;
  forEachExtraField(extra, (id, body) => {
    if (id === ZIP64_EXTRA_ID) return;
    const record = Buffer.allocUnsafe(4 + body.length);
    record.writeUInt16LE(id, 0);
    record.writeUInt16LE(body.length, 2);
    body.copy(record, 4);
    ArrayPrototypePush(parts, record);
    total += record.length;
  });
  if (parts.length === 0) return EMPTY_BUFFER;
  return Buffer.concat(parts, total);
}

// Info-ZIP extended timestamp extra field ("UT", 0x5455; see
// parseExtTimestampMtime() above for the format's provenance): a flags byte
// (bit 0 = modification time present) followed by the whole (UTC) second.
function buildExtTimestampExtra(seconds) {
  const buffer = Buffer.allocUnsafe(9);
  buffer.writeUInt16LE(EXTRA_ID_EXT_TIMESTAMP, 0);
  buffer.writeUInt16LE(5, 2);
  buffer.writeUInt8(0x01, 4);
  buffer.writeInt32LE(seconds, 5);
  return buffer;
}

module.exports = {
  parseZip64Extra,
  forEachExtraField,
  extraFieldMtime,
  stripZip64Extra,
  buildExtTimestampExtra,
};
