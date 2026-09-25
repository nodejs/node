'use strict';

const {
  DateNow,
  MathMax,
  MathMin,
  Number,
  Symbol,
  SymbolAsyncDispose,
  SymbolDispose,
} = primordials;

const { Buffer } = require('buffer');
const {
  codes: {
    ERR_INVALID_STATE,
    ERR_METHOD_NOT_IMPLEMENTED,
  },
} = require('internal/errors');
const {
  createEBADF,
} = require('internal/vfs/errors');
const { stringToFlags, toUnixTimestamp } = require('internal/fs/utils');
const { parseFileMode } = require('internal/validators');

// Private symbols
const kPath = Symbol('kPath');
const kFlags = Symbol('kFlags');
const kMode = Symbol('kMode');
const kPosition = Symbol('kPosition');
const kClosed = Symbol('kClosed');
const kAccess = Symbol('kAccess');

const {
  fs: { O_APPEND, O_CREAT, O_EXCL, O_RDONLY, O_RDWR, O_TRUNC, O_WRONLY },
} = internalBinding('constants');

/**
 * Decodes open flags into what they ask for. Both the string spellings and
 * the numeric `fs.constants` values are accepted. The bits decide, because
 * several numeric combinations (`O_WRONLY` alone, `O_RDONLY | O_CREAT`)
 * have no string spelling, and collapsing them to the nearest one changes
 * their meaning: a plain `O_WRONLY` must neither create nor truncate.
 * @param {string|number} flags
 * @returns {{ readable: boolean, writable: boolean, create: boolean,
 *   exclusive: boolean, truncate: boolean, append: boolean }}
 */
function decodeOpenFlags(flags) {
  const bits = typeof flags === 'number' ? flags : stringToFlags(flags);
  const access = bits & (O_RDONLY | O_WRONLY | O_RDWR);
  return {
    __proto__: null,
    readable: access !== O_WRONLY,
    writable: access !== O_RDONLY,
    create: (bits & O_CREAT) !== 0,
    exclusive: (bits & O_EXCL) !== 0,
    truncate: (bits & O_TRUNC) !== 0,
    append: (bits & O_APPEND) !== 0,
  };
}

/**
 * The string spelling closest to numeric flags, for `handle.flags`, which
 * has always been a string. Behaviour is never derived from it.
 * @param {string|number} flags
 * @returns {string}
 */
function flagsToString(flags) {
  if (typeof flags !== 'number') return flags;
  const { readable, writable, exclusive, truncate, append } = decodeOpenFlags(flags);
  const plus = readable && writable ? '+' : '';
  const x = exclusive ? 'x' : '';
  if (append) return `a${x}${plus}`;
  if (truncate) return `w${x}${plus}`;
  return writable ? 'r+' : 'r';
}

function isCurrentPosition(position) {
  return position === null || position === undefined || position === -1;
}

/**
 * Base class for virtual file handles.
 * Provides the interface that file handles must implement.
 */
class VirtualFileHandle {
  /**
   * @param {string} path The file path
   * @param {string} flags The open flags
   * @param {number} [mode] The file mode
   */
  constructor(path, flags, mode) {
    this[kPath] = path;
    this[kAccess] = decodeOpenFlags(flags);
    this[kFlags] = flagsToString(flags);
    this[kMode] = mode ?? 0o644;
    this[kPosition] = 0;
    this[kClosed] = false;
  }

  /**
   * Gets the file path.
   * @returns {string}
   */
  get path() {
    return this[kPath];
  }

  /**
   * Gets the open flags.
   * @returns {string}
   */
  get flags() {
    return this[kFlags];
  }

  /**
   * Gets the file mode.
   * @returns {number}
   */
  get mode() {
    return this[kMode];
  }

  /**
   * Gets the current position.
   * @returns {number}
   */
  get position() {
    return this[kPosition];
  }

  /**
   * Sets the current position.
   * @param {number} pos The new position
   */
  set position(pos) {
    this[kPosition] = pos;
  }

  /**
   * Returns true if the handle is closed.
   * @returns {boolean}
   */
  get closed() {
    return this[kClosed];
  }

