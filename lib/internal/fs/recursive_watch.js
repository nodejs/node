'use strict';

const {
  Promise,
  SafeMap,
  SafeSet,
  StringPrototypeStartsWith,
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
const { createIgnoreMatcher, kFSWatchStart } = require('internal/fs/watchers');
const { kEmptyObject } = require('internal/util');
const { validateBoolean, validateAbortSignal, validateIgnoreOption } = require('internal/validators');
const {
  basename: pathBasename,
  join: pathJoin,
  relative: pathRelative,
  resolve: pathResolve,
  sep: pathSep,
} = require('path');

let internalSync;

function lazyLoadFsSync() {
  internalSync ??= require('fs');
  return internalSync;
}

let kResistStopPropagation;

// Inotify reports changes to a directory's entries, with their names, on the
// directory's own watch, so one watcher per directory is enough on Linux.
// kqueue and event ports only report that the directory itself changed, so
// elsewhere every file keeps a watcher of its own as well.
const kDirectoryWatchReportsEntries = process.platform === 'linux';

class FSWatcher extends EventEmitter {
  #options = null;
  #closed = false;
  // Every path below the root that has been reported (or existed at start).
  #entries = new SafeSet();
  // One fs.watch() per directory and symbolic link (and per file where the
  // directory watch does not report its entries).
  #watchers = new SafeMap();
  #symbolicLinks = new SafeSet();
  #rootPath = pathResolve();
  #ignoreMatcher = null;

  constructor(options = kEmptyObject) {
    super();

    assert(typeof options === 'object');

    const { persistent, recursive, signal, encoding, ignore } = options;
    let { throwIfNoEntry } = options;

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

    if (throwIfNoEntry != null) {
      validateBoolean(throwIfNoEntry, 'options.throwIfNoEntry');
    } else {
      throwIfNoEntry = true;
    }

    if (encoding != null) {
      // This is required since on macOS and Windows it throws ERR_INVALID_ARG_VALUE
      if (typeof encoding !== 'string') {
        throw new ERR_INVALID_ARG_VALUE('options.encoding', encoding);
      }
    }

    validateIgnoreOption(ignore, 'options.ignore');
    this.#ignoreMatcher = createIgnoreMatcher(ignore);

    this.#options = { persistent, recursive, signal, encoding, throwIfNoEntry };
  }

  close() {
    if (this.#closed) {
      return;
    }

    this.#closed = true;

    for (const watcher of this.#watchers.values()) {
      watcher.close();
    }
    this.#watchers.clear();
    this.#entries.clear();
    this.#symbolicLinks.clear();
    this.emit('close');
  }

  #emit(eventType, file) {
    this.emit('change', eventType, pathRelative(this.#rootPath, file));
  }

  #forget(file) {
    const childPrefix = file + pathSep;
    for (const entry of this.#entries) {
      if (entry === file || StringPrototypeStartsWith(entry, childPrefix)) {
        this.#entries.delete(entry);
        this.#symbolicLinks.delete(entry);
        const watcher = this.#watchers.get(entry);
        if (watcher !== undefined) {
          watcher.close();
          this.#watchers.delete(entry);
        }
      }
    }
  }

  // An entry that vanished between being listed and being watched is left to
  // the directory's own watcher to report.
  #watch(file, onChange) {
    if (this.#closed || this.#watchers.has(file)) {
      return;
    }
    const { watch } = lazyLoadFsSync();
    let watcher;
    try {
      watcher = watch(file, { persistent: this.#options.persistent }, onChange);
    } catch (err) {
      if (err.code === 'ENOENT') {
        return;
      }
      throw err;
    }
    this.#watchers.set(file, watcher);
  }

  // Registers the entries of `folder` that are not known yet (emitting
  // 'rename' for them unless this is the initial scan) and arms one watcher
  // for the directory; #addEntry() descends into subdirectories.
  #scanFolder(folder, initial) {
    const { readdirSync } = lazyLoadFsSync();
    let entries;
    try {
      entries = readdirSync(folder, { withFileTypes: true });
    } catch (error) {
      if (error.code !== 'ENOENT') {
        this.emit('error', error);
      }
      return;
    }

    this.#watch(folder, (eventType, filename) => this.#onFolderEvent(folder, filename));

    for (const entry of entries) {
      if (this.#closed) {
        break;
      }
      const file = pathJoin(folder, entry.name);
      if (!this.#entries.has(file) && !this.#ignoreMatcher?.(pathRelative(this.#rootPath, file))) {
        this.#addEntry(file, entry, initial);
      }
    }
  }

  // `entry` is the Dirent or the lstat() Stats of `file`.
  #addEntry(file, entry, initial) {
    this.#entries.add(file);
    if (!initial) {
      this.#emit('rename', file);
    }
    if (entry.isSymbolicLink()) {
      // The link target is watched so that changes behind the link surface
      // as a 'rename' of the link, as they always have on this code path.
      this.#symbolicLinks.add(file);
      this.#watch(file, () => this.#emit('rename', file));
    } else if (entry.isDirectory()) {
      this.#scanFolder(file, initial);
    } else if (!kDirectoryWatchReportsEntries) {
      this.#watch(file, () => this.#onEntryEvent(file));
    }
  }

  #onFolderEvent(folder, filename) {
    if (this.#closed) {
      return;
    }
    const { lstatSync, statSync } = lazyLoadFsSync();
    if (!kDirectoryWatchReportsEntries || filename == null) {
      // All that is known is that something about `folder` changed.
      if (statSync(folder, { throwIfNoEntry: false }) === undefined) {
        this.#emit('rename', folder);
        this.#forget(folder);
      } else {
        this.#scanFolder(folder, false);
      }
      return;
    }
    // Events about the watched directory itself are reported under its own
    // name; those take the "unknown entry" path and are resolved by the parent.
    const file = pathJoin(folder, filename);

    if (!this.#entries.has(file)) {
      if (this.#ignoreMatcher?.(pathRelative(this.#rootPath, file))) {
        return;
      }
      const entry = lstatSync(file, { throwIfNoEntry: false });
      if (entry !== undefined) {
        this.#addEntry(file, entry, false);
      } else if (folder === this.#rootPath && statSync(folder, { throwIfNoEntry: false }) === undefined) {
        this.#emit('rename', folder);
        this.#forget(folder);
      }
      return;
    }

    this.#onEntryEvent(file);
  }

  // Something happened to a known entry: work out what from its current state.
  #onEntryEvent(file) {
    if (this.#closed) {
      return;
    }
    const { statSync } = lazyLoadFsSync();
    const stats = statSync(file, { throwIfNoEntry: false });
    if (stats === undefined) {
      this.#emit('rename', file);
      this.#forget(file);
    } else if (this.#symbolicLinks.has(file)) {
      this.#emit('rename', file);
    } else if (stats.isDirectory()) {
      this.#scanFolder(file, false);
    } else {
      this.#emit('change', file);
    }
  }

  #watchRootFile(file) {
    const { statSync } = lazyLoadFsSync();
    this.#entries.add(file);
    this.#watch(file, () => {
      if (this.#closed) {
        return;
      }
      if (statSync(file, { throwIfNoEntry: false }) === undefined) {
        this.#emit('rename', file);
        this.#forget(file);
      } else {
        this.emit('change', 'change', pathBasename(file));
      }
    });
  }

  [kFSWatchStart](filename) {
    filename = pathResolve(getValidatedPath(filename));

    try {
      const file = lazyLoadFsSync().statSync(filename);

      this.#rootPath = filename;
      this.#closed = false;

      if (file.isDirectory()) {
        this.#scanFolder(filename, true);
      } else {
        this.#watchRootFile(filename);
      }
    } catch (error) {
      if (this.#options.throwIfNoEntry || error.code !== 'ENOENT') {
        error.filename = filename;
        this.close();
        throw error;
      }
    }

  }

  ref() {
    for (const watcher of this.#watchers.values()) {
      watcher.ref();
    }
  }

  unref() {
    for (const watcher of this.#watchers.values()) {
      watcher.unref();
    }
  }

  [SymbolAsyncIterator]() {
    const { signal } = this.#options;
    const promiseExecutor = signal == null ?
      (resolve) => {
        this.once('change', (eventType, filename) => {
          resolve({ __proto__: null, value: { eventType, filename } });
        });
      } : (resolve, reject) => {
        const onAbort = () => {
          this.close();
          reject(new AbortError(undefined, { cause: signal.reason }));
        };
        if (signal.aborted) return onAbort();
        kResistStopPropagation ??= require('internal/event_target').kResistStopPropagation;
        signal.addEventListener('abort', onAbort, { __proto__: null, once: true, [kResistStopPropagation]: true });
        this.once('change', (eventType, filename) => {
          signal.removeEventListener('abort', onAbort);
          resolve({ __proto__: null, value: { eventType, filename } });
        });
      };
    return {
      next: () => (this.#closed ?
        { __proto__: null, done: true } :
        new Promise(promiseExecutor)),
      return: () => {
        this.close();
        return { __proto__: null, done: true };
      },
      [SymbolAsyncIterator]() { return this; },
    };
  }
}

module.exports = {
  FSWatcher,
  kFSWatchStart,
};
