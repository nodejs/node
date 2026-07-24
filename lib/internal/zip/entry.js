'use strict';

// `ZipEntry`: a single archive member. Reads (buffered and streaming),
// (de)serialization, and the `create()`/`createSync()`/`createStream()`/
// `createSymlink()` builders, plus the `createEntryMeta()` helper that
// normalizes their options into the internal metadata record.

const {
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  ArrayPrototypeSort,
  Date,
  DateNow,
  JSONStringify,
  MathFloor,
  MathMin,
  NumberMAX_SAFE_INTEGER,
  StringPrototypeEndsWith,
  SymbolAsyncDispose,
  SymbolAsyncIterator,
  SymbolDispose,
  SymbolIterator,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_STATE,
    ERR_ZIP_ENTRY_TOO_LARGE,
    ERR_ZIP_INVALID_ARCHIVE,
    ERR_ZIP_UNSUPPORTED_FEATURE,
  },
} = require('internal/errors');
const {
  validateInteger,
  validateString,
  validateUint32,
} = require('internal/validators');
const {
  isDate,
  isUint8Array,
} = require('internal/util/types');
const { Buffer, kMaxLength } = require('buffer');
const { crc32: crc32Native } = internalBinding('zlib');
const {
  EMPTY_BUFFER,
  SIG_LOCAL_FILE_HEADER,
  SENTINEL16,
  FLAG_DATA_DESCRIPTOR,
  FLAG_UTF8,
  MADE_BY_UNIX,
  METHOD_STORE,
  METHOD_DEFLATE,
  METHOD_ZSTD,
  S_IFREG,
  S_IFDIR,
  S_IFLNK,
  S_IFMT,
  READ_CHUNK_SIZE,
  kFinalize,
  kPromote,
} = require('internal/zip/constants');
const {
  toBuffer,
  validateArchiveRange,
} = require('internal/zip/binary');
const {
  extraFieldMtime,
  stripZip64Extra,
} = require('internal/zip/extra-fields');
const {
  decodeZipName,
  decodeZipText,
} = require('internal/zip/dos');
const {
  CentralFileHeader,
  LocalFileHeader,
  findArchiveEnd,
} = require('internal/zip/headers');
const {
  buildLocalHeader,
  buildCentralHeader,
  buildDataDescriptor64,
} = require('internal/zip/header-builders');
const {
  deflateRawAsync,
  deflateRawSync,
  zstdCompressAsync,
  zstdCompressSync,
  deflateRawStream,
  zstdCompressStream,
  decodeMemberStream,
  decodeMemberAsync,
  decodeMemberSync,
} = require('internal/zip/compression');
const {
  readFdFully,
  readFdFullySync,
} = require('internal/zip/fs-util');
const { getMaxZipContentSize } = require('internal/zip/content-size');

// The whole (UTC) second to record in an extended-timestamp extra field when
// `mtimeMs` cannot be represented exactly by the 2-second-resolution,
// local-time DOS date/time fields (sub-second parts and odd seconds alike),
// or null when the DOS fields suffice or the value does not fit the extra
// field's signed 32-bit Unix-seconds range (through 2038). One rule, shared
// by createEntryMeta() and #finalizeMeta() so the two cannot drift.
function extendedMtimeSeconds(mtimeMs) {
  const seconds = MathFloor(mtimeMs / 1000);
  return (mtimeMs % 2000 !== 0 && seconds >= -2147483648 && seconds <= 2147483647) ?
    seconds : null;
}

