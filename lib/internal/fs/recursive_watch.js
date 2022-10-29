'use strict';

const {
  ArrayPrototypePush,
  SafePromiseAllReturnVoid,
  Promise,
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
  AbortError,
  codes: {
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');
const { getValidatedPath } = require('internal/fs/utils');
const { kFSWatchStart, StatWatcher } = require('internal/fs/watchers');
const { kEmptyObject } = require('internal/util');
const { validateBoolean, validateAbortSignal } = require('internal/validators');
const path = require('path');

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

    if (file.isSymbolicLink()) {
      // Do not follow symbolic links
    } else if (file.isDirectory()) {
      ArrayPrototypePush(subdirectories, traverse(f, files));
    }
  }

  await SafePromiseAllReturnVoid(subdirectories);

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

    assert(typeof options === 'object');

    const { persistent, recursive, signal, encoding } = options;

    // TODO(anonrig): Add non-recursive support to non-native-watcher for IBMi & AIX support.
    if (recursive != null) {
      validateBoolean(recursive, 'options.recursive');
    }

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

  #unwatchFiles(file) {
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
          this.emit('change', 'rename', this.#getPath(f));

          if (file.isFile()) {
            this.#watchFile(f);
          } else {
            this.#files.set(f, file);

            if (file.isDirectory() && !file.isSymbolicLink()) {
              await this.#watchFolder(f);
            }
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
        this.#unwatchFiles(file);
      } else {
        this.emit('change', 'change', this.#getPath(file));

        if (statWatcher.isDirectory() && !statWatcher.isSymbolicLink()) {
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
      this.#watchingFile = file.isFile();

      if (file.isDirectory()) {
        this.#files.set(filename, file);

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
    this.#files.forEach((file) => {
      if (file instanceof StatWatcher) {
        file.ref();
      }
    });
  }

  unref() {
    this.#files.forEach((file) => {
      if (file instanceof StatWatcher) {
        file.unref();
      }
    });
  }

  [SymbolAsyncIterator]() {
    const { signal } = this.#options;
    const promiseExecutor = signal == null ?
      (resolve) => {
        this.once('change', (eventType, filename) => {
          resolve({ __proto__: null, value: { eventType, filename } });
        });
      } : (resolve, reject) => {
        const onAbort = () => reject(new AbortError(undefined, { cause: signal.reason }));
        if (signal.aborted) return onAbort();
        signal.addEventListener('abort', onAbort, { __proto__: null, once: true });
        this.once('change', (eventType, filename) => {
          signal.removeEventListener('abort', onAbort);
          resolve({ __proto__: null, value: { eventType, filename } });
        });
      };
    return {
      next: () => (this.#closed ?
        { __proto__: null, done: true } :
        new Promise(promiseExecutor)),
      [SymbolAsyncIterator]() { return this; },
    };
  }
}

module.exports = {
  FSWatcher,
  kFSWatchStart,
};
