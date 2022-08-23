'use strict';

const {
  SafeMap,
  SafeSet,
  StringPrototypeStartsWith,
} = primordials;

const { validateNumber, validateOneOf } = require('internal/validators');
const { kEmptyObject } = require('internal/util');
const { TIMEOUT_MAX } = require('internal/timers');

const EventEmitter = require('events');
const { watch } = require('fs');
const { fileURLToPath } = require('url');
const { resolve, dirname } = require('path');
const { setTimeout } = require('timers');


const supportsRecursiveWatching = process.platform === 'win32' ||
  process.platform === 'darwin';

class FilesWatcher extends EventEmitter {
  #watchers = new SafeMap();
  #filteredFiles = new SafeSet();
  #throttling = new SafeSet();
  #throttle;
  #mode;

  constructor({ throttle = 500, mode = 'filter' } = kEmptyObject) {
    super();

    validateNumber(throttle, 'options.throttle', 0, TIMEOUT_MAX);
    validateOneOf(mode, 'options.mode', ['filter', 'all']);
    this.#throttle = throttle;
    this.#mode = mode;
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

  #onChange(trigger) {
    if (this.#throttling.has(trigger)) {
      return;
    }
    if (this.#mode === 'filter' && !this.#filteredFiles.has(trigger)) {
      return;
    }
    this.#throttling.add(trigger);
    this.emit('changed');
    setTimeout(() => this.#throttling.delete(trigger), this.#throttle).unref();
  }

  get watchedPaths() {
    return [...this.#watchers.keys()];
  }

  watchPath(path, recursive = true) {
    if (this.#isPathWatched(path)) {
      return;
    }
    const watcher = watch(path, { recursive });
    watcher.on('change', (eventType, fileName) => this
      .#onChange(recursive ? resolve(path, fileName) : path));
    this.#watchers.set(path, { handle: watcher, recursive });
    if (recursive) {
      this.#removeWatchedChildren(path);
    }
  }

  filterFile(file) {
    if (supportsRecursiveWatching) {
      this.watchPath(dirname(file));
    } else {
      // Having multiple FSWatcher's seems to be slower
      // than a single recursive FSWatcher
      this.watchPath(file, false);
    }
    this.#filteredFiles.add(file);
  }
  watchChildProcessModules(child) {
    if (this.#mode !== 'filter') {
      return;
    }
    child.on('message', (message) => {
      try {
        if (message['watch:require']) {
          this.filterFile(message['watch:require']);
        }
        if (message['watch:import']) {
          this.filterFile(fileURLToPath(message['watch:import']));
        }
      } catch {
        // Failed watching file. ignore
      }
    });
  }
  clearFileFilters() {
    this.#filteredFiles.clear();
  }
  clear() {
    this.#watchers.forEach(this.#unwatch);
    this.#watchers.clear();
    this.#filteredFiles.clear();
  }
}

module.exports = { FilesWatcher };
