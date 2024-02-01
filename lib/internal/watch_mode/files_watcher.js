'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeForEach,
  Boolean,
  SafeMap,
  SafeSet,
  SafeWeakMap,
  StringPrototypeStartsWith,
} = primordials;

const { validateNumber, validateOneOf } = require('internal/validators');
const { kEmptyObject } = require('internal/util');
const { TIMEOUT_MAX } = require('internal/timers');

const EventEmitter = require('events');
const { addAbortListener } = require('internal/events/abort_listener');
const { watch, existsSync } = require('fs');
const { fileURLToPath } = require('internal/url');
const { resolve, dirname } = require('path');
const { setTimeout, clearTimeout, setInterval, clearInterval } = require('timers');

const supportsRecursiveWatching = process.platform === 'win32' ||
  process.platform === 'darwin';

class FilesWatcher extends EventEmitter {
  #watchers = new SafeMap();
  #filteredFiles = new SafeSet();
  #debouncing = new SafeSet();
  #depencencyOwners = new SafeMap();
  #ownerDependencies = new SafeMap();
  #debounce;
  #renameInterval;
  #renameTimeout;
  #mode;
  #signal;
  #passthroughIPC = false;
  #ipcHandlers = new SafeWeakMap();

  constructor({
    debounce = 200,
    mode = 'filter',
    renameInterval = 1000,
    renameTimeout = 60_000,
    signal,
  } = kEmptyObject) {
    super({ __proto__: null, captureRejections: true });

    validateNumber(debounce, 'options.debounce', 0, TIMEOUT_MAX);
    validateOneOf(mode, 'options.mode', ['filter', 'all']);
    this.#debounce = debounce;
    this.#mode = mode;
    this.#renameInterval = renameInterval;
    this.#renameTimeout = renameTimeout;
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
      if (watcher.recursive && StringPrototypeStartsWith(path, watchedPath)) {
        return true;
      }
    }

    return false;
  }

  #removeWatchedChildren(path) {
    for (const { 0: watchedPath, 1: watcher } of this.#watchers.entries()) {
      if (path !== watchedPath && StringPrototypeStartsWith(watchedPath, path)) {
        this.#unwatch(watcher);
        this.#watchers.delete(watchedPath);
      }
    }
  }

  #unwatch(watcher) {
    watcher.handle.removeAllListeners();
    watcher.handle.close();
  }

  #onChange(eventType, trigger, recursive) {
    if (eventType === 'rename' && !recursive) {
      return this.#rewatch(trigger);
    }
    if (this.#debouncing.has(trigger)) {
      return;
    }
    if (this.#mode === 'filter' && !this.#filteredFiles.has(trigger)) {
      return;
    }
    this.#debouncing.add(trigger);
    const owners = this.#depencencyOwners.get(trigger);
    setTimeout(() => {
      this.#debouncing.delete(trigger);
      this.emit('changed', { owners });
    }, this.#debounce).unref();
  }

  // When a file is removed, wait for it to be re-added.
  // Often this re-add is immediate - some editors (e.g., gedit) and some docker mount modes do this.
  #rewatch(path) {
    if (this.#isPathWatched(path)) {
      this.#unwatch(this.#watchers.get(path));
      this.#watchers.delete(path);
      if (existsSync(path)) {
        this.watchPath(path, false);
        // This might be redundant. If the file was re-added due to a save event, we will probably see change -> rename.
        // However, in certain situations it's entirely possible for the content to have changed after the rename
        // In these situations we'd miss the change after the rename event
        this.#onChange('change', path, false);
        return;
      }
      let timeout;

      // Wait for the file to exist - check every `renameInterval` ms
      const interval = setInterval(async () => {
        if (existsSync(path)) {
          clearInterval(interval);
          clearTimeout(timeout);
          this.watchPath(path, false);
          this.#onChange('change', path, false);
        }
      }, this.#renameInterval).unref();

      // Don't wait forever - after `renameTimeout` ms, stop trying
      timeout = setTimeout(() => {
        clearInterval(interval);
      }, this.#renameTimeout).unref();
    }
  }

  get watchedPaths() {
    return [...this.#watchers.keys()];
  }

  watchPath(path, recursive = true) {
    if (this.#isPathWatched(path)) {
      return;
    }
    const watcher = watch(path, { recursive, signal: this.#signal });
    watcher.on('change', (eventType, fileName) => {
      // `fileName` can be `null` if it cannot be determined. See
      // https://github.com/nodejs/node/pull/49891#issuecomment-1744673430.
      this.#onChange(eventType, recursive ? resolve(path, fileName) : path, recursive);
    });
    this.#watchers.set(path, { handle: watcher, recursive });
    if (recursive) {
      this.#removeWatchedChildren(path);
    }
  }

  filterFile(file, owner) {
    if (!file) return;
    if (supportsRecursiveWatching) {
      this.watchPath(dirname(file));
    } else {
      // Having multiple FSWatcher's seems to be slower
      // than a single recursive FSWatcher
      this.watchPath(file, false);
    }
    this.#filteredFiles.add(file);
    if (owner) {
      const owners = this.#depencencyOwners.get(file) ?? new SafeSet();
      const dependencies = this.#ownerDependencies.get(file) ?? new SafeSet();
      owners.add(owner);
      dependencies.add(file);
      this.#depencencyOwners.set(file, owners);
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
    if (this.#mode !== 'filter') {
      return;
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
        this.#depencencyOwners.delete(dependency);
      });
      this.#filteredFiles.delete(owner);
      this.#depencencyOwners.delete(owner);
      this.#ownerDependencies.delete(owner);
    });
  }
  clearFileFilters() {
    this.#filteredFiles.clear();
  }
  clear() {
    this.#watchers.forEach(this.#unwatch);
    this.#watchers.clear();
    this.#filteredFiles.clear();
    this.#depencencyOwners.clear();
    this.#ownerDependencies.clear();
  }
}

module.exports = { FilesWatcher };
