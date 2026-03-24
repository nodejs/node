'use strict';

const {
  DateNow,
  MathMax,
  MathMin,
  Number,
  Symbol,
  SymbolAsyncDispose,
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

// Private symbols
const kPath = Symbol('kPath');
const kFlags = Symbol('kFlags');
const kMode = Symbol('kMode');
const kPosition = Symbol('kPosition');
const kClosed = Symbol('kClosed');

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
    this[kFlags] = flags;
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
   * No-op chmod — VFS files don't have real permissions.
   * @returns {Promise<void>}
   */
  async chmod() {}

  /**
   * No-op chown — VFS files don't have real ownership.
   * @returns {Promise<void>}
   */
  async chown() {}

  /**
   * No-op utimes — timestamps are handled by the provider.
   * @returns {Promise<void>}
   */
  async utimes() {}

  /**
   * No-op datasync — VFS is in-memory.
   * @returns {Promise<void>}
   */
  async datasync() {}

  /**
   * No-op sync — VFS is in-memory.
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

  /**
   * @returns {Promise<ReadableStream>}
   */
  readableWebStream() {
    throw new ERR_METHOD_NOT_IMPLEMENTED('readableWebStream');
  }

  /**
   * @returns {AsyncIterable}
   */
  readLines() {
    throw new ERR_METHOD_NOT_IMPLEMENTED('readLines');
  }

  /**
   * @returns {ReadStream}
   */
  createReadStream() {
    throw new ERR_METHOD_NOT_IMPLEMENTED('createReadStream');
  }

  /**
   * @returns {WriteStream}
   */
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

  [SymbolAsyncDispose]() {
    return this.close();
  }
}

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

    // Handle different open modes
    if (flags === 'w' || flags === 'w+' ||
        flags === 'wx' || flags === 'wx+') {
      // Write mode: truncate
      this.#content = Buffer.alloc(0);
      this.#size = 0;
      if (entry) {
        entry.content = this.#content;
      }
    } else if (flags === 'a' || flags === 'a+' ||
               flags === 'ax' || flags === 'ax+') {
      // Append mode: position at end
      this.position = this.#size;
    }
  }

  /**
   * Throws EBADF if the handle was not opened for writing.
   */
  #checkWritable() {
    if (this.flags === 'r') {
      throw createEBADF('write');
    }
  }

  /**
   * Throws EBADF if the handle was not opened for reading.
   */
  #checkReadable() {
    const f = this.flags;
    if (f === 'w' || f === 'a' || f === 'wx' || f === 'ax') {
      throw createEBADF('read');
    }
  }

  /**
   * Returns true if this handle was opened in append mode.
   * @returns {boolean}
   */
  #isAppend() {
    const f = this.flags;
    return f === 'a' || f === 'a+' || f === 'ax' || f === 'ax+';
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
    const readPos = position !== null && position !== undefined ?
      Number(position) : this.position;
    const available = content.length - readPos;

    if (available <= 0) {
      return 0;
    }

    const bytesToRead = MathMin(length, available);
    content.copy(buffer, offset, readPos, readPos + bytesToRead);

    // Update position if not using explicit position
    if (position === null || position === undefined) {
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
    const writePos = this.#isAppend() ?
      this.#size :
      (position !== null && position !== undefined ?
        Number(position) : this.position);
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
    if (position === null || position === undefined) {
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
   * Writes data to the file synchronously.
   * Replaces content in 'w' mode, appends in 'a' mode.
   * @param {Buffer|string} data The data to write
   * @param {object} [options] Options
   */
  writeFileSync(data, options) {
    this.#checkClosed('write');
    this.#checkWritable();

    const buffer = typeof data === 'string' ? Buffer.from(data, options?.encoding) : data;

    // In append mode, append to existing content
    if (this.#isAppend()) {
      const neededSize = this.#size + buffer.length;
      if (neededSize > this.#content.length) {
        const newCapacity = MathMax(neededSize, this.#content.length * 2);
        const newContent = Buffer.alloc(newCapacity);
        this.#content.copy(newContent, 0, 0, this.#size);
        this.#content = newContent;
      }
      buffer.copy(this.#content, this.#size);
      this.#size = neededSize;
    } else {
      this.#content = Buffer.from(buffer);
      this.#size = buffer.length;
    }

    // Update the entry's content, mtime, and ctime
    if (this.#entry) {
      const now = DateNow();
      this.#entry.content = this.#content.subarray(0, this.#size);
      this.#entry.mtime = now;
      this.#entry.ctime = now;
    }

    this.position = this.#size;
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
};
