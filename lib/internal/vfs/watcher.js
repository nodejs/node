'use strict';

const {
  ArrayPrototypePush,
  Promise,
  PromiseResolve,
  SafeMap,
  SafeSet,
  Symbol,
  SymbolAsyncIterator,
} = primordials;

const { EventEmitter } = require('events');
const { basename, join } = require('path');
const {
  setInterval,
  clearInterval,
} = require('timers');

// Private symbols
const kVfs = Symbol('kVfs');
const kPath = Symbol('kPath');
const kInterval = Symbol('kInterval');
const kTimer = Symbol('kTimer');
const kLastStats = Symbol('kLastStats');
const kClosed = Symbol('kClosed');
const kPersistent = Symbol('kPersistent');
const kListeners = Symbol('kListeners');
const kRecursive = Symbol('kRecursive');
const kTrackedFiles = Symbol('kTrackedFiles');
const kSignal = Symbol('kSignal');
const kAbortHandler = Symbol('kAbortHandler');

/**
 * VFSWatcher - Polling-based file/directory watcher for VFS.
 * Emits 'change' events when the file content or stats change.
 * Compatible with fs.watch() return value interface.
 */
class VFSWatcher extends EventEmitter {
  /**
   * @param {VirtualProvider} provider The VFS provider
   * @param {string} path The path to watch (provider-relative)
   * @param {object} [options] Options
   * @param {number} [options.interval] Polling interval in ms (default: 100)
   * @param {boolean} [options.persistent] Keep process alive (default: true)
   * @param {boolean} [options.recursive] Watch subdirectories (default: false)
   * @param {AbortSignal} [options.signal] AbortSignal for cancellation
   */
  constructor(provider, path, options = {}) {
    super();

    this[kVfs] = provider;
    this[kPath] = path;
    this[kInterval] = options.interval ?? 100;
    this[kPersistent] = options.persistent !== false;
    this[kRecursive] = options.recursive === true;
    this[kClosed] = false;
    this[kTimer] = null;
    this[kTrackedFiles] = new SafeMap(); // path -> { stats, relativePath }
    this[kSignal] = options.signal;
    this[kAbortHandler] = null;

    // Handle AbortSignal
    if (this[kSignal]) {
      if (this[kSignal].aborted) {
        this.close();
        return;
      }
      this[kAbortHandler] = () => this.close();
      this[kSignal].addEventListener('abort', this[kAbortHandler], { once: true });
    }

    // Get initial stats
    this[kLastStats] = this._getStats();

    // If recursive and watching a directory, build file list
    if (this[kRecursive] && this[kLastStats]?.isDirectory()) {
      this._buildFileList(this[kPath], '');
    }

    // Start polling
    this._startPolling();
  }

  /**
   * Gets stats for the watched path.
   * @returns {Stats|null} The stats or null if file doesn't exist
   * @private
   */
  _getStats() {
    try {
      return this[kVfs].statSync(this[kPath]);
    } catch {
      return null;
    }
  }

  /**
   * Starts the polling timer.
   * @private
   */
  _startPolling() {
    if (this[kClosed]) return;

    this[kTimer] = setInterval(() => this._poll(), this[kInterval]);

    // If not persistent, unref the timer to allow process to exit
    if (!this[kPersistent] && this[kTimer].unref) {
      this[kTimer].unref();
    }
  }

  /**
   * Polls for changes.
   * @private
   */
  _poll() {
    if (this[kClosed]) return;

    // For recursive directory watching, check all tracked files
    if (this[kRecursive] && this[kTrackedFiles].size > 0) {
      for (const { 0: filePath, 1: info } of this[kTrackedFiles]) {
        const newStats = this._getStatsFor(filePath);
        if (this._statsChanged(info.stats, newStats)) {
          const eventType = this._determineEventType(info.stats, newStats);
          this.emit('change', eventType, info.relativePath);
          info.stats = newStats;
        }
      }
      return;
    }

    // For single file/directory watching
    const newStats = this._getStats();

    if (this._statsChanged(this[kLastStats], newStats)) {
      const eventType = this._determineEventType(this[kLastStats], newStats);
      const filename = basename(this[kPath]);
      this.emit('change', eventType, filename);
    }

    this[kLastStats] = newStats;
  }

  /**
   * Gets stats for a specific path.
   * @param {string} filePath The file path
   * @returns {Stats|null}
   * @private
   */
  _getStatsFor(filePath) {
    try {
      return this[kVfs].statSync(filePath);
    } catch {
      return null;
    }
  }

