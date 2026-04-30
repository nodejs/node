'use strict';

const {
  SymbolAsyncIterator,
  SymbolAsyncDispose,
  SymbolDispose,
} = primordials;

const {
  codes: {
    ERR_DIR_CLOSED,
  },
} = require('internal/errors');

/**
 * Virtual directory handle returned by VFS opendir/opendirSync.
 * Mimics the subset of the native Dir interface used by Node.js internals
 * (e.g. fs.cp, fs.promises.cp).
 */
class VirtualDir {
  #path;
  #entries;
  #index;
  #closed;

  constructor(dirPath, entries) {
    this.#path = dirPath;
    this.#entries = entries;
    this.#index = 0;
    this.#closed = false;
  }

  get path() {
    return this.#path;
  }

  readSync() {
    if (this.#closed) {
      throw new ERR_DIR_CLOSED();
    }
    if (this.#index >= this.#entries.length) {
      return null;
    }
    return this.#entries[this.#index++];
  }

  async read(callback) {
    if (typeof callback === 'function') {
      try {
        const result = this.readSync();
        process.nextTick(callback, null, result);
      } catch (err) {
        process.nextTick(callback, err);
      }
      return;
    }
    return this.readSync();
  }

  closeSync() {
    if (this.#closed) {
      throw new ERR_DIR_CLOSED();
    }
    this.#closed = true;
  }

  async close(callback) {
    if (typeof callback === 'function') {
      this.closeSync();
      process.nextTick(callback, null);
      return;
    }
    this.closeSync();
  }

  async *entries() {
    if (this.#closed) {
      throw new ERR_DIR_CLOSED();
    }
    try {
      let entry;
      while ((entry = this.readSync()) !== null) {
        yield entry;
      }
    } finally {
      if (!this.#closed) {
        this.closeSync();
      }
    }
  }

  [SymbolAsyncIterator]() {
    return this.entries();
  }

  [SymbolAsyncDispose]() {
    return this.close();
  }

  [SymbolDispose]() {
    if (!this.#closed) {
      this.closeSync();
    }
  }
}

module.exports = {
  VirtualDir,
};
