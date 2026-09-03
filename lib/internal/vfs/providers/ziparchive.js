'use strict';

const {
  ArrayPrototypeIndexOf,
  ArrayPrototypePush,
  MathMax,
  MathMin,
  Number,
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
const {
  VirtualFileHandle,
  decodeOpenFlags,
  kAccess,
} = require('internal/vfs/file_handle');
const {
  createEBADF,
  createEEXIST,
  createEINVAL,
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
    UV_DIRENT_DIR,
    UV_DIRENT_FILE,
  },
} = internalBinding('constants');
const { ZipBuffer, ZipFile } = require('internal/zip');

const EMPTY_BUFFER = Buffer.alloc(0);

function normalize(vfsPath) {
  return StringPrototypeStartsWith(vfsPath, '/') ? StringPrototypeSlice(vfsPath, 1) : vfsPath;
}

// The open flags decide by their bits, not by a string spelling: numeric
// `fs.constants` combinations such as `O_WRONLY` alone or `O_RDONLY | O_CREAT`
// have no string form, so `decodeOpenFlags` is the one place that interprets
// them and these helpers are thin readers over it. They accept both strings
// and numbers so call sites need not care which they were given.
function isCurrentPosition(position) {
  return position === null || position === undefined || position === -1;
}

function isWriteTruncate(flags) {
  return decodeOpenFlags(flags).truncate;
}

function isExclusive(flags) {
  const access = decodeOpenFlags(flags);
  return access.create && access.exclusive;
}

function mustExist(flags) {
  return !decodeOpenFlags(flags).create;
}

