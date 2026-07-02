'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeForEach,
  Boolean,
  SafeMap,
  SafeSet,
  SafeWeakMap,
  StringPrototypeEndsWith,
  StringPrototypeStartsWith,
} = primordials;

const { validateNumber, validateOneOf } = require('internal/validators');
const { kEmptyObject } = require('internal/util');
const { TIMEOUT_MAX } = require('internal/timers');

const EventEmitter = require('events');
const { addAbortListener } = require('internal/events/abort_listener');
const { watch } = require('fs');
const { fileURLToPath } = require('internal/url');
const { resolve, dirname, sep } = require('path');
const { setTimeout, clearTimeout } = require('timers');

const supportsRecursiveWatching = process.platform === 'win32' ||
  process.platform === 'darwin';

const isParentPath = (parentCandidate, childCandidate) => {
  const parent = resolve(parentCandidate);
  const child = resolve(childCandidate);
  const normalizedParent = StringPrototypeEndsWith(parent, sep) ? parent : parent + sep;
  return StringPrototypeStartsWith(child, normalizedParent);
};

class FilesWatcher extends EventEmitter {
  #watchers = new SafeMap();
  #filteredFiles = new SafeSet();
  #dependencyOwners = new SafeMap();
  #ownerDependencies = new SafeMap();
  #debounceOwners = new SafeSet();
  #debounceTimer;
  #debounceTrigger;
  #debounce;
  #mode;
  #signal;
  #passthroughIPC = false;
  #ipcHandlers = new SafeWeakMap();

  constructor({ debounce = 200, mode = 'filter', signal } = kEmptyObject) {
    super({ __proto__: null, captureRejections: true });

    validateNumber(debounce, 'options.debounce', 0, TIMEOUT_MAX);
    validateOneOf(mode, 'options.mode', ['filter', 'all']);
    this.#debounce = debounce;
    this.#mode = mode;
    this.#signal = signal;
    this.#passthroughIPC = Boolean(process.send);

    if (signal) {
      addAbortListener(signal, () => this.clear());
    }
  }

  #isPathWatched(path) {
    if (this.#watchers.has(path)) {
      return true;
    }

    for (const { 0: watchedPath, 1: watcher } of this.#watchers.entries()) {
      if (watcher.recursive && isParentPath(watchedPath, path)) {
        return true;
      }
    }

    return false;
  }

  #removeWatchedChildren(path) {
    for (const { 0: watchedPath, 1: watcher } of this.#watchers.entries()) {
      if (path !== watchedPath && isParentPath(path, watchedPath)) {
        this.#unwatch(watcher);
        this.#watchers.delete(watchedPath);
      }
    }
  }

  #unwatch(watcher) {
    watcher.handle.removeAllListeners();
    watcher.handle.close();
  }

  #onChange(trigger, eventType) {
    if (this.#mode === 'filter' && !this.#filteredFiles.has(trigger)) {
      return;
    }
    const owners = this.#dependencyOwners.get(trigger);
    if (owners) {
      for (const owner of owners) {
        this.#debounceOwners.add(owner);
      }
    }
    this.#debounceTrigger = trigger;
    clearTimeout(this.#debounceTimer);
    this.#debounceTimer = setTimeout(() => {
      this.#debounceTimer = null;
      this.emit('changed', { owners: this.#debounceOwners, eventType, trigger: this.#debounceTrigger });
      this.#debounceOwners.clear();
      this.#debounceTrigger = undefined;
    }, this.#debounce).unref();
  }

  get watchedPaths() {
    return [...this.#watchers.keys()];
  }

  watchPath(path, recursive = true, options = kEmptyObject) {
    if (this.#isPathWatched(path)) {
      return;
    }
    const { allowMissing = false } = options;

    const watcher = watch(path, { recursive, signal: this.#signal, throwIfNoEntry: !allowMissing });
    watcher.on('change', (eventType, fileName) => {
      // `fileName` can be `null` if it cannot be determined. See
      // https://github.com/nodejs/node/pull/49891#issuecomment-1744673430.
      // `path` is the watched directory (see `filterFile`), so resolve the
      // changed entry against it to get the absolute path of the trigger.
      this.#onChange(resolve(path, fileName ?? ''), eventType);
    });
    this.#watchers.set(path, { handle: watcher, recursive });
    if (recursive) {
      this.#removeWatchedChildren(path);
    }
  }

  filterFile(file, owner, options = kEmptyObject) {
    if (!file) return;
    if (supportsRecursiveWatching) {
      this.watchPath(dirname(file), true, options);
    } else {
      // Watch the parent directory non-recursively rather than the file
      // itself. A watch bound to the file's inode stops receiving events once
      // the file is replaced via unlink+create or rename (atomic saves, Docker
      // Compose watch, ...), so only the first replacement would be detected.
      // Watching the directory keeps working across replacements, and unrelated
      // siblings are discarded by the `filter` mode check in `#onChange`.
      this.watchPath(dirname(file), false, options);
    }
    this.#filteredFiles.add(file);
    if (owner) {
      const owners = this.#dependencyOwners.get(file) ?? new SafeSet();
      const dependencies = this.#ownerDependencies.get(file) ?? new SafeSet();
      owners.add(owner);
      dependencies.add(file);
      this.#dependencyOwners.set(file, owners);
      this.#ownerDependencies.set(owner, dependencies);
    }
  }


  #setupIPC(child) {
    const handlers = {
      __proto__: null,
      parentToChild: (message) => child.send(message),
      childToParent: (message) => process.send(message),
    };
    this.#ipcHandlers.set(child, handlers);
    process.on('message', handlers.parentToChild);
    child.on('message', handlers.childToParent);
  }

  destroyIPC(child) {
    const handlers = this.#ipcHandlers.get(child);
    if (this.#passthroughIPC && handlers !== undefined) {
      process.off('message', handlers.parentToChild);
      child.off('message', handlers.childToParent);
    }
  }

  watchChildProcessModules(child, key = null) {
    if (this.#passthroughIPC) {
      this.#setupIPC(child);
    }

    child.on('message', (message) => {
      try {
        if (ArrayIsArray(message['watch:require'])) {
          ArrayPrototypeForEach(message['watch:require'], (file) => this.filterFile(file, key));
        }
        if (ArrayIsArray(message['watch:import'])) {
          ArrayPrototypeForEach(message['watch:import'], (file) => this.filterFile(fileURLToPath(file), key));
        }
      } catch {
        // Failed watching file. ignore
      }
    });
  }
  unfilterFilesOwnedBy(owners) {
    owners.forEach((owner) => {
      this.#ownerDependencies.get(owner)?.forEach((dependency) => {
        this.#filteredFiles.delete(dependency);
        this.#dependencyOwners.get(dependency)?.delete(owner);
        if (this.#dependencyOwners.get(dependency)?.size === 0) {
          this.#dependencyOwners.delete(dependency);
        }
      });
      this.#filteredFiles.delete(owner);
      this.#dependencyOwners.delete(owner);
      this.#ownerDependencies.delete(owner);
    });
  }
  clearFileFilters() {
    this.#filteredFiles.clear();
  }
  clear() {
    clearTimeout(this.#debounceTimer);
    this.#debounceTimer = null;
    this.#debounceOwners.clear();
    this.#debounceTrigger = undefined;
    this.#watchers.forEach(this.#unwatch);
    this.#watchers.clear();
    this.#filteredFiles.clear();
    this.#dependencyOwners.clear();
    this.#ownerDependencies.clear();
  }
}

module.exports = { FilesWatcher };
