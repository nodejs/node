'use strict';

// `ZipBuffer`: an in-memory, writable view over the entries of an archive
// held in a `Buffer`, serializing the current set back out with
// `toBuffer()`/`toBufferSync()`.

const {
  ArrayPrototypePush,
  FunctionPrototypeCall,
  Map,
  MapPrototypeClear,
  MapPrototypeDelete,
  MapPrototypeEntries,
  MapPrototypeGet,
  MapPrototypeGetSize,
  MapPrototypeHas,
  MapPrototypeKeys,
  MapPrototypeSet,
  SymbolDispose,
  SymbolIterator,
  SymbolToStringTag,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_ZIP_ENTRY_NOT_FOUND,
  },
} = require('internal/errors');
const {
  validateFunction,
  validateString,
} = require('internal/validators');
const { Buffer } = require('buffer');
const { toBuffer } = require('internal/zip/binary');
const { decodeZipText } = require('internal/zip/dos');
const { findArchiveEnd } = require('internal/zip/headers');
const {
  readArchiveEntries,
  ZipEntry,
} = require('internal/zip/entry');
const {
  createZipArchive,
  createZipArchiveSync,
  normalizeArchiveOptions,
} = require('internal/zip/archive');

/**
 * An in-memory view over the entries of a ZIP archive, writable in place:
 * entries can be added or removed, and `toBuffer()` serializes the current
 * set of entries into a fresh archive.
 */
class ZipBuffer {
  #entries = new Map();
  #comment;

  /**
   * Parses an existing archive's central directory into an in-memory,
   * editable map of entries keyed by name.
   * @param {Buffer | TypedArray | DataView | ArrayBuffer} buffer
   */
  constructor(buffer) {
    const buf = toBuffer(buffer, 'buffer');
    // Locate the archive end once; it supplies both the comment and the
    // central-directory bounds for the entry walk.
    const end = findArchiveEnd(buf);
    this.#comment = end.comment;
    for (const entry of readArchiveEntries(buf, end)) {
      MapPrototypeSet(this.#entries, entry.name, entry);
    }
  }
  get writable() { return true; }
  // The EOCD comment has no encoding flag; apply the same UTF-8/CP437
  // heuristic as unflagged member names and comments.
  get comment() { return decodeZipText(this.#comment, 0); }
  has(name) {
    validateString(name, 'name');
    return MapPrototypeHas(this.#entries, name);
  }
  get(name) {
    validateString(name, 'name');
    const entry = MapPrototypeGet(this.#entries, name);
    if (entry === undefined) throw new ERR_ZIP_ENTRY_NOT_FOUND(name);
    return entry;
  }
  /**
   * Adds an already-built entry, keyed by its own name (replacing any
   * existing entry of that name).
   * @param {ZipEntry} entry
   * @returns {ZipEntry}
   */
  addEntry(entry) {
    if (!(entry instanceof ZipEntry)) {
      throw new ERR_INVALID_ARG_TYPE('entry', 'ZipEntry', entry);
    }
    MapPrototypeSet(this.#entries, entry.name, entry);
    return entry;
  }
  /**
   * Builds an entry from in-memory `data` and adds it (replacing any entry of
   * the same name).
   * @param {string} filename
   * @param {Buffer | TypedArray | DataView | ArrayBuffer} data
   * @param {{ comment?: string, mode?: number, modified?: Date, method?: 'deflate' | 'store' | 'zstd' }} [options]
   * @returns {Promise<ZipEntry>}
   */
  async add(filename, data, options) {
    return this.addEntry(await ZipEntry.create(filename, data, options));
  }
  /**
   * The synchronous counterpart of `add()`. Blocks the event loop and
   * further JavaScript execution until done; see `zipEntry.contentSync()`.
   * @param {string} filename
   * @param {Buffer | TypedArray | DataView | ArrayBuffer} data
   * @param {{ comment?: string, mode?: number, modified?: Date, method?: 'deflate' | 'store' | 'zstd' }} [options]
   * @returns {ZipEntry}
   */
  addSync(filename, data, options) {
    return this.addEntry(ZipEntry.createSync(filename, data, options));
  }
  /**
   * @param {string} name
   * @returns {boolean}
   */
  delete(name) {
    validateString(name, 'name');
    return MapPrototypeDelete(this.#entries, name);
  }
  clear() {
    MapPrototypeClear(this.#entries);
  }
  keys() { return MapPrototypeKeys(this.#entries); }
  *values() {
    for (const name of this.keys()) yield this.get(name);
  }
  *entries() {
    for (const name of this.keys()) yield [name, this.get(name)];
  }
  get size() { return MapPrototypeGetSize(this.#entries); }
  [SymbolIterator]() { return this.entries(); }
  get [SymbolToStringTag]() { return 'ZipBuffer'; }
  forEach(callback, thisArg) {
    validateFunction(callback, 'callback');
    for (const { 0: key, 1: value } of MapPrototypeEntries(this.#entries)) {
      FunctionPrototypeCall(callback, thisArg === undefined ? this : thisArg, value, key, this);
    }
  }
  /**
   * Serializes the current set of entries into a fresh archive.
   * @param {string | { comment?: string, baseOffset?: number }} [options]
   * @returns {Promise<Buffer>}
   */
  async toBuffer(options) {
    const { comment, baseOffset } = normalizeArchiveOptions(options);
    const chunks = [];
    // Defaulting to the raw #comment bytes (not the decoded string)
    // round-trips a non-UTF-8 archive comment unchanged.
    for await (const chunk of createZipArchive(this.values(), { comment: comment ?? this.#comment, baseOffset })) {
      ArrayPrototypePush(chunks, chunk);
    }
    return Buffer.concat(chunks);
  }
  /**
   * The synchronous counterpart of `toBuffer()`. Blocks the event loop and
   * further JavaScript execution until the whole archive has been
   * serialized; see `zipEntry.contentSync()`.
   * @param {string | { comment?: string, baseOffset?: number }} [options]
   * @returns {Buffer}
   */
  toBufferSync(options) {
    const { comment, baseOffset } = normalizeArchiveOptions(options);
    const chunks = [];
    for (const chunk of createZipArchiveSync(this.values(), { comment: comment ?? this.#comment, baseOffset })) {
      ArrayPrototypePush(chunks, chunk);
    }
    return Buffer.concat(chunks);
  }
  // Dispose: drop all entries (this view holds no fd of its own).
  [SymbolDispose]() {
    MapPrototypeClear(this.#entries);
  }
}

module.exports = {
  ZipBuffer,
};