function isWritableFlag(flags) {
  return decodeOpenFlags(flags).writable;
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

function renameOptions(entry) {
  return {
    mode: entry.mode || undefined,
    modified: entry.modified,
    method: methodOption(entry.method),
  };
}

function directoryRenames(source, oldName, newName) {
  const entries = [];
  const prefix = `${oldName}/`;
  for (const name of source.keys()) {
    if (StringPrototypeStartsWith(name, prefix)) {
      ArrayPrototypePush(entries, {
        oldName: name,
        newName: newName + StringPrototypeSlice(name, oldName.length),
      });
    }
  }
  return entries;
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
  #entryMode;
  #modified;

  /**
   * @param {string} path
   * @param {string} flags
   * @param {number} mode
   * @param {ZipBuffer | ZipFile} source
   * @param {string} name The archive-relative entry name
   * @param {Buffer} initial The entry's current decompressed content, or an
   *   empty buffer for a new/truncated file
   */
  constructor(path, flags, mode, source, name, initial, entry = null, dirty = false) {
    super(path, flags, mode);
    // An existing entry keeps its own mode and modification time across a
    // rewrite; only a newly created file takes the mode open() was given.
    this.#entryMode = entry === null ? this.mode : (entry.mode || 0o644);
    this.#modified = entry === null ? null : entry.modified;
    // Creation and truncation take effect at open time on a real file, so
    // such a handle is committed on close even when nothing is written.
    this.#dirty = dirty;
    this.#source = source;
    this.#name = name;
    this.#buffer = initial;
    this.#size = initial.length;
  }

  // Access-mode violations are EBADF, as for any descriptor opened without
  // the needed access; EISDIR is for directories only.
  #checkReadable() {
    if (!this[kAccess].readable) throw createEBADF('read');
  }
  #checkWritable() {
    if (!this[kAccess].writable) throw createEBADF('write');
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
    // `position` may be a BigInt, which fs allows; the arithmetic below is
    // on numbers.
    const pos = useCurrent ? this.position : Number(position);
    const available = MathMax(0, this.#size - pos);
    const bytesRead = MathMin(length, available);
    if (bytesRead > 0) this.#buffer.copy(buffer, offset, pos, pos + bytesRead);
    if (useCurrent) this.position = pos + bytesRead;
    return { __proto__: null, bytesRead, buffer };
  }
  async read(buffer, offset, length, position) {
    return this.#doRead(buffer, offset, length, position);
  }
  // The synchronous form reports the count alone, like `fs.readSync` and the
  // memory handle; the `{ bytesRead, buffer }` shape belongs to the promise.
  readSync(buffer, offset, length, position) {
    return this.#doRead(buffer, offset, length, position).bytesRead;
  }

  #doWrite(buffer, offset, length, position) {
    this.#checkWritable();
    const useCurrent = isCurrentPosition(position);
    const pos = this[kAccess].append ? this.#size : (useCurrent ? this.position : Number(position));
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
    return this.#doWrite(buffer, offset, length, position).bytesWritten;
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

  // Writes from the current position (the end, in append mode), the way
  // `filehandle.writeFile()` does. Whether earlier content is discarded was
  // decided by the open flags: "w" has already truncated, "r+" overwrites in
  // place and keeps any tail. This is what makes `appendFile()` (built on
  // this by VirtualProvider's defaults) actually append.
  #doWriteFile(data, options) {
    const content = typeof data === 'string' ? Buffer.from(data, options?.encoding) : Buffer.from(data);
    this.#doWrite(content, 0, content.length, null);
  }
  async writeFile(data, options) {
    this.#doWriteFile(data, options);
  }
  writeFileSync(data, options) {
    this.#doWriteFile(data, options);
  }

  // Reports the entry's own mode and, until the handle has changed the file,
  // its own modification time; once dirty the file is as new as its close.
  #doStat() {
    return createFileStats(this.#size, {
      __proto__: null,
      mode: this.#entryMode,
      mtimeMs: this.#dirty || this.#modified === null ? undefined : this.#modified.getTime(),
    });
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
    // Growing exposes bytes past the old size. A fresh buffer is zeroed, but
    // one that was shrunk earlier still holds the cut-off content, which a
    // real file never hands back.
    if (len > this.#size) this.#buffer.fill(0, this.#size, len);
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
      await this.#source.add(this.#name, this.#buffer.subarray(0, this.#size),
                             { __proto__: null, mode: this.#entryMode });
    }
    await super.close();
  }
  closeSync() {
    if (this.#dirty && isWritableFlag(this.flags)) {
      this.#source.addSync(this.#name, this.#buffer.subarray(0, this.#size),
                           { __proto__: null, mode: this.#entryMode });
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
    const name = normalize(path);
    const fileEntry = await this.#getEntry(name);
    if (fileEntry === null && this.#isDirectory(name)) {
      throw createEISDIR('open', path);
    }
    const exists = fileEntry !== null;
    if ((isWritableFlag(flags) || !mustExist(flags)) && this.readonly) {
      throw createEROFS('open', path);
    }
    if (isExclusive(flags) && exists) {
      throw createEEXIST('open', path);
    }
    if (!exists && mustExist(flags)) {
      throw createENOENT('open', path);
    }
    let initial = EMPTY_BUFFER;
    if (exists && !isWriteTruncate(flags)) {
      initial = await fileEntry.content();
    }
    return new ZipFileHandle(path, flags, mode, this.#source, name, initial,
                             fileEntry, !exists || isWriteTruncate(flags));
  }
  openSync(path, flags, mode) {
    const name = normalize(path);
    const fileEntry = this.#getEntrySync(name);
    if (fileEntry === null && this.#isDirectory(name)) {
      throw createEISDIR('open', path);
    }
    const exists = fileEntry !== null;
    if ((isWritableFlag(flags) || !mustExist(flags)) && this.readonly) {
      throw createEROFS('open', path);
    }
    if (isExclusive(flags) && exists) {
      throw createEEXIST('open', path);
    }
    if (!exists && mustExist(flags)) {
      throw createENOENT('open', path);
    }
    let initial = EMPTY_BUFFER;
    if (exists && !isWriteTruncate(flags)) {
      initial = fileEntry.contentSync();
    }
    return new ZipFileHandle(path, flags, mode, this.#source, name, initial,
                             fileEntry, !exists || isWriteTruncate(flags));
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
    if (entry !== null && this.#isDirectory(newName)) {
      throw createEISDIR('rename', newPath);
    }
    const entries = entry === null ?
      directoryRenames(this.#source, oldName, newName) :
      [{ oldName, newName, entry }];
    if (entries.length === 0) throw createENOENT('rename', oldPath);
    if (oldName === newName) return;
    if (entry === null && StringPrototypeStartsWith(newName, `${oldName}/`)) {
      throw createEINVAL('rename', oldPath);
    }

    for (let i = 0; i < entries.length; i++) {
      const item = entries[i];
      item.entry ??= await this.#getEntry(item.oldName);
      await this.#source.add(
        item.newName, await item.entry.content(), renameOptions(item.entry));
    }
    for (let i = 0; i < entries.length; i++) {
      await this.#source.delete(entries[i].oldName);
    }
  }
  renameSync(oldPath, newPath) {
    if (this.readonly) throw createEROFS('rename', oldPath);
    const oldName = normalize(oldPath);
    const newName = normalize(newPath);
    const entry = this.#getEntrySync(oldName);
    if (entry !== null && this.#isDirectory(newName)) {
      throw createEISDIR('rename', newPath);
    }
    const entries = entry === null ?
      directoryRenames(this.#source, oldName, newName) :
      [{ oldName, newName, entry }];
    if (entries.length === 0) throw createENOENT('rename', oldPath);
    if (oldName === newName) return;
    if (entry === null && StringPrototypeStartsWith(newName, `${oldName}/`)) {
      throw createEINVAL('rename', oldPath);
    }

    for (let i = 0; i < entries.length; i++) {
      const item = entries[i];
      item.entry ??= this.#getEntrySync(item.oldName);
      this.#source.addSync(
        item.newName, item.entry.contentSync(), renameOptions(item.entry));
    }
    for (let i = 0; i < entries.length; i++) {
      this.#deleteEntrySync(entries[i].oldName);
    }
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
