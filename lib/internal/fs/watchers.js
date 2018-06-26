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
const { defaultTriggerAsyncIdScope } = require('internal/async_hooks');
const { toNamespacedPath } = require('path');
const { validateUint32 } = require('internal/validators');
const { getPathFromURL } = require('internal/url');
const util = require('util');
const assert = require('assert');

const kOldStatus = Symbol('kOldStatus');
const kUseBigint = Symbol('kUseBigint');
const kOwner = Symbol('kOwner');

function emitStop(self) {
  self.emit('stop');
}

function StatWatcher(bigint) {
  EventEmitter.call(this);

  this._handle = null;
  this[kOldStatus] = -1;
  this[kUseBigint] = bigint;
}
util.inherits(StatWatcher, EventEmitter);

function onchange(newStatus, stats) {
  const self = this[kOwner];
  if (self[kOldStatus] === -1 &&
      newStatus === -1 &&
      stats[2/* new nlink */] === stats[16/* old nlink */]) {
    return;
  }

  self[kOldStatus] = newStatus;
  self.emit('change', getStatsFromBinding(stats),
            getStatsFromBinding(stats, kFsStatsFieldsLength));
}

// FIXME(joyeecheung): this method is not documented.
// At the moment if filename is undefined, we
// 1. Throw an Error if it's the first time .start() is called
// 2. Return silently if .start() has already been called
//    on a valid filename and the wrap has been initialized
// This method is a noop if the watcher has already been started.
StatWatcher.prototype.start = function(filename, persistent, interval) {
  if (this._handle !== null)
    return;

  this._handle = new _StatWatcher(this[kUseBigint]);
  this._handle[kOwner] = this;
  this._handle.onchange = onchange;
  if (!persistent)
    this._handle.unref();

  // uv_fs_poll is a little more powerful than ev_stat but we curb it for
  // the sake of backwards compatibility
  this[kOldStatus] = -1;

  filename = getPathFromURL(filename);
  validatePath(filename, 'filename');
  validateUint32(interval, 'interval');
  const err = this._handle.start(toNamespacedPath(filename), interval);
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
  if (this._handle === null)
    return;

  defaultTriggerAsyncIdScope(this._handle.getAsyncId(),
                             process.nextTick,
                             emitStop,
                             this);
  this._handle.close();
  this._handle = null;
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
