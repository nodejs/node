'use strict';

const {
  DateNow,
  MathMin,
  Symbol,
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
   * @private
   */
  _checkClosed() {
    if (this[kClosed]) {
      throw createEBADF('read');
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
    this._checkClosed();
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
    this._checkClosed();
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
    this._checkClosed();
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
    this._checkClosed();
    throw new ERR_METHOD_NOT_IMPLEMENTED('writeSync');
  }

  /**
   * Reads the entire file.
   * @param {object|string} [options] Options or encoding
   * @returns {Promise<Buffer|string>}
   */
  async readFile(options) {
    this._checkClosed();
    throw new ERR_METHOD_NOT_IMPLEMENTED('readFile');
  }

  /**
   * Reads the entire file synchronously.
   * @param {object|string} [options] Options or encoding
   * @throws {ERR_METHOD_NOT_IMPLEMENTED} When not implemented by subclass
   */
  readFileSync(options) {
    this._checkClosed();
    throw new ERR_METHOD_NOT_IMPLEMENTED('readFileSync');
  }

  /**
   * Writes data to the file (replacing content).
   * @param {Buffer|string} data The data to write
   * @param {object} [options] Options
   * @returns {Promise<void>}
   */
  async writeFile(data, options) {
    this._checkClosed();
    throw new ERR_METHOD_NOT_IMPLEMENTED('writeFile');
  }

  /**
   * Writes data to the file synchronously (replacing content).
   * @param {Buffer|string} data The data to write
   * @param {object} [options] Options
   */
  writeFileSync(data, options) {
    this._checkClosed();
    throw new ERR_METHOD_NOT_IMPLEMENTED('writeFileSync');
  }

  /**
   * Gets file stats.
   * @param {object} [options] Options
   * @returns {Promise<Stats>}
   */
  async stat(options) {
    this._checkClosed();
    throw new ERR_METHOD_NOT_IMPLEMENTED('stat');
  }

  /**
   * Gets file stats synchronously.
   * @param {object} [options] Options
   * @throws {ERR_METHOD_NOT_IMPLEMENTED} When not implemented by subclass
   */
  statSync(options) {
    this._checkClosed();
    throw new ERR_METHOD_NOT_IMPLEMENTED('statSync');
  }

  /**
   * Truncates the file.
   * @param {number} [len] The new length
   * @returns {Promise<void>}
   */
  async truncate(len) {
    this._checkClosed();
    throw new ERR_METHOD_NOT_IMPLEMENTED('truncate');
  }

  /**
   * Truncates the file synchronously.
   * @param {number} [len] The new length
   */
  truncateSync(len) {
    this._checkClosed();
    throw new ERR_METHOD_NOT_IMPLEMENTED('truncateSync');
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

/**
 * A file handle for in-memory file content.
 * Used by MemoryProvider and similar providers.
 */
class MemoryFileHandle extends VirtualFileHandle {
  #content;
  #entry;
  #getStats;

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
    this.#entry = entry;
    this.#getStats = getStats;

    // Handle different open modes
    if (flags === 'w' || flags === 'w+') {
      // Write mode: truncate
      this.#content = Buffer.alloc(0);
      if (entry) {
        entry.content = this.#content;
      }
    } else if (flags === 'a' || flags === 'a+') {
      // Append mode: position at end
      this.position = this.#content.length;
    }
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
    return this.#content;
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
   * Gets the raw stored content (without dynamic resolution).
   * @returns {Buffer}
   */
  get _rawContent() {
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
    this._checkClosed();

    // Get content (resolves dynamic content providers)
    const content = this.content;
    const readPos = position !== null && position !== undefined ? position : this.position;
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
    this._checkClosed();

    const writePos = position !== null && position !== undefined ? position : this.position;
    const data = buffer.subarray(offset, offset + length);

    // Expand content if needed
    if (writePos + length > this.#content.length) {
      const newContent = Buffer.alloc(writePos + length);
      this.#content.copy(newContent, 0, 0, this.#content.length);
      this.#content = newContent;
    }

    // Write the data
    data.copy(this.#content, writePos);

    // Update the entry's content and mtime
    if (this.#entry) {
      this.#entry.content = this.#content;
      this.#entry.mtime = DateNow();
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
    this._checkClosed();

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
    this._checkClosed();

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
    this._checkClosed();

    const buffer = typeof data === 'string' ? Buffer.from(data, options?.encoding) : data;

    // In append mode, append to existing content
    if (this.flags === 'a' || this.flags === 'a+') {
      const newContent = Buffer.alloc(this.#content.length + buffer.length);
      this.#content.copy(newContent, 0);
      buffer.copy(newContent, this.#content.length);
      this.#content = newContent;
    } else {
      this.#content = Buffer.from(buffer);
    }

    // Update the entry's content and mtime
    if (this.#entry) {
      this.#entry.content = this.#content;
      this.#entry.mtime = DateNow();
    }

    this.position = this.#content.length;
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
    this._checkClosed();
    if (this.#getStats) {
      return this.#getStats(this.#content.length);
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
    this._checkClosed();

    if (len < this.#content.length) {
      this.#content = this.#content.subarray(0, len);
    } else if (len > this.#content.length) {
      const newContent = Buffer.alloc(len);
      this.#content.copy(newContent, 0, 0, this.#content.length);
      this.#content = newContent;
    }

    // Update the entry's content and mtime
    if (this.#entry) {
      this.#entry.content = this.#content;
      this.#entry.mtime = DateNow();
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