  /**
   * Builds the list of files to track for recursive watching.
   * @param {string} dirPath The directory path
   * @param {string} relativePath The relative path from the watched root
   * @private
   */
  _buildFileList(dirPath, relativePath) {
    try {
      const entries = this[kVfs].readdirSync(dirPath, { withFileTypes: true });
      for (const entry of entries) {
        const fullPath = join(dirPath, entry.name);
        const relPath = relativePath ? join(relativePath, entry.name) : entry.name;

        if (entry.isDirectory()) {
          // Recurse into subdirectory
          this._buildFileList(fullPath, relPath);
        } else {
          // Track the file
          const stats = this._getStatsFor(fullPath);
          this[kTrackedFiles].set(fullPath, {
            stats,
            relativePath: relPath,
          });
        }
      }
    } catch {
      // Directory might not exist or be readable
    }
  }

  /**
   * Checks if stats have changed.
   * @param {Stats|null} oldStats Previous stats
   * @param {Stats|null} newStats Current stats
   * @returns {boolean} True if stats changed
   * @private
   */
  _statsChanged(oldStats, newStats) {
    // File created or deleted
    if ((oldStats === null) !== (newStats === null)) {
      return true;
    }

    // Both null - no change
    if (oldStats === null && newStats === null) {
      return false;
    }

    // Compare mtime and size
    if (oldStats.mtimeMs !== newStats.mtimeMs) {
      return true;
    }
    if (oldStats.size !== newStats.size) {
      return true;
    }

    return false;
  }

  /**
   * Determines the event type based on stats change.
   * @param {Stats|null} oldStats Previous stats
   * @param {Stats|null} newStats Current stats
   * @returns {string} 'rename' or 'change'
   * @private
   */
  _determineEventType(oldStats, newStats) {
    // File was created or deleted
    if ((oldStats === null) !== (newStats === null)) {
      return 'rename';
    }
    // Content changed
    return 'change';
  }

  /**
   * Closes the watcher and stops polling.
   */
  close() {
    if (this[kClosed]) return;
    this[kClosed] = true;

    if (this[kTimer]) {
      clearInterval(this[kTimer]);
      this[kTimer] = null;
    }

    // Clear tracked files
    this[kTrackedFiles].clear();

    // Remove abort handler
    if (this[kSignal] && this[kAbortHandler]) {
      this[kSignal].removeEventListener('abort', this[kAbortHandler]);
    }

    this.emit('close');
  }

  /**
   * Alias for close() - compatibility with FSWatcher.
   * @returns {this}
   */
  unref() {
    this[kTimer]?.unref?.();
    return this;
  }

  /**
   * Makes the timer keep the process alive - compatibility with FSWatcher.
   * @returns {this}
   */
  ref() {
    this[kTimer]?.ref?.();
    return this;
  }
}

/**
 * VFSStatWatcher - Polling-based stat watcher for VFS.
 * Emits 'change' events with current and previous stats.
 * Compatible with fs.watchFile() return value interface.
 */
class VFSStatWatcher extends EventEmitter {
  /**
   * @param {VirtualProvider} provider The VFS provider
   * @param {string} path The path to watch (provider-relative)
   * @param {object} [options] Options
   * @param {number} [options.interval] Polling interval in ms (default: 5007)
   * @param {boolean} [options.persistent] Keep process alive (default: true)
   */
  constructor(provider, path, options = {}) {
    super();

    this[kVfs] = provider;
    this[kPath] = path;
    this[kInterval] = options.interval ?? 5007;
    this[kPersistent] = options.persistent !== false;
    this[kClosed] = false;
    this[kTimer] = null;
    this[kListeners] = new SafeSet();

    // Get initial stats
    this[kLastStats] = this._getStats();

    // Start polling
    this._startPolling();
  }

  /**
   * Gets stats for the watched path.
   * @returns {Stats} The stats (with zeroed values if file doesn't exist)
   * @private
   */
  _getStats() {
    try {
      return this[kVfs].statSync(this[kPath]);
    } catch {
      // Return a zeroed stats object for non-existent files
      // This matches Node.js behavior
      return this._createZeroStats();
    }
  }

  /**
   * Creates a zeroed stats object for non-existent files.
   * @returns {object} Zeroed stats
   * @private
   */
  _createZeroStats() {
    const { createFileStats } = require('internal/vfs/stats');
    return createFileStats(0, {
      mode: 0,
      mtimeMs: 0,
      ctimeMs: 0,
      birthtimeMs: 0,
    });
  }

  /**
   * Starts the polling timer.
   * @private
   */
  _startPolling() {
    if (this[kClosed]) return;

    this[kTimer] = setInterval(() => this._poll(), this[kInterval]);

    // If not persistent, unref the timer to allow process to exit
    if (!this[kPersistent] && this[kTimer].unref) {
      this[kTimer].unref();
    }
  }