// Normalize the public builder options into the internal metadata record:
// name/comment bytes, the UTF-8 flag, the Unix mode packed into the external
// attributes (sec. 4.4.15), and the DOS/extended-timestamp fields.
function createEntryMeta(filename, options) {
  validateString(filename, 'filename');
  const name = Buffer.from(filename, 'utf8');
  if (name.length === 0) {
    throw new ERR_INVALID_ARG_VALUE('filename', filename, 'must not be empty');
  }
  if (name.length > SENTINEL16) {
    throw new ERR_ZIP_ENTRY_TOO_LARGE(
      'the entry name must not exceed 65535 bytes when encoded as UTF-8');
  }
  let comment = EMPTY_BUFFER;
  if (options?.comment !== undefined) {
    validateString(options.comment, 'options.comment');
    comment = Buffer.from(options.comment, 'utf8');
    if (comment.length > SENTINEL16) {
      throw new ERR_ZIP_ENTRY_TOO_LARGE(
        'the entry comment must not exceed 65535 bytes when encoded as UTF-8');
    }
  }
  const isSymlink = options?.symlink === true;
  const isDirectory = !isSymlink && StringPrototypeEndsWith(filename, '/');
  const mode = options?.mode ?? (isSymlink ? 0o777 : isDirectory ? 0o755 : 0o644);
  validateUint32(mode, 'options.mode');
  // Default to the current time at the DOS fields' 2-second resolution, so a
  // default entry needs no extended-timestamp extra field (see below).
  const modified = options?.modified ?? new Date(MathFloor(DateNow() / 2000) * 2000);
  if (!isDate(modified)) {
    throw new ERR_INVALID_ARG_TYPE('options.modified', 'Date', modified);
  }
  if (options?.method !== undefined &&
      options.method !== 'deflate' && options.method !== 'store' && options.method !== 'zstd') {
    throw new ERR_INVALID_ARG_VALUE(
      'options.method', options.method, "must be 'deflate', 'store', or 'zstd'");
  }
  const typeBits = isSymlink ? S_IFLNK : isDirectory ? S_IFDIR : S_IFREG;
  const unixAttrs = (typeBits | (mode & 0o7777)) & SENTINEL16;
  const external = ((unixAttrs << 16) | (isDirectory ? 0x10 : 0)) >>> 0;
  // Record the whole (UTC) second in an extended-timestamp extra field when
  // the DOS fields cannot represent the time exactly; see
  // extendedMtimeSeconds().
  const extendedMtime = extendedMtimeSeconds(modified.getTime());
  return {
    name,
    comment,
    extra: EMPTY_BUFFER,
    flags: FLAG_UTF8,
    method: 0,
    crc: 0,
    compressedSize: 0,
    uncompressedSize: 0,
    modified,
    extendedMtime,
    external,
    internal: 0,
    madeBy: MADE_BY_UNIX,
    pending: true,
  };
}

/**
 * A single file or directory inside a ZIP archive: reading, writing, and
 * (de)serializing one archive member.
 */
class ZipEntry {
  #central;
  #local;
  #content;
  #source = null;
  #meta = null;
  #serialized = false;
  // When #fd is non-null the entry is "file-backed": it holds no content
  // buffer, only a descriptor and the local-header offset, and reads its
  // compressed bytes from disk on demand (see #compressedBytes/#rawChunks).
  // #contentOffset caches the resolved start of the compressed data (a
  // number, not a buffer) once the local header has been read.
  #fd = null;
  #localOffset = -1;
  #contentOffset = -1;

  /**
   * @private
   */
  constructor(central, local, content, fd = null, localOffset = -1) {
    this.#central = central;
    this.#local = local;
    this.#content = content;
    this.#fd = fd;
    this.#localOffset = localOffset;
  }

