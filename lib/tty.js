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

var assert = require('assert');
var inherits = require('util').inherits;
var net = require('net');
var TTY = process.binding('tty_wrap').TTY;
var isTTY = process.binding('tty_wrap').isTTY;

exports.isatty = function(fd) {
  return isTTY(fd);
};


// backwards-compat
exports.setRawMode = function(flag) {
  if (!process.stdin.isTTY) {
    throw new Error('can\'t set raw mode on non-tty');
  }
  process.stdin.setRawMode(flag);
};


function ReadStream(fd) {
  if (!(this instanceof ReadStream)) return new ReadStream(fd);
  net.Socket.call(this, {
    handle: new TTY(fd, true)
  });

  this.readable = true;
  this.writable = false;
  this.isRaw = false;
  // backwards-compat
  require('readline').emitKeypressEvents(this);
}
inherits(ReadStream, net.Socket);

exports.ReadStream = ReadStream;

ReadStream.prototype.pause = function() {
  this._handle.unref();
  return net.Socket.prototype.pause.call(this);
};

ReadStream.prototype.resume = function() {
  this._handle.ref();
  return net.Socket.prototype.resume.call(this);
};

ReadStream.prototype.setRawMode = function(flag) {
  flag = !!flag;
  this._handle.setRawMode(flag);
  this.isRaw = flag;
};

ReadStream.prototype.isTTY = true;



function WriteStream(fd) {
  if (!(this instanceof WriteStream)) return new WriteStream(fd);
  net.Socket.call(this, {
    handle: new TTY(fd, false)
  });

  this.readable = false;
  this.writable = true;

  var winSize = this._handle.getWindowSize();
  this.columns = winSize[0];
  this.rows = winSize[1];
}
inherits(WriteStream, net.Socket);
exports.WriteStream = WriteStream;


WriteStream.prototype.isTTY = true;


WriteStream.prototype._refreshSize = function() {
  var oldCols = this.columns;
  var oldRows = this.rows;
  var winSize = this._handle.getWindowSize();
  var newCols = winSize[0];
  var newRows = winSize[1];
  if (oldCols !== newCols || oldRows !== newRows) {
    this.columns = newCols;
    this.rows = newRows;
    this.emit('resize');
  }
};


// backwards-compat
WriteStream.prototype.cursorTo = function(x, y) {
  require('readline').cursorTo(this, x, y);
};
WriteStream.prototype.moveCursor = function(dx, dy) {
  require('readline').moveCursor(this, dx, dy);
};
WriteStream.prototype.clearLine = function(dir) {
  require('readline').clearLine(this, dir);
};
WriteStream.prototype.clearScreenDown = function() {
  require('readline').clearScreenDown(this);
};
WriteStream.prototype.getWindowSize = function() {
  return [this.columns, this.rows];
};
