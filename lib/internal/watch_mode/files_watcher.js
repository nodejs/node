'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeForEach,
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

class FilesWatcher extends EventEmitter {
  #watchers = new SafeMap();
  #filteredFiles = new SafeSet();
  #throttling = new SafeSet();
  #depencencyOwners = new SafeMap();
  #ownerDependencies = new SafeMap();
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

    for (const watchedPath of this.#watchers.keys()) {
      if (StringPrototypeStartsWith(path, watchedPath)) {
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
    const owners = this.#depencencyOwners.get(trigger);
    this.emit('changed', { owners });
    setTimeout(() => this.#throttling.delete(trigger), this.#throttle).unref();
  }

  get watchedPaths() {
    return [...this.#watchers.keys()];
  }

  watchPath(path) {
    if (this.#isPathWatched(path)) {
      return;
    }
    const watcher = watch(path, { recursive: true });
    watcher.on('change', (eventType, fileName) => this.#onChange(resolve(path, fileName)));
    this.#watchers.set(path, { handle: watcher });
    this.#removeWatchedChildren(path);
  }

  filterFile(file, owner) {
    if (!file) return;
    this.watchPath(dirname(file));
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
  watchChildProcessModules(child, key = null) {
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
