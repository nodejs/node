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

const { inherits, _errnoException, _extend } = require('util');
const net = require('net');
const { TTY, isTTY } = process.binding('tty_wrap');
const errors = require('internal/errors');
const readline = require('readline');
const { release } = require('os');

const OSRelease = release().split('.');

const COLORS_2 = 1;
const COLORS_16 = 4;
const COLORS_256 = 8;
const COLORS_16m = 24;

function isatty(fd) {
  return Number.isInteger(fd) && fd >= 0 && isTTY(fd);
}

function ReadStream(fd, options) {
  if (!(this instanceof ReadStream))
    return new ReadStream(fd, options);
  if (fd >> 0 !== fd || fd < 0)
    throw new errors.RangeError('ERR_INVALID_FD', fd);

  const ctx = {};
  const tty = new TTY(fd, true, ctx);
  if (ctx.code !== undefined) {
    throw new errors.SystemError(ctx);
  }

  options = _extend({
    highWaterMark: 0,
    readable: true,
    writable: false,
    handle: tty
  }, options);

  net.Socket.call(this, options);

  this.isRaw = false;
  this.isTTY = true;
}
inherits(ReadStream, net.Socket);

ReadStream.prototype.setRawMode = function(flag) {
  flag = !!flag;
  this._handle.setRawMode(flag);
  this.isRaw = flag;
};

function WriteStream(fd) {
  if (!(this instanceof WriteStream))
    return new WriteStream(fd);
  if (fd >> 0 !== fd || fd < 0)
    throw new errors.RangeError('ERR_INVALID_FD', fd);

  const ctx = {};
  const tty = new TTY(fd, false, ctx);
  if (ctx.code !== undefined) {
    throw new errors.SystemError(ctx);
  }

  net.Socket.call(this, {
    handle: tty,
    readable: false,
    writable: true
  });

  // Prevents interleaved or dropped stdout/stderr output for terminals.
  // As noted in the following reference, local TTYs tend to be quite fast and
  // this behavior has become expected due historical functionality on OS X,
  // even though it was originally intended to change in v1.0.2 (Libuv 1.2.1).
  // Ref: https://github.com/nodejs/node/pull/1771#issuecomment-119351671
  this._handle.setBlocking(true);

  const winSize = new Array(2);
  const err = this._handle.getWindowSize(winSize);
  if (!err) {
    this.columns = winSize[0];
    this.rows = winSize[1];
  }
}
inherits(WriteStream, net.Socket);

WriteStream.prototype.isTTY = true;

WriteStream.prototype.getColorDepth = function(env = process.env) {
  if (env.NODE_DISABLE_COLORS || env.TERM === 'dumb' && !env.COLORTERM) {
    return COLORS_2;
  }

  if (process.platform === 'win32') {
    // Windows 10 build 10586 is the first Windows release that supports 256
    // colors. Windows 10 build 14931 is the first release that supports
    // 16m/TrueColor.
    if (+OSRelease[0] >= 10) {
      const build = +OSRelease[2];
      if (build >= 14931)
        return COLORS_16m;
      if (build >= 10586)
        return COLORS_256;
    }

    return COLORS_16;
  }

  if (env.TMUX) {
    return COLORS_256;
  }

  if (env.CI) {
    if ('TRAVIS' in env || 'CIRCLECI' in env || 'APPVEYOR' in env ||
      'GITLAB_CI' in env || env.CI_NAME === 'codeship') {
      return COLORS_256;
    }
    return COLORS_2;
  }

  if ('TEAMCITY_VERSION' in env) {
    return /^(9\.(0*[1-9]\d*)\.|\d{2,}\.)/.test(env.TEAMCITY_VERSION) ?
      COLORS_16 : COLORS_2;
  }

  switch (env.TERM_PROGRAM) {
    case 'iTerm.app':
      if (!env.TERM_PROGRAM_VERSION ||
        /^[0-2]\./.test(env.TERM_PROGRAM_VERSION)) {
        return COLORS_256;
      }
      return COLORS_16m;
    case 'HyperTerm':
    case 'Hyper':
    case 'MacTerm':
      return COLORS_16m;
    case 'Apple_Terminal':
      return COLORS_256;
  }

  if (env.TERM) {
    if (/^xterm-256/.test(env.TERM))
      return COLORS_256;
    if (/^screen|^xterm|^vt100|color|ansi|cygwin|linux/i.test(env.TERM))
      return COLORS_16;
  }

  if (env.COLORTERM)
    return COLORS_16;

  return COLORS_2;
};

WriteStream.prototype._refreshSize = function() {
  const oldCols = this.columns;
  const oldRows = this.rows;
  const winSize = new Array(2);
  const err = this._handle.getWindowSize(winSize);
  if (err) {
    this.emit('error', _errnoException(err, 'getWindowSize'));
    return;
  }
  const [newCols, newRows] = winSize;
  if (oldCols !== newCols || oldRows !== newRows) {
    this.columns = newCols;
    this.rows = newRows;
    this.emit('resize');
  }
};

// Backwards-compat
WriteStream.prototype.cursorTo = function(x, y) {
  readline.cursorTo(this, x, y);
};
WriteStream.prototype.moveCursor = function(dx, dy) {
  readline.moveCursor(this, dx, dy);
};
WriteStream.prototype.clearLine = function(dir) {
  readline.clearLine(this, dir);
};
WriteStream.prototype.clearScreenDown = function() {
  readline.clearScreenDown(this);
};
WriteStream.prototype.getWindowSize = function() {
  return [this.columns, this.rows];
};

module.exports = { isatty, ReadStream, WriteStream };