  /**
   * Throws if the handle is closed.
   * @param {string} syscall The syscall name for the error
   */
  #checkClosed(syscall) {
    if (this[kClosed]) {
      throw createEBADF(syscall);
    }
  }

  /**
   * Reads data from the file.
   * @param {Buffer} buffer The buffer to read into
   * @param {number} offset The offset in the buffer to start writing
   * @param {number} length The number of bytes to read
   * @param {number|null} position The position to read from (null uses current position)
   * @returns {Promise<{ bytesRead: number, buffer: Buffer }>}
   */
  async read(buffer, offset, length, position) {
    this.#checkClosed('read');
    throw new ERR_METHOD_NOT_IMPLEMENTED('read');
  }

  /**
   * Reads data from the file synchronously.
   * @param {Buffer} buffer The buffer to read into
   * @param {number} offset The offset in the buffer to start writing
   * @param {number} length The number of bytes to read
   * @param {number|null} position The position to read from (null uses current position)
   * @throws {ERR_METHOD_NOT_IMPLEMENTED} When not implemented by subclass
   */
  readSync(buffer, offset, length, position) {
    this.#checkClosed('read');
    throw new ERR_METHOD_NOT_IMPLEMENTED('readSync');
  }

  /**
   * Writes data to the file.
   * @param {Buffer} buffer The buffer to write from
   * @param {number} offset The offset in the buffer to start reading
   * @param {number} length The number of bytes to write
   * @param {number|null} position The position to write to (null uses current position)
   * @returns {Promise<{ bytesWritten: number, buffer: Buffer }>}
   */
  async write(buffer, offset, length, position) {
    this.#checkClosed('write');
    throw new ERR_METHOD_NOT_IMPLEMENTED('write');
  }

  /**
   * Writes data to the file synchronously.
   * @param {Buffer} buffer The buffer to write from
   * @param {number} offset The offset in the buffer to start reading
   * @param {number} length The number of bytes to write
   * @param {number|null} position The position to write to (null uses current position)
   * @throws {ERR_METHOD_NOT_IMPLEMENTED} When not implemented by subclass
   */
  writeSync(buffer, offset, length, position) {
    this.#checkClosed('write');
    throw new ERR_METHOD_NOT_IMPLEMENTED('writeSync');
  }

  /**
   * Reads the entire file.
   * @param {object|string} [options] Options or encoding
   * @returns {Promise<Buffer|string>}
   */
  async readFile(options) {
    this.#checkClosed('read');
    throw new ERR_METHOD_NOT_IMPLEMENTED('readFile');
  }

  /**
   * Reads the entire file synchronously.
   * @param {object|string} [options] Options or encoding
   * @throws {ERR_METHOD_NOT_IMPLEMENTED} When not implemented by subclass
   */
  readFileSync(options) {
    this.#checkClosed('read');
    throw new ERR_METHOD_NOT_IMPLEMENTED('readFileSync');
  }

  /**
   * Writes data to the file (replacing content).
   * @param {Buffer|string} data The data to write
   * @param {object} [options] Options
   * @returns {Promise<void>}
   */
  async writeFile(data, options) {
    this.#checkClosed('write');
    throw new ERR_METHOD_NOT_IMPLEMENTED('writeFile');
  }

  /**
   * Writes data to the file synchronously (replacing content).
   * @param {Buffer|string} data The data to write
   * @param {object} [options] Options
   */
  writeFileSync(data, options) {
    this.#checkClosed('write');
    throw new ERR_METHOD_NOT_IMPLEMENTED('writeFileSync');
  }

  /**
   * Gets file stats.
   * @param {object} [options] Options
   * @returns {Promise<Stats>}
   */
  async stat(options) {
    this.#checkClosed('fstat');
    throw new ERR_METHOD_NOT_IMPLEMENTED('stat');
  }

  /**
   * Gets file stats synchronously.
   * @param {object} [options] Options
   * @throws {ERR_METHOD_NOT_IMPLEMENTED} When not implemented by subclass
   */
  statSync(options) {
    this.#checkClosed('fstat');
    throw new ERR_METHOD_NOT_IMPLEMENTED('statSync');
  }

  /**
   * Truncates the file.
   * @param {number} [len] The new length
   * @returns {Promise<void>}
   */
  async truncate(len) {
    this.#checkClosed('ftruncate');
    throw new ERR_METHOD_NOT_IMPLEMENTED('truncate');
  }

  /**
   * Truncates the file synchronously.
   * @param {number} [len] The new length
   */
  truncateSync(len) {
    this.#checkClosed('ftruncate');
    throw new ERR_METHOD_NOT_IMPLEMENTED('truncateSync');
  }

  /**
   * @param {number} mode The new permission bits
   */
  chmodSync(mode) {}

  /**
   * @param {number} mode The new permission bits
   * @returns {Promise<void>}
   */
  async chmod(mode) {
    this.chmodSync(mode);
  }

  /**
   * No-op chown - VFS files don't have real ownership.
   * @returns {Promise<void>}
   */
  async chown() {}

  /**
   * @param {Date|number|string} atime The new access time
   * @param {Date|number|string} mtime The new modification time
   */
  utimesSync(atime, mtime) {}

  /**
   * @param {Date|number|string} atime The new access time
   * @param {Date|number|string} mtime The new modification time
   * @returns {Promise<void>}
   */
  async utimes(atime, mtime) {
    this.utimesSync(atime, mtime);
  }

  /**
   * No-op datasync - VFS is in-memory.
   * @returns {Promise<void>}
   */
  async datasync() {}

  /**
   * No-op sync - VFS is in-memory.
   * @returns {Promise<void>}
   */
  async sync() {}

  /**
   * Reads data from the file into multiple buffers.
   * @param {Buffer[]} buffers The buffers to read into
   * @param {number|null} [position] The position to read from
   * @returns {Promise<{ bytesRead: number, buffers: Buffer[] }>}
   */
  async readv(buffers, position) {
    this.#checkClosed('readv');
    let totalRead = 0;
    for (let i = 0; i < buffers.length; i++) {
      const buf = buffers[i];
      const pos = position != null ? position + totalRead : null;
      const { bytesRead } = await this.read(buf, 0, buf.byteLength, pos);
      totalRead += bytesRead;
      if (bytesRead < buf.byteLength) break;
    }
    return { __proto__: null, bytesRead: totalRead, buffers };
  }

  /**
   * Writes data from multiple buffers to the file.
   * @param {Buffer[]} buffers The buffers to write from
   * @param {number|null} [position] The position to write to
   * @returns {Promise<{ bytesWritten: number, buffers: Buffer[] }>}
   */
  async writev(buffers, position) {
    this.#checkClosed('writev');
    let totalWritten = 0;
    for (let i = 0; i < buffers.length; i++) {
      const buf = buffers[i];
      const pos = position != null ? position + totalWritten : null;
      const { bytesWritten } = await this.write(buf, 0, buf.byteLength, pos);
      totalWritten += bytesWritten;
      if (bytesWritten < buf.byteLength) break;
    }
    return { __proto__: null, bytesWritten: totalWritten, buffers };
  }

  /**
   * Appends data to the file.
   * @param {Buffer|string} data The data to append
   * @param {object} [options] Options
   * @returns {Promise<void>}
   */
  async appendFile(data, options) {
    this.#checkClosed('appendFile');
    const buffer = typeof data === 'string' ?
      Buffer.from(data, options?.encoding) : data;
    await this.write(buffer, 0, buffer.length, null);
  }

  readableWebStream() {
    throw new ERR_METHOD_NOT_IMPLEMENTED('readableWebStream');
  }

  readLines() {
    throw new ERR_METHOD_NOT_IMPLEMENTED('readLines');
  }

  createReadStream() {
    throw new ERR_METHOD_NOT_IMPLEMENTED('createReadStream');
  }

  createWriteStream() {
    throw new ERR_METHOD_NOT_IMPLEMENTED('createWriteStream');
  }

  /**
   * Closes the file handle.
   * @returns {Promise<void>}
   */
  async close() {
    this[kClosed] = true;
  }

  /**
   * Closes the file handle synchronously.
   */
  closeSync() {
    this[kClosed] = true;
  }
}

