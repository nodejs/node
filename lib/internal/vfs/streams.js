'use strict';

const {
  MathMin,
} = primordials;

const { Buffer } = require('buffer');
const { Readable, Writable } = require('stream');
const { createEBADF } = require('internal/vfs/errors');
const { getLazy, kEmptyObject } = require('internal/util');

// Lazy-load fd module to avoid circular dependency
const lazyGetVirtualFd = getLazy(
  () => require('internal/vfs/fd').getVirtualFd,
);

/**
 * A readable stream for virtual files.
 */
class VirtualReadStream extends Readable {
  #vfs;
  #path;
  #fd = null;
  #end;
  #pos;
  #content = null;
  #autoClose;

  /**
   * Number of bytes read so far.
   * @type {number}
   */
  bytesRead = 0;

  /**
   * True until the first read completes.
   * @type {boolean}
   */
  pending = true;

  /**
   * @param {VirtualFileSystem} vfs The VFS instance
   * @param {string} filePath The path to the file
   * @param {object} [options] Stream options
   */
  constructor(vfs, filePath, options = kEmptyObject) {
    const {
      start = 0,
      end = Infinity,
      highWaterMark = 64 * 1024,
      encoding,
      fd,
      ...streamOptions
    } = options;

    super({ ...streamOptions, highWaterMark, encoding });

    this.#vfs = vfs;
    this.#path = filePath;
    this.#end = end;
    this.#pos = start;
    this.#autoClose = options.autoClose !== false;

    if (fd !== null && fd !== undefined) {
      // Use the already-open file descriptor
      this.#fd = fd;
      process.nextTick(() => {
        this.emit('open', this.#fd);
        this.emit('ready');
      });
    } else {
      // Open the file on next tick so listeners can be attached.
      // Note: #openFile will not throw - if it fails, the stream is destroyed.
      process.nextTick(() => this.#openFile());
    }
  }

  /**
   * Gets the file path.
   * @returns {string}
   */
  get path() {
    return this.#path;
  }

  /**
   * Opens the virtual file.
   * Events are emitted synchronously within this method, which runs
   * asynchronously via process.nextTick - matching real fs behavior.
   */
  #openFile() {
    try {
      this.#fd = this.#vfs.openSync(this.#path);
      this.emit('open', this.#fd);
      this.emit('ready');
    } catch (err) {
      this.destroy(err);
    }
  }

  /**
   * Implements the readable _read method.
   * @param {number} size Number of bytes to read
   */
  _read(size) {
    if (this.destroyed || this.#fd === null) {
      this.destroy(createEBADF('read'));
      return;
    }

    // Load content on first read (lazy loading)
    if (this.#content === null) {
      try {
        const getVirtualFd = lazyGetVirtualFd();
        const vfd = getVirtualFd(this.#fd);
        if (!vfd) {
          this.destroy(createEBADF('read'));
          return;
        }
        // Use the file handle's readFileSync to get content
        this.#content = vfd.entry.readFileSync();
        this.pending = false;
      } catch (err) {
        this.destroy(err);
        return;
      }
    }

    // Calculate how much to read
    // Note: end is inclusive, so we use end + 1 for the upper bound
    const endPos = this.#end === Infinity ? this.#content.length : this.#end + 1;
    const remaining = MathMin(endPos, this.#content.length) - this.#pos;
    if (remaining <= 0) {
      this.push(null);
      return;
    }

    const bytesToRead = MathMin(size, remaining);
    const chunk = this.#content.subarray(this.#pos, this.#pos + bytesToRead);
    this.#pos += bytesToRead;
    this.bytesRead += bytesToRead;

    this.push(chunk);

    // Check if we've reached the end
    if (this.#pos >= endPos || this.#pos >= this.#content.length) {
      this.push(null);
    }
  }

  /**
   * Closes the file descriptor.
   * Note: Does not emit 'close' - the base Readable class handles that.
   */
  #close() {
    if (this.#fd !== null) {
      try {
        this.#vfs.closeSync(this.#fd);
      } catch {
        // Ignore close errors
      }
      this.#fd = null;
    }
  }

  /**
   * Implements the readable _destroy method.
   * @param {Error|null} err The error
   * @param {Function} callback Callback
   */
  _destroy(err, callback) {
    if (this.#autoClose) {
      this.#close();
    }
    callback(err);
  }
}

/**
 * A writable stream for virtual files.
 */
class VirtualWriteStream extends Writable {
  #vfs;
  #path;
  #fd = null;
  #autoClose;
  #start;

  /**
   * Number of bytes written so far.
   * @type {number}
   */
  bytesWritten = 0;

  /**
   * True until the first write completes.
   * @type {boolean}
   */
  pending = true;

  /**
   * @param {VirtualFileSystem} vfs The VFS instance
   * @param {string} filePath The path to the file
   * @param {object} [options] Stream options
   */
  constructor(vfs, filePath, options = kEmptyObject) {
    const {
      highWaterMark = 64 * 1024,
      ...streamOptions
    } = options;

    super({ ...streamOptions, highWaterMark });

    this.#vfs = vfs;
    this.#path = filePath;
    this.#autoClose = options.autoClose !== false;
    this.#start = options.start;

    const fd = options.fd;
    if (fd !== null && fd !== undefined) {
      // Use the already-open file descriptor
      this.#fd = fd;
      if (this.#start !== undefined) {
        this.#setPosition(this.#start);
      }
      process.nextTick(() => {
        this.emit('open', this.#fd);
        this.emit('ready');
      });
    } else {
      // Open file synchronously (VFS is in-memory) so writes can proceed
      // immediately. Emit events on next tick for listener attachment.
      const flags = options.flags || 'w';
      try {
        this.#fd = this.#vfs.openSync(this.#path, flags);
        if (this.#start !== undefined) {
          this.#setPosition(this.#start);
        }
      } catch (err) {
        process.nextTick(() => this.destroy(err));
        return;
      }
      process.nextTick(() => {
        this.emit('open', this.#fd);
        this.emit('ready');
      });
    }
  }

  /**
   * Sets the file handle position for the given fd.
   * @param {number} pos The position to set
   */
  #setPosition(pos) {
    const getVirtualFd = lazyGetVirtualFd();
    const vfd = getVirtualFd(this.#fd);
    if (vfd) {
      vfd.entry.position = pos;
    }
  }

  /**
   * Gets the file path.
   * @returns {string}
   */
  get path() {
    return this.#path;
  }

  /**
   * Implements the writable _write method.
   * @param {Buffer|string} chunk Data to write
   * @param {string} encoding Encoding
   * @param {Function} callback Callback
   */
  _write(chunk, encoding, callback) {
    if (this.destroyed || this.#fd === null) {
      callback(createEBADF('write'));
      return;
    }

    try {
      const buffer = typeof chunk === 'string' ?
        Buffer.from(chunk, encoding) : chunk;
      this.#vfs.writeSync(this.#fd, buffer, 0, buffer.length, null);
      this.bytesWritten += buffer.length;
      this.pending = false;
      callback();
    } catch (err) {
      callback(err);
    }
  }

  /**
   * Implements the writable _final method (flush before close).
   * @param {Function} callback Callback
   */
  _final(callback) {
    callback();
  }

  /**
   * Closes the file descriptor.
   */
  #close() {
    if (this.#fd !== null) {
      try {
        this.#vfs.closeSync(this.#fd);
      } catch {
        // Ignore close errors
      }
      this.#fd = null;
    }
  }

  /**
   * Implements the writable _destroy method.
   * @param {Error|null} err The error
   * @param {Function} callback Callback
   */
  _destroy(err, callback) {
    if (this.#autoClose) {
      this.#close();
    }
    callback(err);
  }
}

module.exports = {
  VirtualReadStream,
  VirtualWriteStream,
};
