'use strict';

const {
  SafeMap,
  StringPrototypeStartsWith,
  Symbol,
  SymbolAsyncIterator,
  ObjectCreate,
} = primordials;

const { Buffer } = require('buffer');
const {
  codes: {
    ERR_FEATURE_UNAVAILABLE_ON_PLATFORM,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');
const { URL } = require('internal/url');
const { kEmptyObject } = require('internal/util');
const { validateObject, validateBoolean, validateAbortSignal, validateBuffer } = require('internal/validators');
const { EventEmitter, once } = require('events');
const path = require('path');

const kFSWatchStart = Symbol('kFSWatchStart');

let internalSync;
let internalPromises;

function lazyLoadFsPromises() {
  internalPromises ??= require('fs/promises');
  return internalPromises;
}

function lazyLoadFsSync() {
  internalSync ??= require('fs');
  return internalSync;
}

async function traverse(dir, files = new SafeMap()) {
  const { readdir } = lazyLoadFsPromises();

  const filenames = await readdir(dir, { withFileTypes: true });

  for await (const file of filenames) {
    const f = path.join(dir, file.name);

    files.set(f, file);

    if (file.isDirectory()) {
      await traverse(f, files);
    }
  }

  return files;
}

class FSWatcher extends EventEmitter {
  #options = null;
  #closed = false;
  #files = new SafeMap();
  #rootPath = path.resolve();

  constructor(options = kEmptyObject) {
    super();

    validateObject(options, 'options');

    if (options.persistent != null) {
      validateBoolean(options.persistent, 'options.persistent');
    }

    if (options.recursive != null) {
      validateBoolean(options.recursive, 'options.recursive');
    }

    if (options.signal != null) {
      validateAbortSignal(options.signal, 'options.signal');
    }

    if (options.encoding != null) {
      // This is required since on macOS and Windows it throws ERR_INVALID_ARG_VALUE
      if (typeof options.encoding !== 'string') {
        throw new ERR_INVALID_ARG_VALUE('encoding', 'options.encoding');
      }
    }

    this.#options = options;
  }

  close() {
    if (this.#closed) {
      return;
    }

    const { unwatchFile } = lazyLoadFsSync();
    this.#closed = true;

    for (const file of this.#files.keys()) {
      unwatchFile(file);
    }

    this.#files.clear();
    this.emit('close');
  }

  /**
   * @param {string} file
   * @return {string}
   */
  #getPath(file) {
    const root = this.#files.get(this.#rootPath);

    // When watching files the path is always the root.
    if (root.isFile()) {
      return this.#rootPath.slice(this.#rootPath.lastIndexOf('/') + 1);
    }

    if (file === this.#rootPath) {
      return this.#rootPath;
    }

    return path.relative(this.#rootPath, file);
  }

  #unwatchFolder(file) {
    const { unwatchFile } = lazyLoadFsSync();

    for (const filename of this.#files.keys()) {
      if (StringPrototypeStartsWith(filename, file)) {
        unwatchFile(filename);
      }
    }
  }

  async #watchFolder(folder) {
    const { opendir } = lazyLoadFsPromises();

    try {
      const files = await opendir(folder);

      for await (const file of files) {
        const f = path.join(folder, file.name);

        if (this.#closed) {
          break;
        }

        if (!this.#files.has(f)) {
          this.#files.set(f, file);
          this.emit('change', 'rename', this.#getPath(f));

          if (file.isDirectory()) {
            await this.#watchFolder(f);
          } else {
            this.#watchFile(f);
          }
        }
      }
    } catch (error) {
      this.emit('error', error);
    }
  }

  #watchFile(file) {
    const { watchFile } = lazyLoadFsSync();

    if (this.#closed) {
      return;
    }

    const existingStat = this.#files.get(file);

    watchFile(file, {
      persistent: this.#options.persistent,
    }, (statWatcher, previousStatWatcher) => {
      if (existingStat && !existingStat.isDirectory() &&
        statWatcher.nlink !== 0 && existingStat.mtime?.getTime() === statWatcher.mtime?.getTime()) {
        return;
      }

      this.#files.set(file, statWatcher);

      if (statWatcher.birthtimeMs === 0 && previousStatWatcher.birthtimeMs !== 0) {
        // The file is now deleted
        this.#files.delete(file);
        this.emit('change', 'rename', this.#getPath(file));

        if (statWatcher.isDirectory()) {
          this.#unwatchFolder(file);
        }
      } else {
        this.emit('change', 'change', this.#getPath(file));

        if (statWatcher.isDirectory()) {
          this.#watchFolder(file);
        }
      }
    });
  }

  [kFSWatchStart](filename) {
    if (typeof filename !== 'string' && !(filename instanceof Buffer) && !(filename instanceof URL)) {
      throw new ERR_INVALID_ARG_TYPE('filename', 'string', filename);
    }

    if (typeof filename === 'string') {
      if (filename.length === 0) {
        throw new ERR_INVALID_ARG_TYPE('filename', 'string', filename);
      }
    } else if (Buffer.isBuffer(filename)) {
      validateBuffer(filename, 'filename');

      if (filename.length === 0) {
        throw new ERR_INVALID_ARG_TYPE('filename', 'buffer', filename);
      }
    }

    try {
      const file = lazyLoadFsSync().statSync(filename);

      this.#rootPath = filename;
      this.#closed = false;
      this.#files.set(filename, file);

      if (file.isDirectory()) {
        traverse(filename, this.#files)
          .then(() => {
            for (const f of this.#files.keys()) {
              this.#watchFile(f);
            }
          });
      } else {
        this.#watchFile(filename);
      }
    } catch (error) {
      if (error.code === 'ENOENT') {
        error.filename = filename;
        throw error;
      }
    }

  }

  ref() {
    // This is kept to have the same API with FSWatcher
    throw new ERR_FEATURE_UNAVAILABLE_ON_PLATFORM('ref');
  }

  unref() {
    // This is kept to have the same API with FSWatcher
    throw new ERR_FEATURE_UNAVAILABLE_ON_PLATFORM('unref');
  }

  [SymbolAsyncIterator]() {
    const watcher = this;
    return ObjectCreate({
      __proto__: null,
      async next() {
        if (watcher.#closed) {
          return { __proto__: null, done: true };
        }
        const { 0: eventType, 1: filename } = await once(watcher, 'change');
        return {
          __proto__: null,
          value: { __proto__: null, eventType, filename },
          done: false,
        };
      },
      [SymbolAsyncIterator]() { return this; },
    });
  }
}

module.exports = {
  FSWatcher,
  kFSWatchStart,
};
