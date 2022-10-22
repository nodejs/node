'use strict';

const { EventEmitter } = require('events');
const path = require('path');
const { SafeMap, Symbol, StringPrototypeStartsWith } = primordials;
const { validateObject } = require('internal/validators');
const { kEmptyObject } = require('internal/util');
const { ERR_FEATURE_UNAVAILABLE_ON_PLATFORM } = require('internal/errors');
const { Dirent, UV_DIRENT_DIR } = require('internal/fs/utils');

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

function traverse(dir, files = new SafeMap()) {
  const { readdirSync } = lazyLoadFsSync();

  const filenames = readdirSync(dir, { withFileTypes: true });

  files.set(dir, new Dirent(dir, UV_DIRENT_DIR));

  for (const file of filenames) {
    const f = path.join(dir, file.name);

    files.set(f, file);

    if (file.isDirectory()) {
      traverse(f, files);
    }
  }

  return files;
}

class FSWatcher extends EventEmitter {
  #options = null;
  #closed = false;
  #files = new SafeMap();
  #rootPath = path.resolve();

  /**
   * @param {{
   *   persistent?: boolean;
   *   recursive?: boolean;
   *   encoding?: string;
   *   signal?: AbortSignal;
   *   }} [options]
   */
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

  #getPath(file) {
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
    const { opendir, stat } = lazyLoadFsPromises();

    try {
      const files = await opendir(folder);

      for await (const file of files) {
        const f = path.join(folder, file.name);

        if (this.#closed) {
          break;
        }

        if (!this.#files.has(f)) {
          const fileStats = await stat(f);

          this.#files.set(f, fileStats);
          this.emit('change', 'rename', this.#getPath(f));

          if (fileStats.isDirectory()) {
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

  /**
   * @param {string} file
   */
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
      } else if (statWatcher.isDirectory()) {
        this.#watchFolder(file);
        this.emit('change', 'change', this.#getPath(file));
      } else {
        this.emit('change', 'change', this.#getPath(file));
      }
    });
  }

  /**
   * @param {string | Buffer | URL} filename
   */
  [kFSWatchStart](filename) {
    this.#rootPath = filename;
    this.#closed = false;
    this.#files = traverse(filename);

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
}

module.exports = {
  FSWatcher,
  kFSWatchStart,
};
