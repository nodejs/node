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


var binding = process.binding('stdio'),
    Stream = require('stream').Stream,
    inherits = require('util').inherits,
    writeTTY = binding.writeTTY,
    closeTTY = binding.closeTTY;


function ReadStream(fd) {
  if (!(this instanceof ReadStream)) return new ReadStream(fd);
  Stream.call(this);

  var self = this;
  this.fd = fd;
  this.paused = true;
  this.readable = true;

  var dataListeners = this.listeners('data'),
      dataUseString = false;

  function onError(err) {
    self.emit('error', err);
  }
  function onKeypress(char, key) {
    self.emit('keypress', char, key);
    if (dataListeners.length && char) {
      self.emit('data', dataUseString ? char : new Buffer(char, 'utf-8'));
    }
  }
  function onResize() {
    process.emit('SIGWINCH');
  }

  // A windows console stream is always UTF-16, actually
  // So it makes no sense to set the encoding, however someone could call
  // setEncoding to tell us he wants strings not buffers
  this.setEncoding = function(encoding) {
    dataUseString = !!encoding;
  }

  binding.initTTYWatcher(fd, onError, onKeypress, onResize);
}
inherits(ReadStream, Stream);
exports.ReadStream = ReadStream;

ReadStream.prototype.isTTY = true;

ReadStream.prototype.pause = function() {
  if (this.paused)
    return;

  this.paused = true;
  binding.stopTTYWatcher();
};

ReadStream.prototype.resume = function() {
  if (!this.readable)
    throw new Error('Cannot resume() closed tty.ReadStream.');
  if (!this.paused)
    return;

  this.paused = false;
  binding.startTTYWatcher();
};

ReadStream.prototype.destroy = function() {
  if (!this.readable)
    return;

  this.readable = false;
  binding.destroyTTYWatcher();

  var self = this;
  process.nextTick(function() {
    try {
      closeTTY(self.fd);
      self.emit('close');
    } catch (err) {
      self.emit('error', err);
    }
  });
};

ReadStream.prototype.destroySoon = ReadStream.prototype.destroy;


function WriteStream(fd) {
  if (!(this instanceof WriteStream)) return new WriteStream(fd);
  Stream.call(this);

  var self = this;
  this.fd = fd;
  this.writable = true;
}
inherits(WriteStream, Stream);
exports.WriteStream = WriteStream;

WriteStream.prototype.isTTY = true;

WriteStream.prototype.write = function(data, encoding) {
  if (!this.writable) {
    throw new Error('stream not writable');
  }

  if (Buffer.isBuffer(data)) {
    data = data.toString('utf-8');
  }

  try {
    writeTTY(this.fd, data);
  } catch (err) {
    this.emit('error', err);
  }
  return true;
};

WriteStream.prototype.end = function(data, encoding) {
  if (data) {
    this.write(data, encoding);
  }
  this.destroy();
};

WriteStream.prototype.destroy = function() {
  if (!this.writable)
    return;

  this.writable = false;

  var self = this;
  process.nextTick(function() {
    var closed = false;
    try {
      closeTTY(self.fd);
      closed = true;
    } catch (err) {
      self.emit('error', err);
    }
    if (closed) {
      self.emit('close');
    }
  });
};

WriteStream.prototype.moveCursor = function(dx, dy) {
  if (!this.writable)
    throw new Error('Stream not writable');

  binding.moveCursor(this.fd, dx, dy);
};

WriteStream.prototype.cursorTo = function(x, y) {
  if (!this.writable)
    throw new Error('Stream not writable');

  binding.cursorTo(this.fd, x, y);
};

WriteStream.prototype.clearLine = function(direction) {
  if (!this.writable)
    throw new Error('Stream not writable');

  binding.clearLine(this.fd, direction || 0);
};
