'use strict';

// Reader-side header structures (sec. 4.3): the end-of-central-directory
// record, the Zip64 EOCD record/locator, and the central/local file headers,
// plus `findArchiveEnd()` which locates them and `readCentralDirectory()`
// which walks the central directory into an array of headers.

const {
  ArrayPrototypePush,
  MathMax,
  Number,
} = primordials;

const {
  codes: {
    ERR_ZIP_INVALID_ARCHIVE,
    ERR_ZIP_UNSUPPORTED_FEATURE,
  },
} = require('internal/errors');
const {
  BIGINT_MAX_SAFE_INTEGER,
  SIG_LOCAL_FILE_HEADER,
  SIG_CENTRAL_FILE_HEADER,
  SIG_ZIP64_EOCD_RECORD,
  SIG_ZIP64_EOCD_LOCATOR,
  SIG_EOCD,
  MADE_BY_UNIX,
  SENTINEL16,
  SENTINEL32,
  ZIP64_EOCD_MAX_LENGTH,
  S_IFLNK,
  S_IFMT,
} = require('internal/zip/constants');
const {
  validateArchiveRange,
  readSafeUint64,
} = require('internal/zip/binary');
const { parseZip64Extra } = require('internal/zip/extra-fields');
const {
  decodeDosDateTime,
  decodeZipName,
  decodeZipText,
} = require('internal/zip/dos');

