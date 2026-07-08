'use strict';

// `ZipFile`: random-access, in-place-writable view over an archive on disk
// (a raw fd), plus `buildCentralDirectoryChunks()` which rebuilds the central
// directory (and its Zip64/EOCD trailer) for the in-place rewrite path.

const {
  ArrayPrototypePush,
  ArrayPrototypeSort,
  FunctionPrototypeCall,
  JSONStringify,
  Map,
  MapPrototypeClear,
  MapPrototypeDelete,
  MapPrototypeEntries,
  MapPrototypeGet,
  MapPrototypeGetSize,
  MapPrototypeHas,
  MapPrototypeKeys,
  MapPrototypeSet,
  MathMin,
  PromisePrototypeThen,
  PromiseResolve,
  SymbolAsyncDispose,
  SymbolAsyncIterator,
  SymbolDispose,
  SymbolIterator,
  SymbolToStringTag,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_STATE,
    ERR_ZIP_ARCHIVE_TOO_LARGE,
    ERR_ZIP_ENTRY_NOT_FOUND,
    ERR_ZIP_INVALID_ARCHIVE,
    ERR_ZIP_NOT_WRITABLE,
  },
} = require('internal/errors');
const {
  validateBoolean,
  validateFunction,
  validateString,
} = require('internal/validators');
const { Buffer, kMaxLength } = require('buffer');
const { Readable } = require('stream');
const fs = require('fs');
const {
  TAIL_LENGTH,
  kFinalize,
  kPromote,
} = require('internal/zip/constants');
const {
  buildArchiveTrailer,
} = require('internal/zip/header-builders');
const {
  CentralFileHeader,
  findArchiveEnd,
  readCentralDirectory,
} = require('internal/zip/headers');
const { decodeZipText } = require('internal/zip/dos');
const {
  fsOpenAsync,
  fsCloseAsync,
  fsFstatAsync,
  fsFtruncateAsync,
  readFdFully,
  readFdFullySync,
  writeFdFully,
  writeFdFullySync,
} = require('internal/zip/fs-util');
const { ZipEntry } = require('internal/zip/entry');
const {
  createZipArchive,
  createZipArchiveSync,
} = require('internal/zip/archive');

/**
 * Builds a fresh central directory (sec. 4.3.12) plus its trailer (the Zip64
 * end record/locator when needed and the end-of-central-directory record;
 * see `buildArchiveTrailer()`) for `records`, an array of
 * `{ entry, localOffset }` pairs already in their final order and at their
 * final (possibly pre-existing, possibly freshly written) offsets.
 * @param {Array<{ entry: ZipEntry, localOffset: number }>} records
 * @param {number} centralDirectoryOffset
 * @param {Buffer} comment
 * @returns {{ centralHeaders: Buffer[], chunks: Buffer[] }}
 */
function buildCentralDirectoryChunks(records, centralDirectoryOffset, comment) {
  const centralHeaders = [];
  let centralDirectorySize = 0;
  for (let i = 0; i < records.length; i++) {
    // `true`: this rewrite leaves local headers on disk untouched, so an
    // entry whose local header advertises a data descriptor (bit 3) must
    // keep advertising it in the rebuilt central header; see [kFinalize].
    const header = records[i].entry[kFinalize](records[i].localOffset, true);
    ArrayPrototypePush(centralHeaders, header);
    centralDirectorySize += header.length;
  }
  const count = records.length;
  const chunks = [];
  for (let i = 0; i < centralHeaders.length; i++) ArrayPrototypePush(chunks, centralHeaders[i]);
  const trailer = buildArchiveTrailer(count, centralDirectorySize, centralDirectoryOffset, comment);
  for (let i = 0; i < trailer.length; i++) ArrayPrototypePush(chunks, trailer[i]);
  return { centralHeaders, chunks };
}

