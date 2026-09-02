'use strict';

const {
  ArrayPrototypeIndexOf,
  ArrayPrototypePush,
  MathMax,
  MathMin,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
  SymbolAsyncDispose,
  SymbolDispose,
} = primordials;

const { Buffer } = require('buffer');
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_METHOD_NOT_IMPLEMENTED,
  },
} = require('internal/errors');
const { VirtualProvider } = require('internal/vfs/provider');
const { VirtualFileHandle } = require('internal/vfs/file_handle');
const {
  createEEXIST,
  createEISDIR,
  createENOENT,
  createENOTDIR,
  createENOTEMPTY,
  createEROFS,
} = require('internal/vfs/errors');
const { createFileStats, createDirectoryStats } = require('internal/vfs/stats');
const { Dirent } = require('internal/fs/utils');
const {
  fs: {
    O_APPEND,
    O_CREAT,
    O_EXCL,
    O_RDWR,
    O_TRUNC,
    O_WRONLY,
    UV_DIRENT_DIR,
    UV_DIRENT_FILE,
  },
} = internalBinding('constants');
const { ZipBuffer, ZipFile } = require('internal/zip');

const EMPTY_BUFFER = Buffer.alloc(0);

function normalize(vfsPath) {
  return StringPrototypeStartsWith(vfsPath, '/') ? StringPrototypeSlice(vfsPath, 1) : vfsPath;
}

// Converts numeric open flags (e.g. `fs.constants.O_RDWR`) to the flag strings
// the helpers below understand, so a caller passing `node:fs`-style numeric
// flags through the VFS is handled the same way `fs` would. Strings pass
// through unchanged; anything else falls back to 'r'.
function normalizeFlags(flags) {
  if (typeof flags === 'string') return flags;
  if (typeof flags !== 'number') return 'r';
  const rdwr = (flags & O_RDWR) !== 0;
  const append = (flags & O_APPEND) !== 0;
  const excl = (flags & O_EXCL) !== 0;
  const write = (flags & O_WRONLY) !== 0 || (flags & O_CREAT) !== 0 || (flags & O_TRUNC) !== 0;
  if (append) return 'a' + (excl ? 'x' : '') + (rdwr ? '+' : '');
  if (write) return 'w' + (excl ? 'x' : '') + (rdwr ? '+' : '');
  if (rdwr) return 'r+';
  return 'r';
}

function isCurrentPosition(position) {
  return position === null || position === undefined || position === -1;
}

function isWriteTruncate(flags) {
  return flags === 'w' || flags === 'w+' || flags === 'wx' || flags === 'wx+';
}

function isAppend(flags) {
  return flags === 'a' || flags === 'a+' || flags === 'ax' || flags === 'ax+';
}

function isReadableFlag(flags) {
  return flags !== 'w' && flags !== 'a' && flags !== 'wx' && flags !== 'ax';
}

function isWritableFlag(flags) {
  return flags !== 'r';
}

/**
 * The `options.method` value that reproduces `method` (a `zipEntry.method`
 * raw compression method number) on `add()`/`addSync()`, so `rename()`
 * doesn't silently recompress an entry with a different method than the one
 * it already had (e.g. turning a zstd-compressed entry into a stored one).
 * @param {number} method
 * @returns {'store' | 'zstd' | 'deflate'}
 */
function methodOption(method) {
  if (method === 0) return 'store';
  if (method === 93) return 'zstd';
  return 'deflate';
}

/**
 * A file handle over one ZIP entry. ZIP members can't be edited in place
 * (they're a single compressed blob), so writes accumulate in memory and are
 * only committed - as a brand-new entry - when the handle is closed. Since
 * this is all in-memory buffer manipulation with no real I/O, every method
 * and its `*Sync` counterpart share one private implementation.
 */
class ZipFileHandle extends VirtualFileHandle {
  #source;
  #name;
  #buffer;
  #size;
  #dirty = false;

  /**
   * @param {string} path
   * @param {string} flags
   * @param {number} mode
   * @param {ZipBuffer | ZipFile} source
   * @param {string} name The archive-relative entry name
   * @param {Buffer} initial The entry's current decompressed content, or an
   *   empty buffer for a new/truncated file
   */
  constructor(path, flags, mode, source, name, initial) {
    super(path, flags, mode);
    this.#source = source;
    this.#name = name;
    this.#buffer = initial;
    this.#size = initial.length;
    if (isAppend(flags)) this.position = this.#size;
  }

