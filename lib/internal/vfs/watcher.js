'use strict';

const {
  ArrayPrototypePush,
  Promise,
  PromiseResolve,
  SafeMap,
  SafeSet,
  SymbolAsyncIterator,
} = primordials;

const { EventEmitter } = require('events');
const { basename, join } = require('path');
const {
  setInterval,
  clearInterval,
} = require('timers');

/**
 * VFSWatcher - Polling-based file/directory watcher for VFS.
 * Emits 'change' events when the file content or stats change.
 * Compatible with fs.watch() return value interface.
 */
class VFSWatcher extends EventEmitter {
  #vfs;
  #path;
  #interval;
  #timer = null;
  #lastStats;
  #closed = false;
  #persistent;
  #recursive;
  #trackedFiles;
  #signal;
  #abortHandler = null;

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

    this.#vfs = provider;
    this.#path = path;
    this.#interval = options.interval ?? 100;
    this.#persistent = options.persistent !== false;
    this.#recursive = options.recursive === true;
    this.#trackedFiles = new SafeMap(); // path -> { stats, relativePath }
    this.#signal = options.signal;

    // Handle AbortSignal
    if (this.#signal) {
      if (this.#signal.aborted) {
        this.close();
        return;
      }
      this.#abortHandler = () => this.close();
      this.#signal.addEventListener('abort', this.#abortHandler, { once: true });
    }

    // Get initial stats
    this.#lastStats = this.#getStats();

    // If recursive and watching a directory, build file list
    if (this.#recursive && this.#lastStats?.isDirectory()) {
      this.#buildFileList(this.#path, '');
    }