// Shared post-location validation for open()/openSync(): the archive tail
// has been found; make sure the central directory it describes can actually
// be buffered and lies inside the file.
function checkArchiveEnd(end, size) {
  if (end.centralDirectorySize > kMaxLength) {
    throw new ERR_ZIP_ARCHIVE_TOO_LARGE('the central directory is too large to buffer');
  }
  if (end.centralDirectoryOffset + end.centralDirectorySize > size) {
    throw new ERR_ZIP_INVALID_ARCHIVE('central directory is out of bounds');
  }
}

// Reject any member whose declared compressed bytes cannot lie inside the
// file, and any pair of members whose data ranges overlap each other or the
// central directory. The buffered read paths (`ZipEntry.prototype.content()`
// and friends) allocate `compressedSize` bytes before reading, so without
// the bounds check a tiny file whose central directory lies about a
// member's size could force an allocation of up to `kMaxLength` bytes. The
// overlap check counters the "quoted overlap" zip-bomb shape (CVE-2024-0450
// in other implementations): N central records quoting the same data region
// turn one small file into N full-size extractions, while real archives lay
// members out disjointly. The local header's exact length is not known
// without reading it, but it is at least 30 bytes, so
// `offset + 30 + compressedSize` is a safe lower bound for where the next
// member may start; trailing data descriptors only widen the true gap. The
// in-memory reader applies the same rules in `readArchiveEntries()`.
function validateMemberBounds(headers, prefix, size, centralDirectoryOffset) {
  const members = [];
  for (let i = 0; i < headers.length; i++) {
    const header = headers[i];
    const offset = header.localFileHeaderOffset + prefix;
    if (offset + header.compressedSize > size) {
      throw new ERR_ZIP_INVALID_ARCHIVE(
        `entry ${JSONStringify(header.fileName)} data is out of bounds`);
    }
    ArrayPrototypePush(members,
                       { header, offset, end: offset + 30 + header.compressedSize });
  }
  ArrayPrototypeSort(members, (a, b) => a.offset - b.offset);
  for (let i = 0; i < members.length; i++) {
    const bound = i + 1 < members.length ? members[i + 1].offset : centralDirectoryOffset;
    if (members[i].end > bound) {
      throw new ERR_ZIP_INVALID_ARCHIVE(
        `entry ${JSONStringify(members[i].header.fileName)} overlaps the next ` +
        'entry or the central directory (possible zip bomb)');
    }
  }
}

/**
 * A random-access view over the entries of a ZIP archive on disk. Only the
 * archive tail and central directory are read up front; individual member
 * content is read lazily and on demand. Writable when opened with
 * `{ writable: true }`: adding or deleting an entry rewrites the central
 * directory in place, appending new entry content where the old central
 * directory used to be.
 *
 * Every method has a `*Sync` counterpart. The synchronous methods block the
 * Node.js event loop and further JavaScript execution until the operation
 * completes - use them only where synchronous I/O is appropriate (for
 * example, short-lived scripts or startup code), never in code that must
 * stay responsive. A synchronous method throws `ERR_INVALID_STATE` if called
 * while an asynchronous `addEntry()`/`add()`/`delete()`/`close()` on the same
 * `ZipFile` has not settled yet, since letting the two interleave could
 * corrupt the archive.
 */
class ZipFile {
  #fd;
  #writable;
  #comment;
  #centralDirectoryOffset;
  #entries = new Map();
  #queue = PromiseResolve();
  #pendingAsyncOps = 0;

