'use strict';

// Shared constants, symbols, and tiny shared values for the ZIP
// implementation. This module is a leaf: it requires nothing from the rest of
// `internal/zip`, so every other zip module can require it without cycles.

const {
  BigInt,
  NumberMAX_SAFE_INTEGER,
  Symbol,
} = primordials;

const { FastBuffer } = require('internal/buffer');

const EMPTY_BUFFER = new FastBuffer();
const BIGINT_MAX_SAFE_INTEGER = BigInt(NumberMAX_SAFE_INTEGER);

// ZIP record signatures (APPNOTE.TXT, PKWARE Inc.)
const SIG_LOCAL_FILE_HEADER = 0x04034b50; // sec. 4.3.7
const SIG_DATA_DESCRIPTOR = 0x08074b50; // sec. 4.3.9
const SIG_CENTRAL_FILE_HEADER = 0x02014b50; // sec. 4.3.12
const SIG_ZIP64_EOCD_RECORD = 0x06064b50; // sec. 4.3.14
const SIG_ZIP64_EOCD_LOCATOR = 0x07064b50; // sec. 4.3.15
const SIG_EOCD = 0x06054b50; // sec. 4.3.16

const MADE_BY_UNIX = 3; // sec. 4.4.2
const ZIP64_EXTRA_ID = 0x0001; // sec. 4.5.3

const SENTINEL16 = 0xffff;
const SENTINEL32 = 0xffffffff;

const FLAG_ENCRYPTED = 0x0001; // sec. 4.4.4 bit 0
const FLAG_DATA_DESCRIPTOR = 0x0008; // bit 3
const FLAG_UTF8 = 0x0800; // bit 11: name/comment are UTF-8 (EFS)

const METHOD_STORE = 0; // sec. 4.4.5
const METHOD_DEFLATE = 8; // sec. 4.4.5
const METHOD_ZSTD = 93; // sec. 4.4.5

const VERSION_DEFAULT = 20; // 2.0: deflate + directories (sec. 4.4.3)
const VERSION_ZIP64 = 45; // 4.5: Zip64 structures
const VERSION_ZSTD = 63; // 6.3: Zstandard compression (method 93)

// The Zip64 EOCD record may carry an extensible data sector of arbitrary
// length between its fixed part and the locator, so it can start before any
// fixed-size tail read. When the locator points further back than the bytes
// at hand, callers re-read from the recorded offset - but only within this
// bound, so a hostile locator cannot demand an unbounded allocation.
const ZIP64_EOCD_MAX_LENGTH = 56 + 1024 * 1024;

const S_IFREG = 0o100000; // Unix mode type bits: regular file
const S_IFDIR = 0o040000; // Unix mode type bits: directory
const S_IFLNK = 0o120000; // Unix mode type bits: symbolic link
const S_IFMT = 0o170000; // Unix mode type mask

// Extra-field header IDs we consult on read (sec. 4.5).
const EXTRA_ID_NTFS = 0x000a; // NTFS times (100 ns since 1601)
const EXTRA_ID_EXT_TIMESTAMP = 0x5455; // Info-ZIP extended timestamp ("UT")
const EXTRA_ID_UNIX_OLD = 0x5855; // Info-ZIP Unix, original ("UX")
const EXTRA_ID_UNICODE_PATH = 0x7075; // Info-ZIP Unicode Path ("up")

// Passed between `ZipEntry` and the archive writers/`ZipFile`: `kFinalize`
// asks an entry for its central-directory header at a known local offset;
// `kPromote` rebinds a just-serialized streaming entry to its on-disk copy.
const kFinalize = Symbol('kFinalize');
const kPromote = Symbol('kPromote');

// The chunk size for reading a file-backed member's compressed bytes.
const READ_CHUNK_SIZE = 4 * 1024 * 1024;
// EOCD + max comment + Zip64 locator + Zip64 record + slack for an
// extensible data sector: the fixed-size tail `ZipFile.open()` reads first.
const TAIL_LENGTH = 22 + SENTINEL16 + 20 + 56 + 4096;

module.exports = {
  EMPTY_BUFFER,
  BIGINT_MAX_SAFE_INTEGER,
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
  FLAG_ENCRYPTED,
  FLAG_DATA_DESCRIPTOR,
  FLAG_UTF8,
  METHOD_STORE,
  METHOD_DEFLATE,
  METHOD_ZSTD,
  VERSION_DEFAULT,
  VERSION_ZIP64,
  VERSION_ZSTD,
  ZIP64_EOCD_MAX_LENGTH,
  S_IFREG,
  S_IFDIR,
  S_IFLNK,
  S_IFMT,
  EXTRA_ID_NTFS,
  EXTRA_ID_EXT_TIMESTAMP,
  EXTRA_ID_UNIX_OLD,
  EXTRA_ID_UNICODE_PATH,
  kFinalize,
  kPromote,
  READ_CHUNK_SIZE,
  TAIL_LENGTH,
};
