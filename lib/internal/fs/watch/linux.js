'use strict';

const { SafeMap, Symbol, StringPrototypeStartsWith, SymbolAsyncIterator, Promise } = primordials;

const { ERR_FEATURE_UNAVAILABLE_ON_PLATFORM } = require('internal/errors');
const { kEmptyObject } = require('internal/util');
const { validateObject, validateString } = require('internal/validators');
const { EventEmitter } = require('events');
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
    this.#options = options;
  }

  close() {
    const { unwatchFile } = lazyLoadFsSync();
    this.#closed = true;

    for (const file of this.#files.keys()) {
      unwatchFile(file);
    }

    this.emit('close');
  }

  /**
   * @param {string} file
   * @return {string}
   */
  #getPath(file) {
    validateString(file, 'file');

    if (file === this.#rootPath) {
      return this.#rootPath;
    }

    return path.relative(this.#rootPath, file);
  }

  #unwatchFolder(file) {
    validateString(file, 'file');

    const { unwatchFile } = lazyLoadFsSync();

    for (const filename of this.#files.keys()) {
      if (StringPrototypeStartsWith(filename, file)) {
        unwatchFile(filename);
      }
    }
  }

  async #watchFolder(folder) {
    validateString(folder, 'folder');

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
    validateString(file, 'file');

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
      } else if (statWatcher.isDirectory()) {
        this.emit('change', 'change', this.#getPath(file));
        this.#watchFolder(file);
      } else {
        this.emit('change', 'change', this.#getPath(file));
      }
    });
  }

  async [kFSWatchStart](filename) {
    const { stat } = lazyLoadFsPromises();

    this.#rootPath = filename;
    this.#closed = false;
    this.#files = await traverse(filename);
    this.#files.set(filename, await stat(filename));

    for (const f of this.#files.keys()) {
      this.#watchFile(f);
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

  async next() {
    if (this.#closed) {
      return { done: true };
    }

    const result = await new Promise((resolve) => {
      this.once('change', (eventType, filename) => {
        resolve({ eventType, filename });
      });
    });

    return { done: false, value: result };
  }

  // eslint-disable-next-line require-yield
  *[SymbolAsyncIterator]() {
    return this;
  }
}

module.exports = {
  FSWatcher,
  kFSWatchStart,
};