VirtualFileHandle.prototype[SymbolAsyncDispose] = VirtualFileHandle.prototype.close;
VirtualFileHandle.prototype[SymbolDispose] = VirtualFileHandle.prototype.closeSync;

/**
 * A file handle for in-memory file content.
 * Used by MemoryProvider and similar providers.
 */
class MemoryFileHandle extends VirtualFileHandle {
  #content;
  #size;
  #entry;
  #getStats;

  #checkClosed(syscall) {
    if (this.closed) {
      throw createEBADF(syscall);
    }
  }

  /**
   * @param {string} path The file path
   * @param {string} flags The open flags
   * @param {number} [mode] The file mode
   * @param {Buffer} content The initial file content
   * @param {object} entry The entry object (for updating content)
   * @param {Function} getStats Function to get updated stats
   */
  constructor(path, flags, mode, content, entry, getStats) {
    super(path, flags, mode);
    this.#content = content;
    this.#size = content.length;
    this.#entry = entry;
    this.#getStats = getStats;

    // O_TRUNC empties the file at open time. O_APPEND does not move the
    // read offset: it only forces writes to the end, so the position stays
    // at 0 and reads start from the beginning as they do on a real file.
    if (this[kAccess].truncate) {
      this.#content = Buffer.alloc(0);
      this.#size = 0;
      if (entry) {
        entry.content = this.#content;
      }
    }
  }

  /**
   * Throws EBADF if the handle was not opened for writing.
   */
  #checkWritable() {
    if (!this[kAccess].writable) {
      throw createEBADF('write');
    }
  }

  /**
   * Throws EBADF if the handle was not opened for reading.
   */
  #checkReadable() {
    if (!this[kAccess].readable) {
      throw createEBADF('read');
    }
  }

  /**
   * Returns true if this handle was opened in append mode.
   * @returns {boolean}
   */
  #isAppend() {
    return this[kAccess].append;
  }

  /**
   * Gets the current content synchronously.
   * For dynamic content providers, this gets fresh content from the entry.
   * @returns {Buffer}
   */
  get content() {
    // If entry has a dynamic content provider, get fresh content sync
    if (this.#entry?.isDynamic && this.#entry.isDynamic()) {
      return this.#entry.getContentSync();
    }
    return this.#content.subarray(0, this.#size);
  }

  /**
   * Gets the current content asynchronously.
   * For dynamic content providers, this gets fresh content from the entry.
   * @returns {Promise<Buffer>}
   */
  async getContentAsync() {
    // If entry has a dynamic content provider, get fresh content async
    if (this.#entry?.getContentAsync) {
      return this.#entry.getContentAsync();
    }
    return this.#content;
  }

  /**
   * Reads data from the file synchronously.
   * @param {Buffer} buffer The buffer to read into
   * @param {number} offset The offset in the buffer to start writing
   * @param {number} length The number of bytes to read
   * @param {number|null} position The position to read from (null uses current position)
   * @returns {number} The number of bytes read
   */
  readSync(buffer, offset, length, position) {
    this.#checkClosed('read');
    this.#checkReadable();

    // Get content (resolves dynamic content providers)
    const content = this.content;
    const useCurrentPosition = isCurrentPosition(position);
    const readPos = useCurrentPosition ? this.position : Number(position);
    const available = content.length - readPos;

    if (available <= 0) {
      return 0;
    }

    const bytesToRead = MathMin(length, available);
    content.copy(buffer, offset, readPos, readPos + bytesToRead);

    // Update position if not using explicit position
    if (useCurrentPosition) {
      this.position = readPos + bytesToRead;
    }

    return bytesToRead;
  }

  /**
   * Reads data from the file.
   * @param {Buffer} buffer The buffer to read into
   * @param {number} offset The offset in the buffer to start writing
   * @param {number} length The number of bytes to read
   * @param {number|null} position The position to read from (null uses current position)
   * @returns {Promise<{ bytesRead: number, buffer: Buffer }>}
   */
  async read(buffer, offset, length, position) {
    const bytesRead = this.readSync(buffer, offset, length, position);
    return { __proto__: null, bytesRead, buffer };
  }

  /**
   * Writes data to the file synchronously.
   * @param {Buffer} buffer The buffer to write from
   * @param {number} offset The offset in the buffer to start reading
   * @param {number} length The number of bytes to write
   * @param {number|null} position The position to write to (null uses current position)
   * @returns {number} The number of bytes written
   */
  writeSync(buffer, offset, length, position) {
    this.#checkClosed('write');
    this.#checkWritable();

    // In append mode, always write at the end
    const useCurrentPosition = isCurrentPosition(position);
    const writePos = this.#isAppend() ?
      this.#size :
      (useCurrentPosition ? this.position : Number(position));
    const data = buffer.subarray(offset, offset + length);

    // Expand buffer if needed (geometric doubling for amortized O(1) appends)
    const neededSize = writePos + length;
    if (neededSize > this.#content.length) {
      const newCapacity = MathMax(neededSize, this.#content.length * 2);
      const newContent = Buffer.alloc(newCapacity);
      this.#content.copy(newContent, 0, 0, this.#size);
      this.#content = newContent;
    }

    // Write the data
    data.copy(this.#content, writePos);

    // Update actual content size
    if (neededSize > this.#size) {
      this.#size = neededSize;
    }

    // Update the entry's content, mtime, and ctime
    if (this.#entry) {
      const now = DateNow();
      this.#entry.content = this.#content.subarray(0, this.#size);
      this.#entry.mtime = now;
      this.#entry.ctime = now;
    }

    // Update position if not using explicit position
    if (useCurrentPosition) {
      this.position = writePos + length;
    }

    return length;
  }

  /**
   * Writes data to the file.
   * @param {Buffer} buffer The buffer to write from
   * @param {number} offset The offset in the buffer to start reading
   * @param {number} length The number of bytes to write
   * @param {number|null} position The position to write to (null uses current position)
   * @returns {Promise<{ bytesWritten: number, buffer: Buffer }>}
   */
  async write(buffer, offset, length, position) {
    const bytesWritten = this.writeSync(buffer, offset, length, position);
    return { __proto__: null, bytesWritten, buffer };
  }

  /**
   * Reads the entire file synchronously.
   * @param {object|string} [options] Options or encoding
   * @returns {Buffer|string}
   */
  readFileSync(options) {
    this.#checkClosed('read');
    this.#checkReadable();

    // Get content (resolves dynamic content providers)
    const content = this.content;
    const encoding = typeof options === 'string' ? options : options?.encoding;
    if (encoding) {
      return content.toString(encoding);
    }
    return Buffer.from(content);
  }

  /**
   * Reads the entire file.
   * @param {object|string} [options] Options or encoding
   * @returns {Promise<Buffer|string>}
   */
  async readFile(options) {
    this.#checkClosed('read');
    this.#checkReadable();

    // Get content asynchronously (supports async content providers)
    const content = await this.getContentAsync();
    const encoding = typeof options === 'string' ? options : options?.encoding;
    if (encoding) {
      return content.toString(encoding);
    }
    return Buffer.from(content);
  }

  /**
   * Writes data to the file synchronously, from the current position (the
   * end, in append mode), the way `filehandle.writeFile()` does. Whether
   * earlier content is discarded was decided by the open flags: "w" has
   * already truncated, "r+" overwrites in place and keeps any tail.
   * @param {Buffer|string} data The data to write
   * @param {object} [options] Options
   */
  writeFileSync(data, options) {
    const buffer = typeof data === 'string' ? Buffer.from(data, options?.encoding) : data;
    this.writeSync(buffer, 0, buffer.length, null);
  }

  /**
   * Writes data to the file (replacing content).
   * @param {Buffer|string} data The data to write
   * @param {object} [options] Options
   * @returns {Promise<void>}
   */
  async writeFile(data, options) {
    this.writeFileSync(data, options);
  }

  /**
   * Gets file stats synchronously.
   * @param {object} [options] Options
   * @returns {Stats}
   */
  statSync(options) {
    this.#checkClosed('fstat');
    if (this.#getStats) {
      return this.#getStats(this.#size);
    }
    throw new ERR_INVALID_STATE('stats not available');
  }

  /**
   * @param {number} mode The new permission bits
   */
  chmodSync(mode) {
    this.#checkClosed('fchmod');
    mode = parseFileMode(mode, 'mode');
    if (this.#entry) {
      this.#entry.mode = (this.#entry.mode & ~0o7777) | (mode & 0o7777);
      this.#entry.ctime = DateNow();
    }
  }

  /**
   * @param {Date|number|string} atime The new access time
   * @param {Date|number|string} mtime The new modification time
   */
  utimesSync(atime, mtime) {
    this.#checkClosed('futimes');
    const atimeMs = toUnixTimestamp(atime, 'atime') * 1000;
    const mtimeMs = toUnixTimestamp(mtime, 'mtime') * 1000;
    if (this.#entry) {
      this.#entry.atime = atimeMs;
      this.#entry.mtime = mtimeMs;
      this.#entry.ctime = DateNow();
    }
  }

  /**
   * Gets file stats.
   * @param {object} [options] Options
   * @returns {Promise<Stats>}
   */
  async stat(options) {
    return this.statSync(options);
  }

  /**
   * Truncates the file synchronously.
   * @param {number} [len] The new length
   */
  truncateSync(len = 0) {
    this.#checkClosed('ftruncate');
    this.#checkWritable();

    if (len < this.#size) {
      // Zero out truncated region to avoid stale data
      this.#content.fill(0, len, this.#size);
      this.#size = len;
    } else if (len > this.#size) {
      if (len > this.#content.length) {
        const newContent = Buffer.alloc(len);
        this.#content.copy(newContent, 0, 0, this.#size);
        this.#content = newContent;
      } else {
        // Buffer has enough capacity, just zero-fill the extension
        this.#content.fill(0, this.#size, len);
      }
      this.#size = len;
    }

    // Update the entry's content, mtime, and ctime
    if (this.#entry) {
      const now = DateNow();
      this.#entry.content = this.#content.subarray(0, this.#size);
      this.#entry.mtime = now;
      this.#entry.ctime = now;
    }
  }

  /**
   * Truncates the file.
   * @param {number} [len] The new length
   * @returns {Promise<void>}
   */
  async truncate(len) {
    this.truncateSync(len);
  }
}

module.exports = {
  VirtualFileHandle,
  MemoryFileHandle,
  decodeOpenFlags,
  kAccess,
};
