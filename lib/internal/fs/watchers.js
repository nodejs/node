'use strict';

const errors = require('internal/errors');
const {
  kFsStatsFieldsLength,
  StatWatcher: _StatWatcher
} = process.binding('fs');
const { FSEvent } = process.binding('fs_event_wrap');
const { EventEmitter } = require('events');
const {
  getStatsFromBinding,
  validatePath
} = require('internal/fs/utils');
const { toNamespacedPath } = require('path');
const { validateUint32 } = require('internal/validators');
const { getPathFromURL } = require('internal/url');
const util = require('util');
const assert = require('assert');

function emitStop(self) {
  self.emit('stop');
}

function StatWatcher() {
  EventEmitter.call(this);

  this._handle = new _StatWatcher();

  // uv_fs_poll is a little more powerful than ev_stat but we curb it for
  // the sake of backwards compatibility
  let oldStatus = -1;

  this._handle.onchange = (newStatus, stats) => {
    if (oldStatus === -1 &&
        newStatus === -1 &&
        stats[2/* new nlink */] === stats[16/* old nlink */]) return;

    oldStatus = newStatus;
    this.emit('change', getStatsFromBinding(stats),
              getStatsFromBinding(stats, kFsStatsFieldsLength));
  };

  this._handle.onstop = () => {
    process.nextTick(emitStop, this);
  };
}
util.inherits(StatWatcher, EventEmitter);


// FIXME(joyeecheung): this method is not documented.
// At the moment if filename is undefined, we
// 1. Throw an Error if it's the first time .start() is called
// 2. Return silently if .start() has already been called
//    on a valid filename and the wrap has been initialized
// This method is a noop if the watcher has already been started.
StatWatcher.prototype.start = function(filename, persistent, interval) {
  assert(this._handle instanceof _StatWatcher, 'handle must be a StatWatcher');
  if (this._handle.isActive) {
    return;
  }

  filename = getPathFromURL(filename);
  validatePath(filename, 'filename');
  validateUint32(interval, 'interval');
  const err = this._handle.start(toNamespacedPath(filename),
                                 persistent, interval);
  if (err) {
    const error = errors.uvException({
      errno: err,
      syscall: 'watch',
      path: filename
    });
    error.filename = filename;
    throw error;
  }
};

// FIXME(joyeecheung): this method is not documented while there is
// another documented fs.unwatchFile(). The counterpart in
// FSWatcher is .close()
// This method is a noop if the watcher has not been started.
StatWatcher.prototype.stop = function() {
  assert(this._handle instanceof _StatWatcher, 'handle must be a StatWatcher');
  if (!this._handle.isActive) {
    return;
  }
  this._handle.stop();
};


function FSWatcher() {
  EventEmitter.call(this);

  this._handle = new FSEvent();
  this._handle.owner = this;

  this._handle.onchange = (status, eventType, filename) => {
    // TODO(joyeecheung): we may check self._handle.initialized here
    // and return if that is false. This allows us to avoid firing the event
    // after the handle is closed, and to fire both UV_RENAME and UV_CHANGE
    // if they are set by libuv at the same time.
    if (status < 0) {
      if (this._handle !== null) {
        // We don't use this.close() here to avoid firing the close event.
        this._handle.close();
        this._handle = null;  // make the handle garbage collectable
      }
      const error = errors.uvException({
        errno: status,
        syscall: 'watch',
        path: filename
      });
      error.filename = filename;
      this.emit('error', error);
    } else {
      this.emit('change', eventType, filename);
    }
  };
}
util.inherits(FSWatcher, EventEmitter);

// FIXME(joyeecheung): this method is not documented.
// At the moment if filename is undefined, we
// 1. Throw an Error if it's the first time .start() is called
// 2. Return silently if .start() has already been called
//    on a valid filename and the wrap has been initialized
// 3. Return silently if the watcher has already been closed
// This method is a noop if the watcher has already been started.
FSWatcher.prototype.start = function(filename,
                                     persistent,
                                     recursive,
                                     encoding) {
  if (this._handle === null) {  // closed
    return;
  }
  assert(this._handle instanceof FSEvent, 'handle must be a FSEvent');
  if (this._handle.initialized) {  // already started
    return;
  }

  filename = getPathFromURL(filename);
  validatePath(filename, 'filename');

  const err = this._handle.start(toNamespacedPath(filename),
                                 persistent,
                                 recursive,
                                 encoding);
  if (err) {
    const error = errors.uvException({
      errno: err,
      syscall: 'watch',
      path: filename
    });
    error.filename = filename;
    throw error;
  }
};

// This method is a noop if the watcher has not been started or
// has already been closed.
FSWatcher.prototype.close = function() {
  if (this._handle === null) {  // closed
    return;
  }
  assert(this._handle instanceof FSEvent, 'handle must be a FSEvent');
  if (!this._handle.initialized) {  // not started
    return;
  }
  this._handle.close();
  this._handle = null;  // make the handle garbage collectable
  process.nextTick(emitCloseNT, this);
};

function emitCloseNT(self) {
  self.emit('close');
}

module.exports = {
  FSWatcher,
  StatWatcher
};