  #checkReadable() {
    if (!isReadableFlag(this.flags)) throw createEISDIR('read', this.path);
  }
  #checkWritable() {
    if (!isWritableFlag(this.flags)) throw createEISDIR('write', this.path);
  }
  #ensureCapacity(size) {
    if (size <= this.#buffer.length) return;
    const capacity = MathMax(size, this.#buffer.length * 2);
    const grown = Buffer.alloc(capacity);
    this.#buffer.copy(grown, 0, 0, this.#size);
    this.#buffer = grown;
  }

  #doRead(buffer, offset, length, position) {
    this.#checkReadable();
    const useCurrent = isCurrentPosition(position);
    const pos = useCurrent ? this.position : position;
    const available = MathMax(0, this.#size - pos);
    const bytesRead = MathMin(length, available);
    if (bytesRead > 0) this.#buffer.copy(buffer, offset, pos, pos + bytesRead);
    if (useCurrent) this.position = pos + bytesRead;
    return { __proto__: null, bytesRead, buffer };
  }
  async read(buffer, offset, length, position) {
    return this.#doRead(buffer, offset, length, position);
  }
  readSync(buffer, offset, length, position) {
    return this.#doRead(buffer, offset, length, position);
  }

  #doWrite(buffer, offset, length, position) {
    this.#checkWritable();
    const useCurrent = isCurrentPosition(position);
    const pos = isAppend(this.flags) ? this.#size : (useCurrent ? this.position : position);
    this.#ensureCapacity(pos + length);
    buffer.copy(this.#buffer, pos, offset, offset + length);
    if (pos + length > this.#size) this.#size = pos + length;
    this.#dirty = true;
    if (useCurrent) this.position = pos + length;
    return { __proto__: null, bytesWritten: length, buffer };
  }
  async write(buffer, offset, length, position) {
    return this.#doWrite(buffer, offset, length, position);
  }
  writeSync(buffer, offset, length, position) {
    return this.#doWrite(buffer, offset, length, position);
  }

  #doReadFile(options) {
    this.#checkReadable();
    const encoding = typeof options === 'string' ? options : options?.encoding;
    const content = this.#buffer.subarray(0, this.#size);
    return encoding && encoding !== 'buffer' ? content.toString(encoding) : Buffer.from(content);
  }
  async readFile(options) {
    return this.#doReadFile(options);
  }
  readFileSync(options) {
    return this.#doReadFile(options);
  }

  // Replaces content, except in append mode ('a'/'a+'/'ax'/'ax+'), where it
  // appends to the existing content instead - matching MemoryFileHandle and
  // what makes `appendFile()`/`appendFileSync()` (built on this, by
  // VirtualProvider's defaults) actually append.
  #doWriteFile(data, options) {
    this.#checkWritable();
    const content = typeof data === 'string' ? Buffer.from(data, options?.encoding) : Buffer.from(data);
    if (isAppend(this.flags)) {
      this.#ensureCapacity(this.#size + content.length);
      content.copy(this.#buffer, this.#size);
      this.#size += content.length;
    } else {
      this.#buffer = content;
      this.#size = content.length;
    }
    this.#dirty = true;
  }
  async writeFile(data, options) {
    this.#doWriteFile(data, options);
  }
  writeFileSync(data, options) {
    this.#doWriteFile(data, options);
  }

  #doStat() {
    return createFileStats(this.#size, { mode: this.mode });
  }
  async stat(options) {
    return this.#doStat();
  }
  statSync(options) {
    return this.#doStat();
  }

  #doTruncate(len) {
    this.#checkWritable();
    this.#ensureCapacity(len);
    this.#size = len;
    this.#dirty = true;
  }
  async truncate(len = 0) {
    this.#doTruncate(len);
  }
  truncateSync(len = 0) {
    this.#doTruncate(len);
  }

  async close() {
    if (this.#dirty && isWritableFlag(this.flags)) {
      await this.#source.add(this.#name, this.#buffer.subarray(0, this.#size), { mode: this.mode });
    }
    await super.close();
  }
  closeSync() {
    if (this.#dirty && isWritableFlag(this.flags)) {
      this.#source.addSync(this.#name, this.#buffer.subarray(0, this.#size), { mode: this.mode });
    }
    super.closeSync();
  }
}

