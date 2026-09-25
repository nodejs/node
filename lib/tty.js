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

'use strict';

const {
  NumberIsInteger,
  ObjectSetPrototypeOf,
} = primordials;

const net = require('net');
const {
  TTY,
  UV_TTY_MODE_IO,
  UV_TTY_MODE_NORMAL,
  UV_TTY_MODE_RAW_VT,
  isTTY,
} = internalBinding('tty_wrap');
const {
  ErrnoException,
  codes: {
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_FD,
    ERR_TTY_INIT_FAILED,
  },
} = require('internal/errors');
const {
  getColorDepth,
  hasColors,
} = require('internal/tty');

// Lazy loaded for startup performance.
let readline;

function isatty(fd) {
  return NumberIsInteger(fd) && fd >= 0 && fd <= 2147483647 &&
         isTTY(fd);
}

function ReadStream(fd, options) {
  if (!(this instanceof ReadStream))
    return new ReadStream(fd, options);
  if (fd >> 0 !== fd || fd < 0)
    throw new ERR_INVALID_FD(fd);

  const ctx = {};
  const tty = new TTY(fd, ctx);
  if (ctx.code !== undefined) {
    throw new ERR_TTY_INIT_FAILED(ctx);
  }

  net.Socket.call(this, {
    readableHighWaterMark: 0,
    handle: tty,
    manualStart: true,
    ...options,
  });

  this.isRaw = false;
  this.rawMode = false;
  this.isTTY = true;
}

ObjectSetPrototypeOf(ReadStream.prototype, net.Socket.prototype);
ObjectSetPrototypeOf(ReadStream, net.Socket);

ReadStream.prototype.setRawMode = function(mode) {
  let rawMode;
  if (mode === 'io' || mode === 'raw') {
    rawMode = mode;
  } else if (typeof mode === 'string') {
    throw new ERR_INVALID_ARG_VALUE(
      'mode', mode, "must be true, false, 'raw', or 'io'");
  } else {
    rawMode = mode ? 'raw' : false;
  }
  let ttyMode = UV_TTY_MODE_NORMAL;
  if (rawMode === 'io') {
    ttyMode = UV_TTY_MODE_IO;
  } else if (rawMode === 'raw') {
    ttyMode = UV_TTY_MODE_RAW_VT;
  }
  const err = this._handle?.setRawMode(ttyMode);
  if (err) {
    this.emit('error', new ErrnoException(err, 'setRawMode'));
    return this;
  }
  this.isRaw = rawMode !== false;
  this.rawMode = rawMode;
  return this;
};

function WriteStream(fd) {
  if (!(this instanceof WriteStream))
    return new WriteStream(fd);
  if (fd >> 0 !== fd || fd < 0)
    throw new ERR_INVALID_FD(fd);

  const ctx = {};
  const tty = new TTY(fd, ctx);
  if (ctx.code !== undefined) {
    throw new ERR_TTY_INIT_FAILED(ctx);
  }

  net.Socket.call(this, {
    readableHighWaterMark: 0,
    handle: tty,
    manualStart: true,
  });

  // Prevents interleaved or dropped stdout/stderr output for terminals.
  // As noted in the following reference, local TTYs tend to be quite fast and
  // this behavior has become expected due historical functionality on OS X,
  // even though it was originally intended to change in v1.0.2 (Libuv 1.2.1).
  // Ref: https://github.com/nodejs/node/pull/1771#issuecomment-119351671
  this._handle.setBlocking(true);

  const winSize = [0, 0];
  const err = this._handle.getWindowSize(winSize);
  if (!err) {
    this.columns = winSize[0];
    this.rows = winSize[1];
  }
}

ObjectSetPrototypeOf(WriteStream.prototype, net.Socket.prototype);
ObjectSetPrototypeOf(WriteStream, net.Socket);

WriteStream.prototype.isTTY = true;

WriteStream.prototype.getColorDepth = getColorDepth;

WriteStream.prototype.hasColors = hasColors;

WriteStream.prototype._refreshSize = function() {
  const oldCols = this.columns;
  const oldRows = this.rows;
  const winSize = [0, 0];
  const err = this._handle.getWindowSize(winSize);
  if (err) {
    this.emit('error', new ErrnoException(err, 'getWindowSize'));
    return;
  }
  const { 0: newCols, 1: newRows } = winSize;
  if (oldCols !== newCols || oldRows !== newRows) {
    this.columns = newCols;
    this.rows = newRows;
    this.emit('resize');
  }
};

// Backwards-compat
WriteStream.prototype.cursorTo = function(x, y, callback) {
  if (readline === undefined) readline = require('readline');
  return readline.cursorTo(this, x, y, callback);
};
WriteStream.prototype.moveCursor = function(dx, dy, callback) {
  if (readline === undefined) readline = require('readline');
  return readline.moveCursor(this, dx, dy, callback);
};
WriteStream.prototype.clearLine = function(dir, callback) {
  if (readline === undefined) readline = require('readline');
  return readline.clearLine(this, dir, callback);
};
WriteStream.prototype.clearScreenDown = function(callback) {
  if (readline === undefined) readline = require('readline');
  return readline.clearScreenDown(this, callback);
};
WriteStream.prototype.getWindowSize = function() {
  return [this.columns, this.rows];
};

module.exports = { isatty, ReadStream, WriteStream };
