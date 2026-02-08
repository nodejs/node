'use strict';

const {
  MathMin,
} = primordials;

const { Readable } = require('stream');
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
  #destroyed = false;
  #autoClose;

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
      ...streamOptions
    } = options;

    super({ ...streamOptions, highWaterMark, encoding });

    this.#vfs = vfs;
    this.#path = filePath;
    this.#end = end;
    this.#pos = start;
    this.#autoClose = options.autoClose !== false;

    // Open the file on next tick so listeners can be attached
    process.nextTick(() => this.#openFile());
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
    if (this.#destroyed || this.#fd === null) {
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
      // Note: #close() will be called by _destroy() when autoClose is true
      return;
    }

    const bytesToRead = MathMin(size, remaining);
    const chunk = this.#content.subarray(this.#pos, this.#pos + bytesToRead);
    this.#pos += bytesToRead;

    this.push(chunk);

    // Check if we've reached the end
    if (this.#pos >= endPos || this.#pos >= this.#content.length) {
      this.push(null);
      // Note: #close() will be called by _destroy() when autoClose is true
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
    this.#destroyed = true;
    if (this.#autoClose) {
      this.#close();
    }
    callback(err);
  }
}

module.exports = {
  VirtualReadStream,
};