/**
 * A `node:vfs` provider backed by a ZIP archive: either a [`ZipBuffer`][] (in
 * memory) or a [`ZipFile`][] (on disk). Read-only unless the underlying
 * archive is writable (a `ZipBuffer`, or a `ZipFile` opened with
 * `{ writable: true }`). Every method has a synchronous counterpart, backed
 * by the equally complete synchronous surface `ZipBuffer`/`ZipFile` expose;
 * as with those, the synchronous methods here block the Node.js event loop
 * and further JavaScript execution until the operation (including any
 * deflate/inflate pass) completes.
 */
class ZipProvider extends VirtualProvider {
  #source;

  /**
   * @param {ZipBuffer | ZipFile} source
   */
  constructor(source) {
    super();
    if (!(source instanceof ZipBuffer) && !(source instanceof ZipFile)) {
      throw new ERR_INVALID_ARG_TYPE('source', ['ZipBuffer', 'ZipFile'], source);
    }
    this.#source = source;
  }

  get readonly() { return !this.#source.writable; }

  /**
   * @param {string} name
   * @returns {Promise<import('internal/zip').ZipEntry | null>}
   */
  async #getEntry(name) {
    return this.#source.has(name) ? this.#source.get(name) : null;
  }
  /**
   * @param {string} name
   * @returns {import('internal/zip').ZipEntry | null}
   */
  #getEntrySync(name) {
    if (!this.#source.has(name)) return null;
    // `ZipBuffer.prototype.get` is already synchronous (it has no `getSync`
    // of its own); `ZipFile.prototype.get` is asynchronous, so its `getSync`
    // is used instead when present.
    return typeof this.#source.getSync === 'function' ?
      this.#source.getSync(name) : this.#source.get(name);
  }
  /**
   * @param {string} name
   * @returns {boolean}
   */
  #deleteEntrySync(name) {
    // Same reasoning as `#getEntrySync`: `ZipBuffer.prototype.delete` is
    // already synchronous; `ZipFile.prototype.delete` is not, so its
    // `deleteSync` is used instead when present.
    return typeof this.#source.deleteSync === 'function' ?
      this.#source.deleteSync(name) : this.#source.delete(name);
  }

  /**
   * Whether `name` (no trailing slash) is a directory: either explicitly
   * (a `"name/"` entry) or implicitly (some entry starts with `"name/"`).
   * @param {string} name
   * @returns {boolean}
   */
  #isDirectory(name) {
    const prefix = `${name}/`;
    if (this.#source.has(prefix)) return true;
    for (const key of this.#source.keys()) {
      if (StringPrototypeStartsWith(key, prefix)) return true;
    }
    return false;
  }

  async open(path, flags, mode) {
    flags = normalizeFlags(flags);
    const name = normalize(path);
    const fileEntry = await this.#getEntry(name);
    if (fileEntry === null && this.#isDirectory(name)) {
      throw createEISDIR('open', path);
    }
    const exists = fileEntry !== null;
    if (isWritableFlag(flags) && this.readonly) {
      throw createEROFS('open', path);
    }
    if ((flags === 'wx' || flags === 'wx+' || flags === 'ax' || flags === 'ax+') && exists) {
      throw createEEXIST('open', path);
    }
    if (!exists && (flags === 'r' || flags === 'r+')) {
      throw createENOENT('open', path);
    }
    let initial = EMPTY_BUFFER;
    if (exists && !isWriteTruncate(flags)) {
      initial = await fileEntry.content();
    }
    return new ZipFileHandle(path, flags, mode, this.#source, name, initial);
  }
  openSync(path, flags, mode) {
    flags = normalizeFlags(flags);
    const name = normalize(path);
    const fileEntry = this.#getEntrySync(name);
    if (fileEntry === null && this.#isDirectory(name)) {
      throw createEISDIR('open', path);
    }
    const exists = fileEntry !== null;
    if (isWritableFlag(flags) && this.readonly) {
      throw createEROFS('open', path);
    }
    if ((flags === 'wx' || flags === 'wx+' || flags === 'ax' || flags === 'ax+') && exists) {
      throw createEEXIST('open', path);
    }
    if (!exists && (flags === 'r' || flags === 'r+')) {
      throw createENOENT('open', path);
    }
    let initial = EMPTY_BUFFER;
    if (exists && !isWriteTruncate(flags)) {
      initial = fileEntry.contentSync();
    }
    return new ZipFileHandle(path, flags, mode, this.#source, name, initial);
  }

  async stat(path, options) {
    const name = normalize(path);
    if (name === '') return createDirectoryStats({ mode: 0o755 });
    const entry = await this.#getEntry(name) ?? await this.#getEntry(`${name}/`);
    if (entry !== null) {
      return entry.isDirectory ?
        createDirectoryStats({ mode: entry.mode || 0o755, mtimeMs: entry.modified.getTime() }) :
        createFileStats(entry.size, { mode: entry.mode || 0o644, mtimeMs: entry.modified.getTime() });
    }
    if (this.#isDirectory(name)) return createDirectoryStats({ mode: 0o755 });
    throw createENOENT('stat', path);
  }
  statSync(path, options) {
    const name = normalize(path);
    if (name === '') return createDirectoryStats({ mode: 0o755 });
    const entry = this.#getEntrySync(name) ?? this.#getEntrySync(`${name}/`);
    if (entry !== null) {
      return entry.isDirectory ?
        createDirectoryStats({ mode: entry.mode || 0o755, mtimeMs: entry.modified.getTime() }) :
        createFileStats(entry.size, { mode: entry.mode || 0o644, mtimeMs: entry.modified.getTime() });
    }
    if (this.#isDirectory(name)) return createDirectoryStats({ mode: 0o755 });
    throw createENOENT('stat', path);
  }

  #readdirEntries(path, name, options, stats) {
    if (!stats.isDirectory()) throw createENOTDIR('scandir', path);
    const prefix = name === '' ? '' : `${name}/`;
    const withFileTypes = options?.withFileTypes === true;
    const names = [];
    const isDir = [];
    for (const key of this.#source.keys()) {
      if (!StringPrototypeStartsWith(key, prefix)) continue;
      const rest = StringPrototypeSlice(key, prefix.length);
      if (rest === '') continue; // The directory's own explicit entry
      const slash = StringPrototypeIndexOf(rest, '/');
      const childName = slash === -1 ? rest : StringPrototypeSlice(rest, 0, slash);
      const childIsDir = slash !== -1;
      const existingIndex = ArrayPrototypeIndexOf(names, childName);
      if (existingIndex !== -1) {
        if (childIsDir) isDir[existingIndex] = true;
        continue;
      }
      ArrayPrototypePush(names, childName);
      ArrayPrototypePush(isDir, childIsDir);
    }
    const result = [];
    for (let i = 0; i < names.length; i++) {
      if (withFileTypes) {
        ArrayPrototypePush(result, new Dirent(names[i], isDir[i] ? UV_DIRENT_DIR : UV_DIRENT_FILE, name));
      } else {
        ArrayPrototypePush(result, names[i]);
      }
    }
    return result;
  }
  async readdir(path, options) {
    if (options?.recursive) {
      throw new ERR_METHOD_NOT_IMPLEMENTED('readdir with { recursive: true } on a ZipProvider');
    }
    const name = normalize(path);
    return this.#readdirEntries(path, name, options, await this.stat(path));
  }
  readdirSync(path, options) {
    if (options?.recursive) {
      throw new ERR_METHOD_NOT_IMPLEMENTED('readdirSync with { recursive: true } on a ZipProvider');
    }
    const name = normalize(path);
    return this.#readdirEntries(path, name, options, this.statSync(path));
  }

  async mkdir(path, options) {
    if (this.readonly) throw createEROFS('mkdir', path);
    const name = normalize(path);
    if (await this.exists(path)) {
      // `{ recursive: true }` only tolerates an existing *directory*; an
      // existing file (or any non-directory) still collides with EEXIST.
      if (options?.recursive && this.#isDirectory(name)) return undefined;
      throw createEEXIST('mkdir', path);
    }
    await this.#source.add(`${name}/`, EMPTY_BUFFER, { mode: options?.mode });
    return undefined;
  }
  mkdirSync(path, options) {
    if (this.readonly) throw createEROFS('mkdir', path);
    const name = normalize(path);
    if (this.existsSync(path)) {
      // `{ recursive: true }` only tolerates an existing *directory*; an
      // existing file (or any non-directory) still collides with EEXIST.
      if (options?.recursive && this.#isDirectory(name)) return undefined;
      throw createEEXIST('mkdir', path);
    }
    this.#source.addSync(`${name}/`, EMPTY_BUFFER, { mode: options?.mode });
    return undefined;
  }

  async rmdir(path) {
    if (this.readonly) throw createEROFS('rmdir', path);
    const name = normalize(path);
    const stats = await this.stat(path);
    if (!stats.isDirectory()) throw createENOTDIR('rmdir', path);
    const prefix = `${name}/`;
    for (const key of this.#source.keys()) {
      if (key !== prefix && StringPrototypeStartsWith(key, prefix)) {
        throw createENOTEMPTY('rmdir', path);
      }
    }
    if (!this.#source.has(prefix)) {
      // An implicit-only directory can never be empty (something has to be
      // under it for it to exist at all), so getting here means `path` is
      // not a directory this provider can remove.
      throw createENOENT('rmdir', path);
    }
    await this.#source.delete(prefix);
  }
  rmdirSync(path) {
    if (this.readonly) throw createEROFS('rmdir', path);
    const name = normalize(path);
    const stats = this.statSync(path);
    if (!stats.isDirectory()) throw createENOTDIR('rmdir', path);
    const prefix = `${name}/`;
    for (const key of this.#source.keys()) {
      if (key !== prefix && StringPrototypeStartsWith(key, prefix)) {
        throw createENOTEMPTY('rmdir', path);
      }
    }
    if (!this.#source.has(prefix)) {
      throw createENOENT('rmdir', path);
    }
    this.#deleteEntrySync(prefix);
  }

  async unlink(path) {
    if (this.readonly) throw createEROFS('unlink', path);
    const name = normalize(path);
    if (!this.#source.has(name)) {
      throw this.#isDirectory(name) ? createEISDIR('unlink', path) : createENOENT('unlink', path);
    }
    await this.#source.delete(name);
  }
  unlinkSync(path) {
    if (this.readonly) throw createEROFS('unlink', path);
    const name = normalize(path);
    if (!this.#source.has(name)) {
      throw this.#isDirectory(name) ? createEISDIR('unlink', path) : createENOENT('unlink', path);
    }
    this.#deleteEntrySync(name);
  }

  async rename(oldPath, newPath) {
    if (this.readonly) throw createEROFS('rename', oldPath);
    const oldName = normalize(oldPath);
    const newName = normalize(newPath);
    const entry = await this.#getEntry(oldName);
    if (entry === null) throw createENOENT('rename', oldPath);
    const content = await entry.content();
    await this.#source.add(newName, content, {
      mode: entry.mode || undefined,
      modified: entry.modified,
      method: methodOption(entry.method),
    });
    await this.#source.delete(oldName);
  }
  renameSync(oldPath, newPath) {
    if (this.readonly) throw createEROFS('rename', oldPath);
    const oldName = normalize(oldPath);
    const newName = normalize(newPath);
    const entry = this.#getEntrySync(oldName);
    if (entry === null) throw createENOENT('rename', oldPath);
    const content = entry.contentSync();
    this.#source.addSync(newName, content, {
      mode: entry.mode || undefined,
      modified: entry.modified,
      method: methodOption(entry.method),
    });
    this.#deleteEntrySync(oldName);
  }

  /**
   * Closes the backing archive. A `ZipFile` releases its file descriptor; a
   * `ZipBuffer` is purely in-memory and has nothing to close, so this is a
   * no-op for it. Aliased as `Symbol.asyncDispose` so a provider can be used
   * with `await using`.
   */
  async close() {
    if (typeof this.#source.close === 'function') await this.#source.close();
  }
  /**
   * Synchronous counterpart of `close()`, aliased as `Symbol.dispose` for
   * `using`.
   */
  closeSync() {
    if (typeof this.#source.closeSync === 'function') this.#source.closeSync();
  }
}

ZipProvider.prototype[SymbolAsyncDispose] = ZipProvider.prototype.close;
ZipProvider.prototype[SymbolDispose] = ZipProvider.prototype.closeSync;

module.exports = {
  ZipProvider,
};