  /**
   * Builds the by-name entry map from the already-parsed central headers,
   * recording each member's local-header offset (adjusted by any prefix).
   * @private
   */
  constructor(fd, centralHeaders, prefix, centralDirectoryOffset, comment, writable) {
    this.#fd = fd;
    this.#writable = writable;
    this.#comment = comment;
    this.#centralDirectoryOffset = centralDirectoryOffset;
    for (let i = 0; i < centralHeaders.length; i++) {
      const central = centralHeaders[i];
      MapPrototypeSet(this.#entries, central.fileName, {
        central,
        entry: undefined,
        localOffset: central.localFileHeaderOffset + prefix,
      });
    }
  }
  get writable() { return this.#writable; }
  // The EOCD comment has no encoding flag; apply the same UTF-8/CP437
  // heuristic as unflagged member names and comments.
  get comment() { return decodeZipText(this.#comment, 0); }
  // Guard: throw unless this archive was opened writable.
  #assertWritable() {
    if (!this.#writable) throw new ERR_ZIP_NOT_WRITABLE();
  }
  // Guard: reject a synchronous call while an async mutation is still in
  // flight, since interleaving the two could corrupt the archive.
  #assertNotBusy() {
    if (this.#pendingAsyncOps > 0) {
      throw new ERR_INVALID_STATE(
        'cannot call a synchronous ZipFile method while an asynchronous ' +
        'add(), addEntry(), delete(), or close() call has not settled yet');
    }
  }
  // Serialize async mutations (add/delete/close) through a promise chain so
  // they never overlap and corrupt the archive; the in-flight counter backs
  // #assertNotBusy(). Failures do not break the chain (both arms continue it).
  #enqueue(fn) {
    this.#pendingAsyncOps++;
    const run = async () => {
      try {
        return await fn();
      } finally {
        this.#pendingAsyncOps--;
      }
    };
    const result = PromisePrototypeThen(this.#queue, run, run);
    this.#queue = PromisePrototypeThen(result, () => undefined, () => undefined);
    return result;
  }
  has(name) {
    validateString(name, 'name');
    return MapPrototypeHas(this.#entries, name);
  }
  // Return the lazy, file-backed ZipEntry handle for `info`, creating and
  // caching it on first access. The handle stores only a descriptor and the
  // local-header offset - never the member's content - so repeated `get()`s
  // return the same lightweight object and no content buffer is retained by
  // the ZipFile. Any read (`content()`, `contentIterator()`) goes to disk.
  #handleFor(info) {
    info.entry ??= new ZipEntry(info.central, null, null, this.#fd, info.localOffset);
    return info.entry;
  }
  /**
   * Returns a lazy, file-backed `ZipEntry` for `name`. Nothing is read from
   * disk here and no content is buffered; the entry reads (and, for
   * `content()`, decompresses) straight from the file on each access. The
   * returned entry is valid only while this `ZipFile` is open.
   * @param {string} name
   * @returns {Promise<ZipEntry>}
   */
  async get(name) {
    validateString(name, 'name');
    const info = MapPrototypeGet(this.#entries, name);
    if (info === undefined) throw new ERR_ZIP_ENTRY_NOT_FOUND(name);
    return this.#handleFor(info);
  }
  /**
   * The synchronous counterpart of `get()`. Like `get()`, it reads nothing
   * up front and buffers no content - it only builds the lazy handle - so it
   * does not itself block on I/O; see the class-level note on synchronous
   * methods for reads performed later through the returned entry.
   * @param {string} name
   * @returns {ZipEntry}
   */
  getSync(name) {
    this.#assertNotBusy();
    validateString(name, 'name');
    const info = MapPrototypeGet(this.#entries, name);
    if (info === undefined) throw new ERR_ZIP_ENTRY_NOT_FOUND(name);
    return this.#handleFor(info);
  }
  /**
   * Streams a member's decoded content without buffering the whole member,
   * as a `Readable` (verifying CRC-32 by default; `{ verify: false }` to opt
   * out). Sugar for wrapping `get(name).contentIterator(options)`; the
   * compressed bytes are read from disk as the stream is consumed.
   * @param {string} name
   * @param {{ verify?: boolean, maxSize?: number }} [options]
   * @returns {Promise<import('stream').Readable>}
   */
  async stream(name, options) {
    const entry = await this.get(name);
    return Readable.from(entry.contentIterator(options), { objectMode: false });
  }
  /**
   * Writes `entry`'s serialized bytes where the central directory currently
   * starts, then rewrites the central directory to include it. Replaces any
   * existing entry of the same name (its bytes become dead space, reclaimed
   * by `compact()`).
   * @param {ZipEntry} entry
   * @returns {Promise<ZipEntry>}
   */
  async addEntry(entry) {
    this.#assertWritable();
    if (!(entry instanceof ZipEntry)) {
      throw new ERR_INVALID_ARG_TYPE('entry', 'ZipEntry', entry);
    }
    return this.#enqueue(() => this.#doAdd(entry));
  }
  /**
   * Builds an entry from in-memory `data` and appends it; see `addEntry()`.
   * @param {string} filename
   * @param {Buffer | TypedArray | DataView | ArrayBuffer} data
   * @param {{ comment?: string, mode?: number, modified?: Date, method?: 'deflate' | 'store' | 'zstd' }} [options]
   * @returns {Promise<ZipEntry>}
   */
  async add(filename, data, options) {
    this.#assertWritable();
    return this.addEntry(await ZipEntry.create(filename, data, options));
  }
  // Append the entry's bytes where the central directory currently starts,
  // then rewrite the directory to include it. On write failure, restore the
  // original directory (the partial write may have clobbered it) and rethrow;
  // on success, promote a spent stream entry to its on-disk copy.
  async #doAdd(entry) {
    const localOffset = this.#centralDirectoryOffset;
    let written = 0;
    try {
      for await (const chunk of entry) {
        await writeFdFully(this.#fd, chunk, localOffset + written);
        written += chunk.length;
      }
    } catch (err) {
      // The failed entry's bytes start where the old central directory
      // started, so part of it may already be overwritten - while the EOCD
      // still points at it. Nothing has been adopted into memory yet, so
      // rebuild and rewrite the directory (and EOCD) at its original offset
      // to leave the archive exactly as it was before the call.
      try {
        await this.#rewriteCentralDirectory();
      } catch {
        // Restoring failed too (the device is likely full or gone); the
        // original error is the actionable one.
      }
      throw err;
    }
    this.#centralDirectoryOffset = localOffset + written;
    MapPrototypeSet(this.#entries, entry.name, { central: null, entry, localOffset });
    await this.#rewriteCentralDirectory();
    // The entry now has a stable home in this archive; if it was a spent
    // streaming entry, rebind it to that on-disk copy so it stays readable.
    entry[kPromote](this.#fd, localOffset);
    return entry;
  }
  /**
   * The synchronous counterpart of `addEntry()`. `entry` must not be a
   * pending streaming entry (one created with `ZipEntry.createStream()`) -
   * there is no synchronous way to drain its asynchronous source. Blocks the
   * event loop until done; see the class-level note on synchronous methods.
   * @param {ZipEntry} entry
   * @returns {ZipEntry}
   */
  addEntrySync(entry) {
    this.#assertWritable();
    this.#assertNotBusy();
    if (!(entry instanceof ZipEntry)) {
      throw new ERR_INVALID_ARG_TYPE('entry', 'ZipEntry', entry);
    }
    const localOffset = this.#centralDirectoryOffset;
    let written = 0;
    try {
      for (const chunk of entry) {
        writeFdFullySync(this.#fd, chunk, localOffset + written);
        written += chunk.length;
      }
    } catch (err) {
      // See #doAdd(): restore the (partially overwritten) central directory
      // before surfacing the failure.
      try {
        this.#rewriteCentralDirectorySync();
      } catch {
        // Restoring failed too; the original error is the actionable one.
      }
      throw err;
    }
    this.#centralDirectoryOffset = localOffset + written;
    MapPrototypeSet(this.#entries, entry.name, { central: null, entry, localOffset });
    this.#rewriteCentralDirectorySync();
    entry[kPromote](this.#fd, localOffset);
    return entry;
  }
  /**
   * The synchronous counterpart of `add()`. Blocks the event loop until
   * done (including the deflate pass); see the class-level note on
   * synchronous methods.
   * @param {string} filename
   * @param {Buffer | TypedArray | DataView | ArrayBuffer} data
   * @param {{ comment?: string, mode?: number, modified?: Date, method?: 'deflate' | 'store' | 'zstd' }} [options]
   * @returns {ZipEntry}
   */
  addSync(filename, data, options) {
    this.#assertWritable();
    return this.addEntrySync(ZipEntry.createSync(filename, data, options));
  }
  /**
   * Removes an entry by name. The central directory is rewritten in place
   * (no new content is written, so the archive does not grow); the removed
   * entry's bytes become dead space, reclaimed by `compact()`.
   * @param {string} name
   * @returns {Promise<boolean>}
   */
  async delete(name) {
    this.#assertWritable();
    validateString(name, 'name');
    return this.#enqueue(() => this.#doDelete(name));
  }
  // Drop the named entry and rewrite the central directory (no member bytes
  // move, so the file does not grow); reports whether it existed.
  async #doDelete(name) {
    const existed = MapPrototypeDelete(this.#entries, name);
    if (existed) await this.#rewriteCentralDirectory();
    return existed;
  }
  /**
   * The synchronous counterpart of `delete()`. Blocks the event loop until
   * done; see the class-level note on synchronous methods.
   * @param {string} name
   * @returns {boolean}
   */
  deleteSync(name) {
    this.#assertWritable();
    this.#assertNotBusy();
    validateString(name, 'name');
    const existed = MapPrototypeDelete(this.#entries, name);
    if (existed) this.#rewriteCentralDirectorySync();
    return existed;
  }
  // Snapshot the live entries as ordered { entry, localOffset } records for a
  // central-directory rebuild, materializing a plain ZipEntry for any member
  // not yet handed out as a lazy handle.
  #liveRecords() {
    const records = [];
    const names = [];
    for (const { 0: name, 1: value } of MapPrototypeEntries(this.#entries)) {
      ArrayPrototypePush(records, {
        entry: value.entry ?? new ZipEntry(value.central, null, null),
        localOffset: value.localOffset,
      });
      ArrayPrototypePush(names, name);
    }
    return { records, names };
  }
  // Rebuild the central directory and its trailer (sec. 4.3.12/4.3.16) for the
  // current live set and overwrite it in place at its current offset,
  // truncating any leftover tail, then adopt the freshly written headers.
  async #rewriteCentralDirectory() {
    const { records, names } = this.#liveRecords();
    const { centralHeaders, chunks } = buildCentralDirectoryChunks(
      records, this.#centralDirectoryOffset, this.#comment);
    let pos = this.#centralDirectoryOffset;
    for (let i = 0; i < chunks.length; i++) {
      await writeFdFully(this.#fd, chunks[i], pos);
      pos += chunks[i].length;
    }
    await fsFtruncateAsync(this.#fd, pos);
    this.#adoptRewrittenCentralDirectory(names, centralHeaders, records);
  }
  // Sync counterpart of #rewriteCentralDirectory().
  #rewriteCentralDirectorySync() {
    const { records, names } = this.#liveRecords();
    const { centralHeaders, chunks } = buildCentralDirectoryChunks(
      records, this.#centralDirectoryOffset, this.#comment);
    let pos = this.#centralDirectoryOffset;
    for (let i = 0; i < chunks.length; i++) {
      writeFdFullySync(this.#fd, chunks[i], pos);
      pos += chunks[i].length;
    }
    fs.ftruncateSync(this.#fd, pos);
    this.#adoptRewrittenCentralDirectory(names, centralHeaders, records);
  }
  // Re-derives fresh, disk-backed central headers from what was just
  // written, so every entry - original or freshly added - is uniformly
  // readable by offset from now on, regardless of whether its in-memory
  // ZipEntry (e.g. a streaming entry, whose source can only be consumed
  // once) is still around.
  #adoptRewrittenCentralDirectory(names, centralHeaders, records) {
    for (let i = 0; i < names.length; i++) {
      MapPrototypeSet(this.#entries, names[i], {
        central: new CentralFileHeader(centralHeaders[i], 0),
        entry: undefined,
        localOffset: records[i].localOffset,
      });
    }
  }
  /**
   * Serializes the currently live entries into a fresh archive stream,
   * leaving behind any dead space left by prior `addEntry()`/`delete()`
   * calls. Does not modify the open file; pipe the result into a new one.
   * @param {string} [comment]
   * @returns {import('stream').Readable}
   */
  compact(comment) {
    // Snapshot the live set now: a later addEntry()/delete() must not error
    // out (or change) an archive stream that is already being produced.
    // Reading the snapshot stays valid regardless of later mutations -
    // neither addEntry() nor delete() moves existing member bytes.
    const entries = [];
    for (const { 1: info } of MapPrototypeEntries(this.#entries)) {
      ArrayPrototypePush(entries, this.#handleFor(info));
    }
    return createZipArchive(entries, { comment: comment ?? this.#comment });
  }
  /**
   * The synchronous counterpart of `compact()`. Blocks the event loop until
   * the whole archive has been read and re-serialized; see the class-level
   * note on synchronous methods.
   * @param {string} [comment]
   * @returns {Buffer}
   */
  compactSync(comment) {
    this.#assertNotBusy();
    const entries = [];
    for (const { 1: info } of MapPrototypeEntries(this.#entries)) {
      ArrayPrototypePush(entries, this.#handleFor(info));
    }
    const chunks = [];
    for (const chunk of createZipArchiveSync(entries, { comment: comment ?? this.#comment })) {
      ArrayPrototypePush(chunks, chunk);
    }
    return Buffer.concat(chunks);
  }
  keys() { return MapPrototypeKeys(this.#entries); }
  *values() {
    for (const name of this.keys()) yield this.get(name);
  }
  /**
   * The synchronous counterpart of `values()`, yielding resolved `ZipEntry`
   * values instead of `Promise`s.
   * @yields {ZipEntry}
   */
  *valuesSync() {
    for (const name of this.keys()) yield this.getSync(name);
  }
  *entries() {
    for (const name of this.keys()) yield [name, this.get(name)];
  }
  /**
   * The synchronous counterpart of `entries()`, yielding resolved `ZipEntry`
   * values instead of `Promise`s.
   * @yields {[string, ZipEntry]}
   */
  *entriesSync() {
    for (const name of this.keys()) yield [name, this.getSync(name)];
  }
  // Async iteration yields each resolved ZipEntry (awaiting the lazy handles).
  async *[SymbolAsyncIterator]() {
    for (const promise of this.values()) yield await promise;
  }
  get size() { return MapPrototypeGetSize(this.#entries); }
  [SymbolIterator]() { return this.entries(); }
  get [SymbolToStringTag]() { return 'ZipFile'; }
  forEach(callback, thisArg) {
    validateFunction(callback, 'callback');
    for (const { 0: key, 1: value } of this.entries()) {
      FunctionPrototypeCall(callback, thisArg === undefined ? this : thisArg, value, key, this);
    }
  }
  /**
   * The synchronous counterpart of `forEach()`, invoking `callback` with a
   * resolved `ZipEntry` instead of a `Promise`.
   * @param {Function} callback
   * @param {*} [thisArg]
   */
  forEachSync(callback, thisArg) {
    validateFunction(callback, 'callback');
    for (const { 0: key, 1: value } of this.entriesSync()) {
      FunctionPrototypeCall(callback, thisArg === undefined ? this : thisArg, value, key, this);
    }
  }
  // Drop all entries and close the fd, queued behind any pending async ops.
  close() {
    return this.#enqueue(async () => {
      MapPrototypeClear(this.#entries);
      await fsCloseAsync(this.#fd);
    });
  }
  /**
   * The synchronous counterpart of `close()`; see the class-level note on
   * synchronous methods.
   */
  closeSync() {
    this.#assertNotBusy();
    MapPrototypeClear(this.#entries);
    fs.closeSync(this.#fd);
  }
  async [SymbolAsyncDispose]() {
    await this.close();
  }
  [SymbolDispose]() {
    this.closeSync();
  }
  /**
   * Opens an archive, reading only its tail and central directory up front
   * (member content stays on disk). Pass `{ writable: true }` for in-place
   * editing.
   * @param {string} filename
   * @param {{ writable?: boolean }} [options]
   * @returns {Promise<ZipFile>}
   */
  static async open(filename, options) {
    validateString(filename, 'filename');
    const writable = options?.writable ?? false;
    validateBoolean(writable, 'options.writable');
    const fd = await fsOpenAsync(filename, writable ? 'r+' : 'r');
    try {
      const stat = await fsFstatAsync(fd);
      const size = stat.size;
      const tailLength = MathMin(size, TAIL_LENGTH);
      const tail = Buffer.allocUnsafe(tailLength);
      await readFdFully(fd, tail, size - tailLength);
      let end = findArchiveEnd(tail, size - tailLength);
      if (end.needTailFrom !== undefined) {
        // The Zip64 EOCD record's extensible data sector pushes the record
        // start beyond the fixed-size tail; re-read from the recorded
        // offset (bounded by ZIP64_EOCD_MAX_LENGTH inside findArchiveEnd).
        const retry = Buffer.allocUnsafe(size - end.needTailFrom);
        await readFdFully(fd, retry, end.needTailFrom);
        end = findArchiveEnd(retry, end.needTailFrom);
        if (end.needTailFrom !== undefined) {
          throw new ERR_ZIP_INVALID_ARCHIVE(
            'Zip64 end of central directory record not found');
        }
      }
      checkArchiveEnd(end, size);
      const directory = Buffer.allocUnsafe(end.centralDirectorySize);
      await readFdFully(fd, directory, end.centralDirectoryOffset);
      const headers = readCentralDirectory(directory, end.totalRecords);
      validateMemberBounds(headers, end.prefix, size, end.centralDirectoryOffset);
      return new ZipFile(fd, headers, end.prefix, end.centralDirectoryOffset, end.comment, writable);
    } catch (err) {
      try {
        await fsCloseAsync(fd);
      } catch {
        // The archive failed to parse; the close error is not actionable.
      }
      throw err;
    }
  }
  /**
   * The synchronous counterpart of `open()`. Blocks the event loop and
   * further JavaScript execution until the archive's tail and central
   * directory have been read; see the class-level note on synchronous
   * methods.
   * @param {string} filename
   * @param {{ writable?: boolean }} [options]
   * @returns {ZipFile}
   */
  static openSync(filename, options) {
    validateString(filename, 'filename');
    const writable = options?.writable ?? false;
    validateBoolean(writable, 'options.writable');
    const fd = fs.openSync(filename, writable ? 'r+' : 'r');
    try {
      const size = fs.fstatSync(fd).size;
      const tailLength = MathMin(size, TAIL_LENGTH);
      const tail = Buffer.allocUnsafe(tailLength);
      readFdFullySync(fd, tail, size - tailLength);
      let end = findArchiveEnd(tail, size - tailLength);
      if (end.needTailFrom !== undefined) {
        // See open(): the Zip64 EOCD record starts before the fixed-size
        // tail; re-read from the recorded offset.
        const retry = Buffer.allocUnsafe(size - end.needTailFrom);
        readFdFullySync(fd, retry, end.needTailFrom);
        end = findArchiveEnd(retry, end.needTailFrom);
        if (end.needTailFrom !== undefined) {
          throw new ERR_ZIP_INVALID_ARCHIVE(
            'Zip64 end of central directory record not found');
        }
      }
      checkArchiveEnd(end, size);
      const directory = Buffer.allocUnsafe(end.centralDirectorySize);
      readFdFullySync(fd, directory, end.centralDirectoryOffset);
      const headers = readCentralDirectory(directory, end.totalRecords);
      validateMemberBounds(headers, end.prefix, size, end.centralDirectoryOffset);
      return new ZipFile(fd, headers, end.prefix, end.centralDirectoryOffset, end.comment, writable);
    } catch (err) {
      try {
        fs.closeSync(fd);
      } catch {
        // The archive failed to parse; the close error is not actionable.
      }
      throw err;
    }
  }
}

module.exports = {
  ZipFile,
};