  /**
   * Polls for changes.
   * @private
   */
  _poll() {
    if (this[kClosed]) return;

    const newStats = this._getStats();

    if (this._statsChanged(this[kLastStats], newStats)) {
      const prevStats = this[kLastStats];
      this[kLastStats] = newStats;
      this.emit('change', newStats, prevStats);
    }
  }

  /**
   * Checks if stats have changed.
   * @param {Stats} oldStats Previous stats
   * @param {Stats} newStats Current stats
   * @returns {boolean} True if stats changed
   * @private
   */
  _statsChanged(oldStats, newStats) {
    // Compare mtime and ctime
    if (oldStats.mtimeMs !== newStats.mtimeMs) {
      return true;
    }
    if (oldStats.ctimeMs !== newStats.ctimeMs) {
      return true;
    }
    if (oldStats.size !== newStats.size) {
      return true;
    }
    return false;
  }

  /**
   * Adds a change listener.
   * @param {Function} listener The listener function
   */
  addListener(listener) {
    this[kListeners].add(listener);
    this.on('change', listener);
  }

  /**
   * Removes a change listener.
   * @param {Function} listener The listener function
   * @returns {boolean} True if listener was removed
   */
  removeListener(listener) {
    const had = this[kListeners].has(listener);
    this[kListeners].delete(listener);
    super.removeListener('change', listener);
    return had;
  }

  /**
   * Returns true if there are no listeners.
   * @returns {boolean}
   */
  hasNoListeners() {
    return this[kListeners].size === 0;
  }

  /**
   * Stops the watcher.
   */
  stop() {
    if (this[kClosed]) return;
    this[kClosed] = true;

    if (this[kTimer]) {
      clearInterval(this[kTimer]);
      this[kTimer] = null;
    }

    this.emit('stop');
  }

  /**
   * Makes the timer not keep the process alive.
   * @returns {this}
   */
  unref() {
    this[kTimer]?.unref?.();
    return this;
  }

  /**
   * Makes the timer keep the process alive.
   * @returns {this}
   */
  ref() {
    this[kTimer]?.ref?.();
    return this;
  }
}

/**
 * VFSWatchAsyncIterable - Async iterable wrapper for VFSWatcher.
 * Compatible with fs.promises.watch() return value interface.
 */
class VFSWatchAsyncIterable {
  #watcher;
  #closed = false;
  #pendingEvents = [];
  #pendingResolvers = [];

  /**
   * @param {VirtualProvider} provider The VFS provider
   * @param {string} path The path to watch (provider-relative)
   * @param {object} [options] Options
   */
  constructor(provider, path, options = {}) {
    this.#watcher = new VFSWatcher(provider, path, options);

    this.#watcher.on('change', (eventType, filename) => {
      const event = { eventType, filename };
      if (this.#pendingResolvers.length > 0) {
        const resolve = this.#pendingResolvers.shift();
        resolve({ value: event, done: false });
      } else {
        ArrayPrototypePush(this.#pendingEvents, event);
      }
    });

    this.#watcher.on('close', () => {
      this.#closed = true;
      // Resolve any pending iterators
      while (this.#pendingResolvers.length > 0) {
        const resolve = this.#pendingResolvers.shift();
        resolve({ value: undefined, done: true });
      }
    });
  }

  /**
   * Returns the async iterator.
   * @returns {AsyncIterator}
   */
  [SymbolAsyncIterator]() {
    return this;
  }

  /**
   * Gets the next event.
   * @returns {Promise<IteratorResult>}
   */
  next() {
    if (this.#closed) {
      return PromiseResolve({ value: undefined, done: true });
    }

    if (this.#pendingEvents.length > 0) {
      const event = this.#pendingEvents.shift();
      return PromiseResolve({ value: event, done: false });
    }

    return new Promise((resolve) => {
      ArrayPrototypePush(this.#pendingResolvers, resolve);
    });
  }

  /**
   * Closes the iterator and underlying watcher.
   * @returns {Promise<IteratorResult>}
   */
  return() {
    this.#watcher.close();
    return PromiseResolve({ value: undefined, done: true });
  }

  /**
   * Handles iterator throw.
   * @param {Error} error The error to throw
   * @returns {Promise<IteratorResult>}
   */
  throw(error) {
    this.#watcher.close();
    return PromiseResolve({ value: undefined, done: true });
  }
}

module.exports = {
  VFSWatcher,
  VFSStatWatcher,
  VFSWatchAsyncIterable,
};