    // Start polling
    this.#startPolling();
  }

  /**
   * Gets stats for the watched path.
   * @returns {Stats|null} The stats or null if file doesn't exist
   */
  #getStats() {
    try {
      return this.#vfs.statSync(this.#path);
    } catch {
      return null;
    }
  }

  /**
   * Starts the polling timer.
   */
  #startPolling() {
    if (this.#closed) return;

    this.#timer = setInterval(() => this.#poll(), this.#interval);

    // If not persistent, unref the timer to allow process to exit
    if (!this.#persistent && this.#timer.unref) {
      this.#timer.unref();
    }
  }

  /**
   * Polls for changes.
   */
  #poll() {
    if (this.#closed) return;

    // For recursive directory watching, check all tracked files
    if (this.#recursive && this.#trackedFiles.size > 0) {
      for (const { 0: filePath, 1: info } of this.#trackedFiles) {
        const newStats = this.#getStatsFor(filePath);
        if (this.#statsChanged(info.stats, newStats)) {
          const eventType = this.#determineEventType(info.stats, newStats);
          this.emit('change', eventType, info.relativePath);
          info.stats = newStats;
        }
      }
      return;
    }

    // For single file/directory watching
    const newStats = this.#getStats();

    if (this.#statsChanged(this.#lastStats, newStats)) {
      const eventType = this.#determineEventType(this.#lastStats, newStats);
      const filename = basename(this.#path);
      this.emit('change', eventType, filename);
    }

    this.#lastStats = newStats;
  }

  /**
   * Gets stats for a specific path.
   * @param {string} filePath The file path
   * @returns {Stats|null}
   */
  #getStatsFor(filePath) {
    try {
      return this.#vfs.statSync(filePath);
    } catch {
      return null;
    }
  }

  /**
   * Builds the list of files to track for recursive watching.
   * @param {string} dirPath The directory path
   * @param {string} relativePath The relative path from the watched root
   */
  #buildFileList(dirPath, relativePath) {
    try {
      const entries = this.#vfs.readdirSync(dirPath, { withFileTypes: true });
      for (const entry of entries) {
        const fullPath = join(dirPath, entry.name);
        const relPath = relativePath ? join(relativePath, entry.name) : entry.name;

        if (entry.isDirectory()) {
          // Recurse into subdirectory
          this.#buildFileList(fullPath, relPath);
        } else {
          // Track the file
          const stats = this.#getStatsFor(fullPath);
          this.#trackedFiles.set(fullPath, {
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
   */
  #statsChanged(oldStats, newStats) {
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
   */
  #determineEventType(oldStats, newStats) {
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
    if (this.#closed) return;
    this.#closed = true;

    if (this.#timer) {
      clearInterval(this.#timer);
      this.#timer = null;
    }

    // Clear tracked files
    this.#trackedFiles.clear();

    // Remove abort handler
    if (this.#signal && this.#abortHandler) {
      this.#signal.removeEventListener('abort', this.#abortHandler);
    }

    this.emit('close');
  }

  /**
   * Alias for close() - compatibility with FSWatcher.
   * @returns {this}
   */
  unref() {
    this.#timer?.unref?.();
    return this;
  }

  /**
   * Makes the timer keep the process alive - compatibility with FSWatcher.
   * @returns {this}
   */
  ref() {
    this.#timer?.ref?.();
    return this;
  }
}

/**
 * VFSStatWatcher - Polling-based stat watcher for VFS.
 * Emits 'change' events with current and previous stats.
 * Compatible with fs.watchFile() return value interface.
 */
class VFSStatWatcher extends EventEmitter {
  #vfs;
  #path;
  #interval;
  #persistent;
  #closed = false;
  #timer = null;
  #lastStats;
  #listeners;

  /**
   * @param {VirtualProvider} provider The VFS provider
   * @param {string} path The path to watch (provider-relative)
   * @param {object} [options] Options
   * @param {number} [options.interval] Polling interval in ms (default: 5007)
   * @param {boolean} [options.persistent] Keep process alive (default: true)
   */
  constructor(provider, path, options = {}) {
    super();

    this.#vfs = provider;
    this.#path = path;
    this.#interval = options.interval ?? 5007;
    this.#persistent = options.persistent !== false;
    this.#listeners = new SafeSet();

    // Get initial stats
    this.#lastStats = this.#getStats();

    // Start polling
    this.#startPolling();
  }

  /**
   * Gets stats for the watched path.
   * @returns {Stats} The stats (with zeroed values if file doesn't exist)
   */
  #getStats() {
    try {
      return this.#vfs.statSync(this.#path);
    } catch {
      // Return a zeroed stats object for non-existent files
      // This matches Node.js behavior
      return this.#createZeroStats();
    }
  }

  /**
   * Creates a zeroed stats object for non-existent files.
   * @returns {object} Zeroed stats
   */
  #createZeroStats() {
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
   */
  #startPolling() {
    if (this.#closed) return;

    this.#timer = setInterval(() => this.#poll(), this.#interval);

    // If not persistent, unref the timer to allow process to exit
    if (!this.#persistent && this.#timer.unref) {
      this.#timer.unref();
    }
  }

  /**
   * Polls for changes.
   */
  #poll() {
    if (this.#closed) return;

    const newStats = this.#getStats();

    if (this.#statsChanged(this.#lastStats, newStats)) {
      const prevStats = this.#lastStats;
      this.#lastStats = newStats;
      this.emit('change', newStats, prevStats);
    }
  }

  /**
   * Checks if stats have changed.
   * @param {Stats} oldStats Previous stats
   * @param {Stats} newStats Current stats
   * @returns {boolean} True if stats changed
   */
  #statsChanged(oldStats, newStats) {
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
    this.#listeners.add(listener);
    this.on('change', listener);
  }

  /**
   * Removes a change listener.
   * @param {Function} listener The listener function
   * @returns {boolean} True if listener was removed
   */
  removeListener(listener) {
    const had = this.#listeners.has(listener);
    this.#listeners.delete(listener);
    super.removeListener('change', listener);
    return had;
  }

  /**
   * Returns true if there are no listeners.
   * @returns {boolean}
   */
  hasNoListeners() {
    return this.#listeners.size === 0;
  }

  /**
   * Stops the watcher.
   */
  stop() {
    if (this.#closed) return;
    this.#closed = true;

    if (this.#timer) {
      clearInterval(this.#timer);
      this.#timer = null;
    }

    this.emit('stop');
  }

  /**
   * Makes the timer not keep the process alive.
   * @returns {this}
   */
  unref() {
    this.#timer?.unref?.();
    return this;
  }

  /**
   * Makes the timer keep the process alive.
   * @returns {this}
   */
  ref() {
    this.#timer?.ref?.();
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