// End of central directory record (sec. 4.3.16).
// Offset   Bytes   Description
// 0        4       Signature = 0x06054b50
// 4        2       Number of this disk
// 6        2       Disk where central directory starts
// 8        2       Number of central directory records on this disk
// 10       2       Total number of central directory records
// 12       4       Size of central directory (bytes)
// 16       4       Offset of start of central directory
// 20       2       Comment length (n)
// 22       n       Comment
class CentralEndHeader {
  #buffer;
  #offset;
  constructor(buffer, offset = 0) {
    validateArchiveRange(buffer, offset, 22, 'end of central directory record');
    if (buffer.readUInt32LE(offset) !== SIG_EOCD) {
      throw new ERR_ZIP_INVALID_ARCHIVE('end of central directory signature is invalid');
    }
    this.#buffer = buffer;
    this.#offset = offset;
    if (offset + this.byteLength > buffer.length) {
      throw new ERR_ZIP_INVALID_ARCHIVE('end of central directory record is truncated');
    }
  }
  get byteLength() { return 22 + this.commentLength; }
  get diskNumber() { return this.#buffer.readUInt16LE(this.#offset + 4); }
  get centralDirectoryDiskNumber() { return this.#buffer.readUInt16LE(this.#offset + 6); }
  get centralDirectoryDiskRecords() { return this.#buffer.readUInt16LE(this.#offset + 8); }
  get centralDirectoryTotalRecords() { return this.#buffer.readUInt16LE(this.#offset + 10); }
  get centralDirectorySize() { return this.#buffer.readUInt32LE(this.#offset + 12); }
  get centralDirectoryOffset() { return this.#buffer.readUInt32LE(this.#offset + 16); }
  get commentLength() { return this.#buffer.readUInt16LE(this.#offset + 20); }
  get commentBuffer() {
    const start = this.#offset + 22;
    return this.#buffer.subarray(start, start + this.commentLength);
  }
}

// Zip64 end of central directory record (sec. 4.3.14).
// 0    4   Signature = 0x06064b50
// 4    8   Size of remainder of this record
// 12   2   Version made by
// 14   2   Version needed to extract
// 16   4   Number of this disk
// 20   4   Disk where central directory starts
// 24   8   Number of central directory records on this disk
// 32   8   Total number of central directory records
// 40   8   Size of central directory
// 48   8   Offset of start of central directory
class Zip64EndRecord {
  #buffer;
  #offset;
  constructor(buffer, offset = 0) {
    validateArchiveRange(buffer, offset, 56, 'Zip64 end of central directory record');
    if (buffer.readUInt32LE(offset) !== SIG_ZIP64_EOCD_RECORD) {
      throw new ERR_ZIP_INVALID_ARCHIVE(
        'Zip64 end of central directory signature is invalid');
    }
    this.#buffer = buffer;
    this.#offset = offset;
  }
  get diskNumber() { return this.#buffer.readUInt32LE(this.#offset + 16); }
  get centralDirectoryDiskNumber() { return this.#buffer.readUInt32LE(this.#offset + 20); }
  get centralDirectoryDiskRecords() { return readSafeUint64(this.#buffer, this.#offset + 24); }
  get centralDirectoryTotalRecords() { return readSafeUint64(this.#buffer, this.#offset + 32); }
  get centralDirectorySize() { return readSafeUint64(this.#buffer, this.#offset + 40); }
  get centralDirectoryOffset() { return readSafeUint64(this.#buffer, this.#offset + 48); }
}

// Zip64 end of central directory locator (sec. 4.3.15).
// 0    4   Signature = 0x07064b50
// 4    4   Disk with the Zip64 end of central directory record
// 8    8   Offset of the Zip64 end of central directory record
// 16   4   Total number of disks
class Zip64EndLocator {
  #buffer;
  #offset;
  constructor(buffer, offset = 0) {
    validateArchiveRange(buffer, offset, 20, 'Zip64 end of central directory locator');
    if (buffer.readUInt32LE(offset) !== SIG_ZIP64_EOCD_LOCATOR) {
      throw new ERR_ZIP_INVALID_ARCHIVE(
        'Zip64 end of central directory locator signature is invalid');
    }
    this.#buffer = buffer;
    this.#offset = offset;
  }
  get recordDiskNumber() { return this.#buffer.readUInt32LE(this.#offset + 4); }
  // Spec field (sec. 4.3.15); not consumed as a getter - findArchiveEnd()
  // reads the offset leniently via readBigUInt64LE instead, so a locator
  // signature that is really comment data cannot turn into a hard parse
  // error on an out-of-range value.
  // get recordOffset() { return readSafeUint64(this.#buffer, this.#offset + 8); }
  get totalDisks() { return this.#buffer.readUInt32LE(this.#offset + 16); }
}

// Central directory file header (sec. 4.3.12).
// 0    4   Signature = 0x02014b50
// 4    2   Version made by
// 6    2   Version needed to extract
// 8    2   General purpose bit flag
// 10   2   Compression method
// 12   2   Last modification time
// 14   2   Last modification date
// 16   4   CRC-32
// 20   4   Compressed size
// 24   4   Uncompressed size
// 28   2   File name length (n)
// 30   2   Extra field length (m)
// 32   2   File comment length (k)
// 34   2   Disk number where file starts
// 36   2   Internal file attributes
// 38   4   External file attributes
// 42   4   Relative offset of local file header
// 46   n   File name
// 46+n m   Extra field
// 46+n+m k File comment
class CentralFileHeader {
  #buffer;
  #offset;
  #zip64 = null;
  constructor(buffer, offset = 0) {
    validateArchiveRange(buffer, offset, 46, 'central directory header');
    if (buffer.readUInt32LE(offset) !== SIG_CENTRAL_FILE_HEADER) {
      throw new ERR_ZIP_INVALID_ARCHIVE('central directory header signature is invalid');
    }
    this.#buffer = buffer;
    this.#offset = offset;
    if (offset + this.byteLength > buffer.length) {
      throw new ERR_ZIP_INVALID_ARCHIVE('central directory header is truncated');
    }
  }
  get byteOffset() { return this.#offset; }
  get byteLength() {
    return 46 + this.fileNameLength + this.extraFieldLength + this.fileCommentLength;
  }
  get version() { return this.#buffer.readUInt16LE(this.#offset + 4); }
  // Spec field "version needed to extract" (sec. 4.4.3); not consumed today.
  // get versionNeeded() { return this.#buffer.readUInt16LE(this.#offset + 6); }
  get flags() { return this.#buffer.readUInt16LE(this.#offset + 8); }
  get compressionMethod() { return this.#buffer.readUInt16LE(this.#offset + 10); }
  get lastModified() {
    return decodeDosDateTime(
      this.#buffer.readUInt16LE(this.#offset + 12),
      this.#buffer.readUInt16LE(this.#offset + 14));
  }
  get crc32() { return this.#buffer.readUInt32LE(this.#offset + 16); }
  // Lazily parse the Zip64 extended-information extra field (sec. 4.5.3),
  // supplying the true 64-bit values only for the classic fields that hold an
  // overflow sentinel. Cached across getters.
  #resolveZip64() {
    if (this.#zip64 === null) {
      this.#zip64 = parseZip64Extra(this.extraField, {
        uncompressedSize: this.#buffer.readUInt32LE(this.#offset + 24) === SENTINEL32,
        compressedSize: this.#buffer.readUInt32LE(this.#offset + 20) === SENTINEL32,
        localFileHeaderOffset: this.#buffer.readUInt32LE(this.#offset + 42) === SENTINEL32,
        diskNumber: this.#buffer.readUInt16LE(this.#offset + 34) === SENTINEL16,
      });
    }
    return this.#zip64;
  }
  get compressedSize() {
    const value = this.#buffer.readUInt32LE(this.#offset + 20);
    return value === SENTINEL32 ? this.#resolveZip64().compressedSize : value;
  }
  get uncompressedSize() {
    const value = this.#buffer.readUInt32LE(this.#offset + 24);
    return value === SENTINEL32 ? this.#resolveZip64().uncompressedSize : value;
  }
  get fileNameLength() { return this.#buffer.readUInt16LE(this.#offset + 28); }
  get extraFieldLength() { return this.#buffer.readUInt16LE(this.#offset + 30); }
  get fileCommentLength() { return this.#buffer.readUInt16LE(this.#offset + 32); }
  get diskNumber() {
    const value = this.#buffer.readUInt16LE(this.#offset + 34);
    return value === SENTINEL16 ? this.#resolveZip64().diskNumber : value;
  }
  get internalFileAttributes() { return this.#buffer.readUInt16LE(this.#offset + 36); }
  get externalFileAttributes() { return this.#buffer.readUInt32LE(this.#offset + 38); }
  get localFileHeaderOffset() {
    const value = this.#buffer.readUInt32LE(this.#offset + 42);
    return value === SENTINEL32 ? this.#resolveZip64().localFileHeaderOffset : value;
  }
  get fileNameBuffer() {
    const start = this.#offset + 46;
    return this.#buffer.subarray(start, start + this.fileNameLength);
  }
  get fileName() { return decodeZipName(this.fileNameBuffer, this.flags, this.extraField); }
  get extraField() {
    const start = this.#offset + 46 + this.fileNameLength;
    return this.#buffer.subarray(start, start + this.extraFieldLength);
  }
  get fileCommentBuffer() {
    const start = this.#offset + 46 + this.fileNameLength + this.extraFieldLength;
    return this.#buffer.subarray(start, start + this.fileCommentLength);
  }
  get fileComment() { return decodeZipText(this.fileCommentBuffer, this.flags); }
  get isUnixMode() { return (this.version >>> 8) === MADE_BY_UNIX; }
  get mode() {
    // Low 12 bits: permissions plus setuid/setgid/sticky (sec. 4.4.15).
    return this.isUnixMode ? (this.externalFileAttributes >>> 16) & 0o7777 : 0;
  }
  get isSymlink() {
    return this.isUnixMode &&
      ((this.externalFileAttributes >>> 16) & S_IFMT) === S_IFLNK;
  }
}

// Local file header (sec. 4.3.7).
// 0    4   Signature = 0x04034b50
// 4    2   Version needed to extract
// 6    2   General purpose bit flag
// 8    2   Compression method
// 10   2   Last modification time
// 12   2   Last modification date
// 14   4   CRC-32
// 18   4   Compressed size
// 22   4   Uncompressed size
// 26   2   File name length (n)
// 28   2   Extra field length (m)
// 30   n   File name
// 30+n m   Extra field
class LocalFileHeader {
  #buffer;
  #offset;
  constructor(buffer, offset = 0) {
    validateArchiveRange(buffer, offset, 30, 'local file header');
    if (buffer.readUInt32LE(offset) !== SIG_LOCAL_FILE_HEADER) {
      throw new ERR_ZIP_INVALID_ARCHIVE('local file header signature is invalid');
    }
    this.#buffer = buffer;
    this.#offset = offset;
    if (offset + this.byteLength > buffer.length) {
      throw new ERR_ZIP_INVALID_ARCHIVE('local file header is truncated');
    }
  }
  get byteLength() { return 30 + this.fileNameLength + this.extraFieldLength; }
  get flags() { return this.#buffer.readUInt16LE(this.#offset + 6); }
  // Spec field (sec. 4.4.5); the central directory's method is authoritative,
  // so the local copy is not consumed today.
  // get compressionMethod() { return this.#buffer.readUInt16LE(this.#offset + 8); }
  get fileNameLength() { return this.#buffer.readUInt16LE(this.#offset + 26); }
  get extraFieldLength() { return this.#buffer.readUInt16LE(this.#offset + 28); }
  get extraField() {
    const start = this.#offset + 30 + this.fileNameLength;
    return this.#buffer.subarray(start, start + this.extraFieldLength);
  }
  // The local header also carries the name and modification time (sec. 4.3.7),
  // but the central directory is authoritative for both - a mismatching local
  // name is ignored by design - so only the flags and the extra field (which
  // may hold a higher-resolution timestamp) are consumed here.
  // get fileName() {
  //   const start = this.#offset + 30;
  //   return decodeZipName(
  //     this.#buffer.subarray(start, start + this.fileNameLength), this.flags, this.extraField);
  // }
  // get lastModified() {
  //   return decodeDosDateTime(this.#buffer.readUInt16LE(this.#offset + 10),
  //                            this.#buffer.readUInt16LE(this.#offset + 12));
  // }
  // Total on-disk length of the local header at `offset` (sec. 4.3.7), read
  // straight from the length fields without constructing a header; 0 if the
  // fixed part does not fit. Lets the reader skip to the entry data.
  static length(buffer, offset) {
    if (offset + 30 > buffer.length) return 0;
    return 30 + buffer.readUInt16LE(offset + 26) + buffer.readUInt16LE(offset + 28);
  }
}

/**
 * Locates and validates the end-of-archive structures (EOCD, and the Zip64
 * EOCD locator/record when present) in `buffer`. `base` is the absolute
 * offset of `buffer[0]` when `buffer` is only the tail of a larger file; all
 * returned offsets are absolute. `buffer` must extend to the end of the
 * archive.
 *
 * When a required Zip64 EOCD record starts before `buffer[0]` (its
 * extensible data sector can push it beyond any fixed-size tail read),
 * returns `{ needTailFrom }` instead: the caller must retry with a buffer
 * that starts at that absolute offset (still ending at the end of the
 * archive). Never returned when `base` is 0.
 * @returns {{
 *   prefix: number,
 *   totalRecords: number,
 *   centralDirectoryOffset: number,
 *   centralDirectorySize: number,
 *   comment: Buffer,
 * } | { needTailFrom: number }}
 */
function findArchiveEnd(buffer, base = 0) {
  if (buffer.length < 22) {
    throw new ERR_ZIP_INVALID_ARCHIVE('no end of central directory record found');
  }
  const min = MathMax(0, buffer.length - (22 + SENTINEL16));
  let eocdPos = -1;
  // Pass 1: the comment must reach exactly to the end of the buffer (this
  // rejects a stray EOCD-looking signature inside an earlier comment).
  for (let pos = buffer.length - 22; pos >= min; pos--) {
    if (buffer.readUInt32LE(pos) !== SIG_EOCD) continue;
    if (pos + 22 + buffer.readUInt16LE(pos + 20) !== buffer.length) continue;
    eocdPos = pos;
    break;
  }
  if (eocdPos < 0) {
    // Pass 2: tolerate trailing padding after the EOCD (some streaming
    // writers pad their output to a fixed block size); take the last
    // candidate found.
    for (let pos = buffer.length - 22; pos >= min; pos--) {
      if (buffer.readUInt32LE(pos) !== SIG_EOCD) continue;
      if (pos + 22 + buffer.readUInt16LE(pos + 20) > buffer.length) continue;
      eocdPos = pos;
      break;
    }
  }
  if (eocdPos < 0) {
    throw new ERR_ZIP_INVALID_ARCHIVE('no end of central directory record found');
  }
  const eocd = new CentralEndHeader(buffer, eocdPos);
  let totalRecords = eocd.centralDirectoryTotalRecords;
  let centralDirectorySize = eocd.centralDirectorySize;
  let centralDirectoryOffset = eocd.centralDirectoryOffset;
  let prefix;
  // A classic field at its maximum is an overflow sentinel that makes the
  // Zip64 record mandatory. Otherwise the classic fields are authoritative,
  // and a Zip64 locator signature in the preceding bytes is not proof of a
  // Zip64 archive - it may be the tail of a file comment that happens to
  // contain those four bytes - so a failed Zip64 lookup falls back to the
  // classic fields instead of rejecting the archive.
  const needsZip64 =
    eocd.diskNumber === SENTINEL16 ||
    eocd.centralDirectoryDiskNumber === SENTINEL16 ||
    eocd.centralDirectoryDiskRecords === SENTINEL16 ||
    totalRecords === SENTINEL16 ||
    centralDirectorySize === SENTINEL32 ||
    centralDirectoryOffset === SENTINEL32;
  let zip64 = null;
  let recordPos = -1;
  const locatorPos = eocdPos - 20;
  if (
    locatorPos >= 0 &&
    buffer.readUInt32LE(locatorPos) === SIG_ZIP64_EOCD_LOCATOR
  ) {
    const locator = new Zip64EndLocator(buffer, locatorPos);
    // Read the recorded offset leniently: on a coincidental signature these
    // eight bytes are arbitrary comment data, which must not turn into a
    // hard parse error.
    const rawRecordOffset = buffer.readBigUInt64LE(locatorPos + 8);
    const recordOffset =
      rawRecordOffset <= BIGINT_MAX_SAFE_INTEGER ? Number(rawRecordOffset) : -1;
    recordPos = recordOffset >= 0 ? recordOffset - base : -1;
    if (
      !(recordPos >= 0 &&
        recordPos + 56 <= locatorPos &&
        buffer.readUInt32LE(recordPos) === SIG_ZIP64_EOCD_RECORD)
    ) {
      // Data was prepended to the archive, shifting the record; scan
      // backward from the locator instead of trusting its recorded offset.
      recordPos = -1;
      const floor = MathMax(0, locatorPos - 56 - SENTINEL16);
      for (let pos = locatorPos - 56; pos >= floor; pos--) {
        if (buffer.readUInt32LE(pos) !== SIG_ZIP64_EOCD_RECORD) continue;
        const size = buffer.readBigUInt64LE(pos + 4);
        if (size >= 44n && pos + 12 + Number(size) === locatorPos) {
          recordPos = pos;
          break;
        }
      }
    }
    if (recordPos < 0 && needsZip64) {
      // The record is required but does not lie inside `buffer`: its
      // extensible data sector may extend it beyond any fixed-size tail
      // read. When the locator points (plausibly) before the bytes at
      // hand, ask the caller for a longer tail instead of failing.
      if (
        recordOffset >= 0 &&
        recordOffset < base &&
        base + locatorPos - recordOffset <= ZIP64_EOCD_MAX_LENGTH
      ) {
        return { needTailFrom: recordOffset };
      }
      throw new ERR_ZIP_INVALID_ARCHIVE('Zip64 end of central directory record not found');
    }
    if (recordPos >= 0) {
      if (locator.totalDisks > 1 || locator.recordDiskNumber !== 0) {
        throw new ERR_ZIP_UNSUPPORTED_FEATURE('multi-disk archives are not supported');
      }
      zip64 = new Zip64EndRecord(buffer, recordPos);
    }
  }
  // The `prefix` math assumes nothing sits between the central directory and
  // the archive-end records. APPNOTE sec. 4.3.13 allows a digital-signature
  // record there; such an archive shifts `prefix` by the signature's length
  // and fails with a central-directory signature error rather than a targeted
  // message - signed archives are essentially extinct, so none is emitted.
  if (zip64 !== null) {
    if (zip64.diskNumber !== 0 || zip64.centralDirectoryDiskNumber !== 0) {
      throw new ERR_ZIP_UNSUPPORTED_FEATURE('multi-disk archives are not supported');
    }
    if (zip64.centralDirectoryDiskRecords !== zip64.centralDirectoryTotalRecords) {
      throw new ERR_ZIP_UNSUPPORTED_FEATURE('multi-disk archives are not supported');
    }
    totalRecords = zip64.centralDirectoryTotalRecords;
    centralDirectorySize = zip64.centralDirectorySize;
    centralDirectoryOffset = zip64.centralDirectoryOffset;
    prefix = base + recordPos - (centralDirectoryOffset + centralDirectorySize);
  } else {
    if (eocd.diskNumber !== 0 || eocd.centralDirectoryDiskNumber !== 0) {
      throw new ERR_ZIP_UNSUPPORTED_FEATURE('multi-disk archives are not supported');
    }
    if (eocd.centralDirectoryDiskRecords !== totalRecords) {
      throw new ERR_ZIP_UNSUPPORTED_FEATURE('multi-disk archives are not supported');
    }
    prefix = base + eocdPos - (centralDirectoryOffset + centralDirectorySize);
  }
  if (prefix < 0) {
    throw new ERR_ZIP_INVALID_ARCHIVE('central directory does not fit inside the archive');
  }
  if (totalRecords * 46 > centralDirectorySize) {
    throw new ERR_ZIP_INVALID_ARCHIVE(
      'central directory record count is inconsistent with its size');
  }
  return {
    prefix,
    totalRecords,
    centralDirectoryOffset: centralDirectoryOffset + prefix,
    centralDirectorySize,
    comment: eocd.commentBuffer,
  };
}

// Walk the contiguous run of `count` central directory file headers
// (sec. 4.3.12) into an array, rejecting multi-disk archives.
function readCentralDirectory(buffer, count) {
  const result = [];
  let pos = 0;
  for (let index = 0; index < count; index++) {
    const header = new CentralFileHeader(buffer, pos);
    if (header.diskNumber !== 0) {
      throw new ERR_ZIP_UNSUPPORTED_FEATURE('multi-disk archives are not supported');
    }
    ArrayPrototypePush(result, header);
    pos += header.byteLength;
  }
  return result;
}

module.exports = {
  CentralFileHeader,
  LocalFileHeader,
  findArchiveEnd,
  readCentralDirectory,
};
