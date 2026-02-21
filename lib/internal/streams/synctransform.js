// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// A synchronous transform stream.
// Passes data through synchronously with optional transformation.
// Enforces backpressure without internal buffering for high performance.

'use strict';

const {
  FunctionPrototypeCall,
  ObjectSetPrototypeOf,
} = primordials;

const { EventEmitter } = require('events');

const {
  codes: {
    ERR_STREAM_WRITE_AFTER_END,
  },
} = require('internal/errors');

const { validateFunction } = require('internal/validators');

const kDestination = Symbol('kDestination');
const kInFlight = Symbol('kInFlight');
const kTransform = Symbol('kTransform');
const kFlush = Symbol('kFlush');
const kDestinationNeedsEnd = Symbol('kDestinationNeedsEnd');
const kLastPush = Symbol('kLastPush');
const kEndEmitted = Symbol('kEndEmitted');
const kDestroyed = Symbol('kDestroyed');

function passthrough(chunk) {
  return chunk;
}

function SyncTransform(transform, flush) {
  if (!(this instanceof SyncTransform))
    return new SyncTransform(transform, flush);

  if (transform !== undefined && transform !== null) {
    validateFunction(transform, 'transform');
  }
  if (flush !== undefined && flush !== null) {
    validateFunction(flush, 'flush');
  }

  FunctionPrototypeCall(EventEmitter, this);

  this[kTransform] = transform || passthrough;
  this[kFlush] = flush || passthrough;
  this[kDestination] = null;
  this[kInFlight] = undefined;
  this.writable = true;
  this[kEndEmitted] = false;
  this[kDestinationNeedsEnd] = true;
  this[kLastPush] = true;
  this[kDestroyed] = false;

  this.on('newListener', onNewListener);
  this.on('removeListener', onRemoveListener);
  this.on('end', onEnd);
}

ObjectSetPrototypeOf(SyncTransform.prototype, EventEmitter.prototype);
ObjectSetPrototypeOf(SyncTransform, EventEmitter);

function onNewListener(ev) {
  if (ev === 'data') {
    if (this[kDestination] && !(this[kDestination] instanceof OnData)) {
      throw new Error('you can use only pipe() or on(\'data\')');
    }
    process.nextTick(deferPiping, this);
  }
}

function deferPiping(that) {
  if (that[kDestination] && that[kDestination] instanceof OnData) {
    // Nothing to do, piping was deferred twice for on('data').
    return;
  }

  that.pipe(new OnData(that));
  if (!that.writable && !that[kEndEmitted]) {
    that.emit('end');
  }
}

function onRemoveListener(ev) {
  if (ev === 'data' &&
      ((ev.listenerCount && ev.listenerCount(this, ev)) !== 0)) {
    this.unpipe(this[kDestination]);
  }
}

function onEnd() {
  this[kEndEmitted] = true;
}

SyncTransform.prototype.pipe = function(dest, opts) {
  const that = this;
  const inFlight = this[kInFlight];

  if (this[kDestination]) {
    throw new Error('multiple pipe not allowed');
  }
  this[kDestination] = dest;

  dest.emit('pipe', this);

  this[kDestination].on('drain', function() {
    that.emit('drain');
  });

  this[kDestination].on('end', function() {
    that.end();
  });

  this[kDestinationNeedsEnd] = !opts || opts.end !== false;

  if (inFlight && this[kDestination].write(inFlight)) {
    this[kInFlight] = undefined;
    this.emit('drain');
  } else if (inFlight === null) {
    doEnd(this);
  }

  return dest;
};

SyncTransform.prototype.unpipe = function(dest) {
  if (!this[kDestination] || this[kDestination] !== dest) {
    return this;
  }

  this[kDestination] = null;

  dest.emit('unpipe', this);

  return this;
};

SyncTransform.prototype.write = function(chunk) {
  if (!this.writable) {
    this.emit('error', new ERR_STREAM_WRITE_AFTER_END());
    return false;
  }

  const res = this[kTransform](chunk);

  if (!this[kDestination]) {
    if (this[kInFlight]) {
      this.emit('error', new Error('upstream must respect backpressure'));
      return false;
    }
    this[kInFlight] = res;
    return false;
  }

  if (res) {
    this[kLastPush] = this[kDestination].write(res);
  } else if (res === null) {
    doEnd(this);
    return false;
  }

  return this[kLastPush];
};

SyncTransform.prototype.push = function(chunk) {
  // Ignoring the return value.
  this[kLastPush] = this[kDestination].write(chunk);
  return this;
};

SyncTransform.prototype.end = function(chunk) {
  if (chunk) {
    this.write(chunk); // Errors if we are after EOF.
  }

  doEnd(this);

  return this;
};

function doEnd(that) {
  if (that.writable) {
    that.writable = false;
    if (that[kDestination]) {
      that[kEndEmitted] = true;
      const toFlush = that[kFlush]() || null;
      if (that[kDestinationNeedsEnd]) {
        that[kDestination].end(toFlush);
      } else if (toFlush !== null) {
        that[kDestination].write(toFlush);
      }
      that.emit('end');
    }
  }
}

SyncTransform.prototype.destroy = function(err) {
  if (!this[kDestroyed]) {
    this[kDestroyed] = true;

    process.nextTick(doDestroy, this, err);
  }

  return this;
};

function doDestroy(that, err) {
  if (err) {
    that.emit('error', err);
  }
  that.emit('close');
}

// Internal class for handling on('data') events.
function OnData(parent) {
  this.parent = parent;
  FunctionPrototypeCall(EventEmitter, this);
}

ObjectSetPrototypeOf(OnData.prototype, EventEmitter.prototype);
ObjectSetPrototypeOf(OnData, EventEmitter);

OnData.prototype.write = function(chunk) {
  this.parent.emit('data', chunk);
  return true;
};

OnData.prototype.end = function() {
  // Intentionally empty.
};

module.exports = SyncTransform;
