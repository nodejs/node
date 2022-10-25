'use strict';

const {
  ArrayPrototypePush,
  SafePromiseAll,
  PromisePrototypeThen,
  SafeMap,
  StringPrototypeStartsWith,
  StringPrototypeSlice,
  StringPrototypeLastIndexOf,
  SymbolAsyncIterator,
} = primordials;

const { EventEmitter } = require('events');
const assert = require('internal/assert');
const {
  codes: {
    ERR_FEATURE_UNAVAILABLE_ON_PLATFORM,
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');
const { getValidatedPath } = require('internal/fs/utils');
const { kFSWatchStart } = require('internal/fs/watchers');
const { kEmptyObject } = require('internal/util');
const { validateBoolean, validateAbortSignal } = require('internal/validators');
const path = require('path');
const { Readable } = require('stream');

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
  const { opendir } = lazyLoadFsPromises();

  const filenames = await opendir(dir);
  const subdirectories = [];

  for await (const file of filenames) {
    const f = path.join(dir, file.name);

    files.set(f, file);

    if (file.isDirectory()) {
      ArrayPrototypePush(subdirectories, traverse(f, files));
    }
  }

  await SafePromiseAll(subdirectories);

  return files;
}

class FSWatcher extends EventEmitter {
  #options = null;
  #closed = false;
  #files = new SafeMap();
  #rootPath = path.resolve();
  #watchingFile = false;

  constructor(options = kEmptyObject) {
    super();

    assert(typeof options === 'object' && options.recursive === true);

    const { persistent, recursive, signal, encoding } = options;

    if (persistent != null) {
      validateBoolean(persistent, 'options.persistent');
    }

    if (signal != null) {
      validateAbortSignal(signal, 'options.signal');
    }

    if (encoding != null) {
      // This is required since on macOS and Windows it throws ERR_INVALID_ARG_VALUE
      if (typeof encoding !== 'string') {
        throw new ERR_INVALID_ARG_VALUE(encoding, 'options.encoding');
      }
    }


    this.#options = { persistent, recursive, signal, encoding };
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
   * @return {string} the full path of `file` if we're watching a directory, or the
                      filename when watching a file.
   */
  #getPath(file) {
    // Always return the filename instead of the full path if watching a file (not a directory)
    if (this.#watchingFile) {
      return StringPrototypeSlice(this.#rootPath, StringPrototypeLastIndexOf(this.#rootPath, '/') + 1);
    }

    // When watching directories, and root directory is changed, return the full file path
    if (file === this.#rootPath) {
      return this.#rootPath;
    }

    // Otherwise return relative path for all files & folders
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
        if (this.#closed) {
          break;
        }

        const f = path.join(folder, file.name);

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
    if (this.#closed) {
      return;
    }

    const { watchFile } = lazyLoadFsSync();
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
    filename = path.resolve(getValidatedPath(filename));

    try {
      const file = lazyLoadFsSync().statSync(filename);

      this.#rootPath = filename;
      this.#closed = false;
      this.#files.set(filename, file);
      this.#watchingFile = file.isFile();

      if (file.isDirectory()) {
        PromisePrototypeThen(
          traverse(filename, this.#files),
          () => {
            for (const f of this.#files.keys()) {
              this.#watchFile(f);
            }
          },
        );
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
    const self = this;
    const stream = new Readable({
      destroy(error, callback) {
        self.close();
        callback(error);
      },
      read() { },
      autoDestroy: true,
      objectMode: true,
      signal: this.#options.signal,
    });

    this.on('change', (eventType, filename) => {
      stream.push({ eventType, filename });
    });

    return stream;
  }
}

module.exports = {
  FSWatcher,
  kFSWatchStart,
};