  // Whether the content is stored in compressed form, whatever the method.
  get compressed() { return this.method !== METHOD_STORE; }
  get rawContent() { return this.#content; }
  get method() {
    return this.#meta ? this.#meta.method : this.#central.compressionMethod;
  }
  get flags() {
    return this.#meta ? this.#meta.flags : (this.#local ?? this.#central).flags;
  }
  get crc32() {
    if (this.#meta) {
      this.#assertNotPending();
      return this.#meta.crc;
    }
    return this.#central.crc32;
  }
  get name() {
    // The central directory is authoritative; a mismatched local-header name
    // is deliberately ignored (defends against parser-confusion attacks).
    // Both branches use the same decoding (Unicode Path extra, then
    // UTF-8/CP437), so serialization - which snapshots the raw bytes into
    // #meta - never changes what this getter reports.
    return this.#meta ?
      decodeZipName(this.#meta.name, this.#meta.flags, this.#meta.extra) :
      this.#central.fileName;
  }
  get nameBuffer() {
    return this.#meta ? this.#meta.name : this.#central.fileNameBuffer;
  }
  get comment() {
    return this.#meta ?
      decodeZipText(this.#meta.comment, this.#meta.flags) :
      this.#central.fileComment;
  }
  get size() {
    if (this.#meta) {
      this.#assertNotPending();
      return this.#meta.uncompressedSize;
    }
    return this.#central.uncompressedSize;
  }
  get compressedSize() {
    if (this.#meta) {
      this.#assertNotPending();
      return this.#meta.compressedSize;
    }
    return this.#central.compressedSize;
  }
  get modified() {
    if (this.#meta) return this.#meta.modified;
    // Prefer an extra-field timestamp (absolute, higher-resolution) over the
    // coarse local-time DOS date/time fields when a foreign archive carries
    // one; consult both the central and local headers. A file-backed entry
    // starts out with only its central header, but some tools (7-Zip, for
    // one) write their high-fidelity timestamp only into the local header -
    // resolve it lazily (a small, one-time positioned read) rather than
    // silently reporting a coarser time than the archive carries, and fall
    // back to the central data when the local header cannot be read.
    if (this.#fd !== null && this.#local === null) {
      try {
        this.#resolveLocalHeaderSync();
      } catch {
        // A malformed local header fails loudly on the content read paths;
        // for metadata, the central directory alone has to do.
      }
    }
    return extraFieldMtime(this.#central.extraField, this.#local?.extraField) ??
      this.#central.lastModified;
  }
  get mode() {
    // The external attributes' high 16 bits hold Unix permissions only when
    // the entry was made by a Unix host (sec. 4.4.2/4.4.15) - the same rule
    // as `CentralFileHeader.prototype.mode`, which the non-meta branch
    // defers to.
    if (this.#meta) {
      return this.#meta.madeBy === MADE_BY_UNIX ?
        (this.#meta.external >>> 16) & 0o7777 : 0;
    }
    return this.#central.mode;
  }
  get isSymlink() {
    // Derived from the made-by host and the external attributes' Unix type
    // bits in both branches, so it survives serialization (which preserves
    // both verbatim); this also makes a fresh `createSymlink()` entry report
    // itself as one.
    if (this.#meta) {
      return this.#meta.madeBy === MADE_BY_UNIX &&
        ((this.#meta.external >>> 16) & S_IFMT) === S_IFLNK;
    }
    return this.#central.isSymlink;
  }
  get isFile() { return !this.isDirectory && !this.isSymlink; }
  get isDirectory() { return StringPrototypeEndsWith(this.name, '/'); }

  // Guard: reject metadata/content reads on a write-streaming entry whose
  // sizes and CRC are not yet known (still pending serialization).
  #assertNotPending() {
    if (this.#meta?.pending) {
      throw new ERR_INVALID_STATE(
        'this streaming entry has not finished serializing yet');
    }
  }

  // Snapshot this (parsed) entry's central-directory data into a
  // re-serializable #meta record, memoized. Needed so a round-tripped entry
  // can be re-emitted from stable, extra-field-aware values rather than raw
  // header bytes; clears the data-descriptor flag (see below).
  #finalizeMeta() {
    if (this.#meta) {
      this.#assertNotPending();
      return this.#meta;
    }
    const central = this.#central;
    // Descriptor entries (bit 3) are re-emitted with known sizes/CRC and bit
    // 3 cleared: full re-serialization emits a fresh local header from this
    // same record, so it never reproduces a bit-3 local header without a
    // data descriptor. (This invariant does NOT hold for `ZipFile`'s
    // in-place central-directory rewrite, which leaves local headers on disk
    // untouched - that path re-asserts bit 3 via `[kFinalize]`; see there.)
    // Sizes come from the central directory
    // (Zip64-aware); Zip64 extras are regenerated as needed, and all other
    // extra-field records (Unicode Path, NTFS/UT timestamps, ...) are
    // preserved so a re-serialized entry keeps its name encoding and
    // timestamps.
    const extra = stripZip64Extra(central.extraField);
    // Snapshot the resolved (extra-field-aware) time, not the raw DOS field,
    // so serialization does not degrade what `modified` reports. When that
    // time cannot be represented exactly in the DOS fields and no preserved
    // extra record carries it, record it in an extended-timestamp extra
    // (see extendedMtimeSeconds(); same rule as createEntryMeta()).
    const modified = this.modified;
    const extendedMtime = extraFieldMtime(extra) === null ?
      extendedMtimeSeconds(modified.getTime()) : null;
    const meta = {
      name: central.fileNameBuffer,
      comment: central.fileCommentBuffer,
      extra,
      flags: central.flags & ~FLAG_DATA_DESCRIPTOR,
      method: central.compressionMethod,
      crc: central.crc32,
      compressedSize: central.compressedSize,
      uncompressedSize: central.uncompressedSize,
      modified,
      extendedMtime,
      external: central.externalFileAttributes,
      internal: central.internalFileAttributes,
      // Preserve the creator's host byte (sec. 4.4.2): the external
      // attributes are only interpretable relative to it.
      madeBy: central.version >>> 8,
      pending: false,
    };
    this.#meta = meta;
    return meta;
  }

  // Read (and cache) this file-backed entry's local file header - whose
  // length (fixed 30 bytes plus variable name/extra fields) is only known
  // from the file itself, not from the central directory. Resolving it also
  // yields the offset where the compressed data begins, and gives the
  // `modified` getter access to a local-header-only timestamp extra.
  async #resolveLocalHeader() {
    if (this.#local !== null) return this.#local;
    const fixed = Buffer.allocUnsafe(30);
    await readFdFully(this.#fd, fixed, this.#localOffset);
    if (fixed.readUInt32LE(0) !== SIG_LOCAL_FILE_HEADER) {
      throw new ERR_ZIP_INVALID_ARCHIVE(
        `entry ${JSONStringify(this.name)} has an invalid local file header`);
    }
    const length = LocalFileHeader.length(fixed, 0);
    const full = Buffer.allocUnsafe(length);
    fixed.copy(full, 0);
    if (length > 30) {
      await readFdFully(this.#fd, full.subarray(30), this.#localOffset + 30);
    }
    this.#local = new LocalFileHeader(full, 0);
    this.#contentOffset = this.#localOffset + length;
    return this.#local;
  }
  // Sync counterpart of #resolveLocalHeader().
  #resolveLocalHeaderSync() {
    if (this.#local !== null) return this.#local;
    const fixed = Buffer.allocUnsafe(30);
    readFdFullySync(this.#fd, fixed, this.#localOffset);
    if (fixed.readUInt32LE(0) !== SIG_LOCAL_FILE_HEADER) {
      throw new ERR_ZIP_INVALID_ARCHIVE(
        `entry ${JSONStringify(this.name)} has an invalid local file header`);
    }
    const length = LocalFileHeader.length(fixed, 0);
    const full = Buffer.allocUnsafe(length);
    fixed.copy(full, 0);
    if (length > 30) {
      readFdFullySync(this.#fd, full.subarray(30), this.#localOffset + 30);
    }
    this.#local = new LocalFileHeader(full, 0);
    this.#contentOffset = this.#localOffset + length;
    return this.#local;
  }
  // The offset where this file-backed entry's compressed data begins, reading
  // the local header first if that has not been resolved yet.
  async #resolveContentOffset() {
    if (this.#contentOffset < 0) await this.#resolveLocalHeader();
    return this.#contentOffset;
  }
  // Sync counterpart of #resolveContentOffset().
  #resolveContentOffsetSync() {
    if (this.#contentOffset < 0) this.#resolveLocalHeaderSync();
    return this.#contentOffset;
  }
  // The in-memory raw bytes, or a clean state error when there are none - a
  // write-streaming entry (`createStream()`) has no readable content until it
  // has been serialized into a backing archive (after which `addEntry()`
  // promotes it to file-backed; see [kPromote]).
  #inMemoryCompressed() {
    if (this.#content === null) {
      throw new ERR_INVALID_STATE(
        'the content of a streaming entry is not available for reading');
    }
    return this.#content;
  }
  // The entry's raw (still-compressed) bytes. For an in-memory entry this is
  // the entry's retained buffer - shared memory, NOT a copy: it may alias
  // the source archive (`ZipEntry.read()`) or the caller's original `data`
  // (`create()` when storing). For a file-backed entry it is freshly read
  // from disk and caller-owned. Paths that hand these bytes out undecoded
  // (the store method) must copy the in-memory case; see `content()`.
  async #compressedBytes() {
    if (this.#fd === null) return this.#inMemoryCompressed();
    const size = this.compressedSize;
    // A member at or beyond the maximum Buffer length cannot be materialized
    // in one allocation; stream it instead. (kMaxLength equals the safe
    // integer ceiling on 64-bit, so a larger size fails to parse anyway.)
    if (size >= kMaxLength) {
      throw new ERR_ZIP_ENTRY_TOO_LARGE(
        `entry ${JSONStringify(this.name)} is too large to buffer ` +
        `(${size} compressed bytes); use contentIterator() instead`);
    }
    const start = await this.#resolveContentOffset();
    const compressed = Buffer.allocUnsafe(size);
    await readFdFully(this.#fd, compressed, start);
    return compressed;
  }
  // Sync counterpart of #compressedBytes().
  #compressedBytesSync() {
    if (this.#fd === null) return this.#inMemoryCompressed();
    const size = this.compressedSize;
    if (size >= kMaxLength) {
      throw new ERR_ZIP_ENTRY_TOO_LARGE(
        `entry ${JSONStringify(this.name)} is too large to buffer ` +
        `(${size} compressed bytes); use contentIterator() instead`);
    }
    const start = this.#resolveContentOffsetSync();
    const compressed = Buffer.allocUnsafe(size);
    readFdFullySync(this.#fd, compressed, start);
    return compressed;
  }
  // The entry's raw compressed bytes as a bounded-memory chunk stream, read
  // straight from disk (file-backed entries only). Nothing is retained.
  async *#rawChunks() {
    const fd = this.#fd;
    let pos = await this.#resolveContentOffset();
    let remaining = this.compressedSize;
    while (remaining > 0) {
      const take = MathMin(READ_CHUNK_SIZE, remaining);
      const chunk = Buffer.allocUnsafe(take);
      await readFdFully(fd, chunk, pos);
      pos += take;
      remaining -= take;
      yield chunk;
    }
  }
  // Sync counterpart of #rawChunks().
  *#rawChunksSync() {
    const fd = this.#fd;
    let pos = this.#resolveContentOffsetSync();
    let remaining = this.compressedSize;
    while (remaining > 0) {
      const take = MathMin(READ_CHUNK_SIZE, remaining);
      const chunk = Buffer.allocUnsafe(take);
      readFdFullySync(fd, chunk, pos);
      pos += take;
      remaining -= take;
      yield chunk;
    }
  }

  /**
   * Reads, decompresses, and (by default) CRC-32-verifies the whole entry
   * into a single `Buffer`, enforcing the declared-size and `maxSize` limits.
   * @param {{ verify?: boolean, maxSize?: number }} [options]
   * @returns {Promise<Buffer>}
   */
  async content(options) {
    const declared = this.size;
    const maxSize = options?.maxSize ?? getMaxZipContentSize();
    if (declared > maxSize) {
      throw new ERR_ZIP_ENTRY_TOO_LARGE(
        `entry ${JSONStringify(this.name)} declares ${declared} bytes, ` +
        `exceeding the ${maxSize} byte limit`);
    }
    const compressed = await this.#compressedBytes();
    const data = await decodeMemberAsync(compressed, {
      name: this.name,
      flags: this.flags,
      method: this.method,
      crc32: this.crc32,
      uncompressedSize: declared,
    }, { verify: options?.verify, maxSize });
    // `data === compressed` only on the store path; copy the in-memory case
    // (the entry's retained buffer, see #compressedBytes()) so the result is
    // caller-owned on every path.
    return data === compressed && this.#fd === null ? Buffer.from(data) : data;
  }

  /**
   * The synchronous counterpart of `content()`. Blocks the event loop and
   * further JavaScript execution until the whole entry has been read and, if
   * applicable, inflated - use only where synchronous I/O is appropriate
   * (for example, short-lived scripts or startup code), not in code that
   * must stay responsive.
   * @param {{ verify?: boolean, maxSize?: number }} [options]
   * @returns {Buffer}
   */
  contentSync(options) {
    const declared = this.size;
    const maxSize = options?.maxSize ?? getMaxZipContentSize();
    if (declared > maxSize) {
      throw new ERR_ZIP_ENTRY_TOO_LARGE(
        `entry ${JSONStringify(this.name)} declares ${declared} bytes, ` +
        `exceeding the ${maxSize} byte limit`);
    }
    const compressed = this.#compressedBytesSync();
    const data = decodeMemberSync(compressed, {
      name: this.name,
      flags: this.flags,
      method: this.method,
      crc32: this.crc32,
      uncompressedSize: declared,
    }, { verify: options?.verify, maxSize });
    // `data === compressed` only on the store path; copy the in-memory case
    // so the result is caller-owned on every path (see content()).
    return data === compressed && this.#fd === null ? Buffer.from(data) : data;
  }

  // The raw (still-compressed) bytes as an async iterable, without buffering
  // the whole member: straight from disk for a file-backed entry, or the
  // single in-memory buffer otherwise. Throws synchronously for a pending
  // write-streaming entry, whose content is not yet available for reading.
  #rawSource() {
    if (this.#fd !== null) return this.#rawChunks();
    const content = this.#inMemoryCompressed();
    return (async function* () {
      if (content.length) yield content;
    })();
  }

  /**
   * Yields the entry's decompressed content as a bounded-memory async
   * iterator of `Buffer` chunks, decompressing on the way and (by default)
   * verifying CRC-32. For a file-backed entry (from `ZipFile.get()`) the
   * compressed bytes are read from disk as they are consumed and nothing is
   * retained.
   * @param {{ verify?: boolean, maxSize?: number }} [options]
   * @returns {AsyncGenerator<Buffer>}
   */
  contentIterator(options) {
    return decodeMemberStream(this.#rawSource(), {
      name: this.name,
      flags: this.flags,
      method: this.method,
      crc32: this.crc32,
      uncompressedSize: this.size,
    }, options);
  }

  // Synchronous serialization: emit the local file header (sec. 4.3.7)
  // followed by the raw (already-compressed) content bytes. Streaming
  // (source-backed) entries cannot be serialized this way.
  *[SymbolIterator]() {
    if (this.#source) {
      throw new ERR_INVALID_STATE('a streaming entry cannot be serialized synchronously');
    }
    const meta = this.#finalizeMeta();
    yield buildLocalHeader(meta);
    if (this.#fd !== null) {
      yield* this.#rawChunksSync();
    } else if (this.#content?.length) {
      yield this.#content;
    }
  }

  // Asynchronous serialization. For a write-streaming entry, drain the
  // source once, computing CRC-32 and sizes on the fly and emitting a Zip64
  // data descriptor after the content (sec. 4.3.9); otherwise defer to the
  // buffered/file-backed paths.
  async *[SymbolAsyncIterator]() {
    const source = this.#source;
    if (!source) {
      if (this.#fd !== null) {
        yield buildLocalHeader(this.#finalizeMeta());
        yield* this.#rawChunks();
        return;
      }
      yield* this[SymbolIterator]();
      return;
    }
    if (this.#serialized) {
      throw new ERR_INVALID_STATE('a streaming entry can only be serialized once');
    }
    this.#serialized = true;
    const meta = this.#meta;
    yield buildLocalHeader(meta);
    let state = 0;
    let uncompressedSize = 0;
    let compressedSize = 0;
    const counted = (async function* () {
      for await (const chunk of source) {
        if (!isUint8Array(chunk)) {
          throw new ERR_INVALID_ARG_TYPE('chunk', 'Uint8Array', chunk);
        }
        if (!chunk.length) continue;
        state = crc32Native(chunk, state);
        uncompressedSize += chunk.length;
        yield chunk;
      }
    })();
    const output = meta.method === METHOD_DEFLATE ? deflateRawStream(counted) :
      meta.method === METHOD_ZSTD ? zstdCompressStream(counted) : counted;
    for await (const chunk of output) {
      compressedSize += chunk.length;
      yield chunk;
    }
    meta.crc = state;
    meta.uncompressedSize = uncompressedSize;
    meta.compressedSize = compressedSize;
    meta.pending = false;
    yield buildDataDescriptor64(meta.crc, compressedSize, uncompressedSize);
  }

  // Release the entry's write-source, if it has one. Only a streaming entry
  // (`createStream()`) owns a source - a caller-supplied `AsyncIterable`,
  // often a file read stream holding a descriptor - and once that entry is
  // handed to `createZipArchive()` the caller can no longer reach it, so the
  // archive machinery disposes it when the archive finishes or its output
  // stream is destroyed early (see `generateZipArchive()`); a caller holding
  // an unused streaming entry can dispose it directly. In-memory and
  // file-backed entries hold no source - a file-backed entry's descriptor
  // belongs to the `ZipFile`, never to the entry - so disposing them is a
  // no-op. Disposal is idempotent and marks the entry spent, so a disposed
  // streaming entry can no longer be serialized.
  #releaseSource() {
    const source = this.#source;
    this.#source = null;
    this.#serialized = true;
    return source;
  }
  [SymbolDispose]() {
    const source = this.#releaseSource();
    if (source === null) return;
    if (typeof source.destroy === 'function') source.destroy();
    else if (typeof source.return === 'function') source.return();
  }
  async [SymbolAsyncDispose]() {
    const source = this.#releaseSource();
    if (source === null) return;
    if (typeof source[SymbolAsyncDispose] === 'function') await source[SymbolAsyncDispose]();
    else if (typeof source.destroy === 'function') source.destroy();
    else if (typeof source.return === 'function') await source.return();
  }

  /**
   * Builds this entry's central-directory header (sec. 4.3.12) recording its
   * local-header start; called by the archive writer once the offset is fixed.
   *
   * `preserveDescriptorFlag` is set by `ZipFile`'s in-place
   * central-directory rewrite, which never regenerates local headers: when
   * the on-disk local header advertises a data descriptor (bit 3, which
   * `#finalizeMeta()` clears for full re-serialization), the rebuilt central
   * header must keep advertising it too, or the two headers would contradict
   * each other (sec. 4.3.12 expects them to agree). Full re-serialization
   * emits a fresh, bit-3-free local header from the same meta, so there the
   * cleared flag is the consistent one.
   * @private
   * @param {number} localOffset
   * @param {boolean} [preserveDescriptorFlag]
   * @returns {Buffer}
   */
  [kFinalize](localOffset, preserveDescriptorFlag = false) {
    validateInteger(localOffset, 'localOffset', 0, NumberMAX_SAFE_INTEGER);
    const meta = this.#finalizeMeta();
    if (preserveDescriptorFlag && this.#central !== null &&
        (this.#central.flags & FLAG_DATA_DESCRIPTOR) !== 0) {
      return buildCentralHeader(
        { ...meta, flags: meta.flags | FLAG_DATA_DESCRIPTOR }, localOffset);
    }
    return buildCentralHeader(meta, localOffset);
  }

  // Rebind a just-serialized write-streaming entry to its on-disk copy so it
  // stops being dead weight: after `addEntry()`/`addEntrySync()` writes the
  // entry into `fd` at `localOffset` (its local-header start), the spent
  // source is dropped and the entry becomes a readable, re-serializable
  // file-backed entry (valid while `fd` stays open). Only a spent stream
  // entry - one with neither an in-memory buffer nor an existing backing fd -
  // is promoted; in-memory and already-file-backed entries are left as they
  // are. The kept `#meta` still supplies the (now-final) name/sizes/crc.
  [kPromote](fd, localOffset) {
    if (this.#content !== null || this.#fd !== null) return;
    this.#fd = fd;
    this.#localOffset = localOffset;
    this.#contentOffset = -1;
    this.#source = null;
    this.#serialized = false;
    // The descriptor flag has served its purpose: the sizes and CRC are
    // known now, and the fd-backed serialization path emits plain headers
    // and never writes a descriptor. Left set, a re-serialization would
    // advertise (bit 3) a data descriptor that never follows - a corrupt
    // archive for any reader that honors the flag.
    this.#meta.flags &= ~FLAG_DATA_DESCRIPTOR;
  }

  /**
   * Parses an in-memory archive, walking the central directory (sec. 4.3.12)
   * and each referenced local header (sec. 4.3.7), and yields one read-only
   * `ZipEntry` per member.
   * @param {Buffer | TypedArray | DataView | ArrayBuffer} buffer
   * @yields {ZipEntry}
   */
  static *read(buffer) {
    const buf = toBuffer(buffer, 'buffer');
    yield* readArchiveEntries(buf, findArchiveEnd(buf));
  }

  /**
   * Builds a ready-to-serialize entry from in-memory `data`, compressing it
   * with the chosen method (falling back to store when that does not shrink
   * it) and recording the CRC-32 and sizes. When the entry ends up stored
   * (explicitly or via the fallback) it retains `data`'s memory rather than
   * copying it, and the CRC-32 is recorded now - mutating `data` afterwards
   * would corrupt the entry on write.
   * @param {string} filename
   * @param {Buffer | TypedArray | DataView | ArrayBuffer} data
   * @param {{
   *   comment?: string,
   *   mode?: number,
   *   modified?: Date,
   *   method?: 'deflate' | 'store' | 'zstd',
   * }} [options]
   * @returns {Promise<ZipEntry>}
   */
  // Shared head of create()/createSync(): validate, snapshot the CRC-32 and
  // uncompressed size, and pick the compression method (store is forced for
  // directories and empty content). The compression pass itself is the only
  // thing the async/sync builders do differently.
  static #prepareCreate(filename, data, options) {
    const meta = createEntryMeta(filename, options);
    const content = toBuffer(data, 'data');
    const isDirectory = StringPrototypeEndsWith(filename, '/');
    if (isDirectory && content.length) {
      throw new ERR_INVALID_ARG_VALUE('data', data, 'must be empty for a directory entry');
    }
    meta.crc = crc32Native(content, 0);
    meta.uncompressedSize = content.length;
    const method =
      isDirectory || content.length === 0 || options?.method === 'store' ? METHOD_STORE :
        options?.method === 'zstd' ? METHOD_ZSTD : METHOD_DEFLATE;
    return { meta, content, method };
  }

  // Shared tail of create()/createSync(): keep the compressed bytes only
  // when the pass actually shrank the content (otherwise store the original,
  // which also retains the caller's memory - see create()'s JSDoc), fill in
  // the final sizes, and construct the entry. `compressed` is null when
  // `method` is store.
  static #finishCreate(meta, content, method, compressed) {
    let finalContent = content;
    if (compressed !== null && compressed.length < content.length) {
      finalContent = compressed;
    } else if (compressed !== null) {
      method = METHOD_STORE; // Compression did not help; fall back to storing
    }
    meta.method = method;
    meta.compressedSize = finalContent.length;
    meta.pending = false;
    const entry = new ZipEntry(null, null, finalContent);
    entry.#meta = meta;
    return entry;
  }

  static async create(filename, data, options) {
    const { meta, content, method } = ZipEntry.#prepareCreate(filename, data, options);
    const compressed =
      method === METHOD_DEFLATE ? await deflateRawAsync(content) :
        method === METHOD_ZSTD ? await zstdCompressAsync(content) : null;
    return ZipEntry.#finishCreate(meta, content, method, compressed);
  }

  /**
   * The synchronous counterpart of `create()`. Blocks the event loop and
   * further JavaScript execution until done (including the deflate pass);
   * see `contentSync()`. Retains `data`'s memory when storing, same as
   * `create()`.
   * @param {string} filename
   * @param {Buffer | TypedArray | DataView | ArrayBuffer} data
   * @param {{
   *   comment?: string,
   *   mode?: number,
   *   modified?: Date,
   *   method?: 'deflate' | 'store' | 'zstd',
   * }} [options]
   * @returns {ZipEntry}
   */
  static createSync(filename, data, options) {
    const { meta, content, method } = ZipEntry.#prepareCreate(filename, data, options);
    const compressed =
      method === METHOD_DEFLATE ? deflateRawSync(content) :
        method === METHOD_ZSTD ? zstdCompressSync(content) : null;
    return ZipEntry.#finishCreate(meta, content, method, compressed);
  }

  /**
   * Builds a write-streaming entry whose content is drained from `source`
   * only at serialization time; its sizes/CRC are unknown until then, so it
   * sets the data-descriptor flag (bit 3, sec. 4.3.9) and stays pending.
   * @param {string} filename
   * @param {AsyncIterable<Buffer>} source
   * @param {{ comment?: string, mode?: number, modified?: Date, method?: 'deflate' | 'store' | 'zstd' }} [options]
   * @returns {ZipEntry}
   */
  static createStream(filename, source, options) {
    const meta = createEntryMeta(filename, options);
    if (StringPrototypeEndsWith(filename, '/')) {
      throw new ERR_INVALID_ARG_VALUE('filename', filename, 'a directory entry cannot be streamed');
    }
    meta.flags |= FLAG_DATA_DESCRIPTOR;
    meta.method = options?.method === 'store' ? METHOD_STORE :
      options?.method === 'zstd' ? METHOD_ZSTD : METHOD_DEFLATE;
    meta.pending = true;
    const entry = new ZipEntry(null, null, null);
    entry.#meta = meta;
    entry.#source = source;
    return entry;
  }

  /**
   * Creates a symbolic-link entry: a stored entry whose content is the link
   * target and whose Unix mode type bits are `S_IFLNK`.
   * @param {string} filename
   * @param {string} target The link target path.
   * @param {{ comment?: string, mode?: number, modified?: Date }} [options]
   * @returns {ZipEntry}
   */
  static createSymlink(filename, target, options) {
    validateString(target, 'target');
    const meta = createEntryMeta(filename, {
      __proto__: null,
      comment: options?.comment,
      mode: options?.mode,
      modified: options?.modified,
      symlink: true,
    });
    const content = Buffer.from(target, 'utf8');
    meta.crc = crc32Native(content, 0);
    meta.uncompressedSize = content.length;
    meta.method = METHOD_STORE;
    meta.compressedSize = content.length;
    meta.pending = false;
    const entry = new ZipEntry(null, null, content);
    entry.#meta = meta;
    return entry;
  }
}

// Walk the central directory described by `end` (a findArchiveEnd() result
// for `buf`), yielding one read-only ZipEntry per member. Split from
// `ZipEntry.read()` so `ZipBuffer` - which also needs the archive-end record
// for its comment - can locate the archive end once and share the result
// instead of scanning for it twice. All records are parsed and their member
// ranges cross-checked before the first entry is yielded: members must be
// disjoint and precede the central directory, or one small file could quote
// the same data region from N records - the "quoted overlap" zip-bomb shape
// (CVE-2024-0450 in other implementations). Here the local headers have
// been read, so the check uses each member's exact end (`ZipFile` applies
// the same rule with a lower bound; see `validateMemberBounds()`).
function* readArchiveEntries(buf, end) {
  let pos = end.centralDirectoryOffset;
  const cdEnd = end.centralDirectoryOffset + end.centralDirectorySize;
  const parsed = [];
  for (let index = 0; index < end.totalRecords; index++) {
    const central = new CentralFileHeader(buf, pos);
    if (pos + central.byteLength > cdEnd) {
      throw new ERR_ZIP_INVALID_ARCHIVE('central directory header is out of bounds');
    }
    if (central.diskNumber !== 0) {
      throw new ERR_ZIP_UNSUPPORTED_FEATURE('multi-disk archives are not supported');
    }
    const localOffset = central.localFileHeaderOffset + end.prefix;
    const local = new LocalFileHeader(buf, localOffset);
    const dataStart = localOffset + local.byteLength;
    const length = central.compressedSize;
    validateArchiveRange(buf, dataStart, length, 'entry data');
    const content = length ? buf.subarray(dataStart, dataStart + length) : EMPTY_BUFFER;
    ArrayPrototypePush(parsed, {
      entry: new ZipEntry(central, local, content),
      start: localOffset,
      dataEnd: dataStart + length,
    });
    pos = central.byteOffset + central.byteLength;
  }
  // Sort a separate range list; entries themselves are yielded in central
  // directory order.
  const ranges = ArrayPrototypeSort(ArrayPrototypeSlice(parsed), (a, b) => a.start - b.start);
  for (let i = 0; i < ranges.length; i++) {
    const bound = i + 1 < ranges.length ? ranges[i + 1].start : end.centralDirectoryOffset;
    if (ranges[i].dataEnd > bound) {
      throw new ERR_ZIP_INVALID_ARCHIVE(
        `entry ${JSONStringify(ranges[i].entry.name)} overlaps the next ` +
        'entry or the central directory (possible zip bomb)');
    }
  }
  for (let i = 0; i < parsed.length; i++) yield parsed[i].entry;
}

module.exports = {
  createEntryMeta,
  readArchiveEntries,
  ZipEntry,
};
