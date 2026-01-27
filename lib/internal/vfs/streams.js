'use strict';

const {
  MathMin,
} = primordials;

const { Readable } = require('stream');
const { createEBADF } = require('internal/vfs/errors');

/**
 * A readable stream for virtual files.
 */
class VirtualReadStream extends Readable {
  /**
   * @param {VirtualFileSystem} vfs The VFS instance
   * @param {string} filePath The path to the file
   * @param {object} [options] Stream options
   */
  constructor(vfs, filePath, options = {}) {
    const {
      start = 0,
      end = Infinity,
      highWaterMark = 64 * 1024,
      encoding,
      ...streamOptions
    } = options;

    super({ ...streamOptions, highWaterMark, encoding });

    this._vfs = vfs;
    this._path = filePath;
    this._fd = null;
    this._start = start;
    this._end = end;
    this._pos = start;
    this._content = null;
    this._destroyed = false;
    this._autoClose = options.autoClose !== false;

    // Open the file on next tick so listeners can be attached
    process.nextTick(() => this._openFile());
  }

  /**
   * Gets the file path.
   * @returns {string}
   */
  get path() {
    return this._path;
  }

  /**
   * Opens the virtual file.
   * Events are emitted synchronously within this method, which runs
   * asynchronously via process.nextTick - matching real fs behavior.
   * @private
   */
  _openFile() {
    try {
      this._fd = this._vfs.openSync(this._path);
      this.emit('open', this._fd);
      this.emit('ready');
    } catch (err) {
      this.destroy(err);
    }
  }

  /**
   * Implements the readable _read method.
   * @param {number} size Number of bytes to read
   * @private
   */
  _read(size) {
    if (this._destroyed || this._fd === null) {
      return;
    }

    // Load content on first read (lazy loading)
    if (this._content === null) {
      try {
        const vfd = require('internal/vfs/fd').getVirtualFd(this._fd);
        if (!vfd) {
          this.destroy(createEBADF('read'));
          return;
        }
        // Use the file handle's readFileSync to get content
        this._content = vfd.entry.readFileSync();
      } catch (err) {
        this.destroy(err);
        return;
      }
    }

    // Calculate how much to read
    // Note: end is inclusive, so we use end + 1 for the upper bound
    const endPos = this._end === Infinity ? this._content.length : this._end + 1;
    const remaining = MathMin(endPos, this._content.length) - this._pos;
    if (remaining <= 0) {
      this.push(null);
      // Note: _close() will be called by _destroy() when autoClose is true
      return;
    }

    const bytesToRead = MathMin(size, remaining);
    const chunk = this._content.subarray(this._pos, this._pos + bytesToRead);
    this._pos += bytesToRead;

    this.push(chunk);

    // Check if we've reached the end
    if (this._pos >= endPos || this._pos >= this._content.length) {
      this.push(null);
      // Note: _close() will be called by _destroy() when autoClose is true
    }
  }

  /**
   * Closes the file descriptor.
   * Note: Does not emit 'close' - the base Readable class handles that.
   * @private
   */
  _close() {
    if (this._fd !== null) {
      try {
        this._vfs.closeSync(this._fd);
      } catch {
        // Ignore close errors
      }
      this._fd = null;
    }
  }

  /**
   * Implements the readable _destroy method.
   * @param {Error|null} err The error
   * @param {Function} callback Callback
   * @private
   */
  _destroy(err, callback) {
    this._destroyed = true;
    if (this._autoClose) {
      this._close();
    }
    callback(err);
  }
}

/**
 * Creates a readable stream for a virtual file.
 * @param {VirtualFileSystem} vfs The VFS instance
 * @param {string} filePath The path to the file
 * @param {object} [options] Stream options
 * @returns {VirtualReadStream}
 */
function createVirtualReadStream(vfs, filePath, options = {}) {
  return new VirtualReadStream(vfs, filePath, options);
}

module.exports = {
  createVirtualReadStream,
  VirtualReadStream,
};
