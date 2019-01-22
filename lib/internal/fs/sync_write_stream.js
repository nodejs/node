'use strict';

const { Writable } = require('stream');
const { closeSync, writeSync } = require('fs');

function SyncWriteStream(fd, options) {
  Writable.call(this);

  options = options || {};

  this.fd = fd;
  this.readable = false;
  this.autoClose = options.autoClose === undefined ? true : options.autoClose;

  this.on('end', () => this._destroy());
}

Object.setPrototypeOf(SyncWriteStream.prototype, Writable.prototype);
Object.setPrototypeOf(SyncWriteStream, Writable);

SyncWriteStream.prototype._write = function(chunk, encoding, cb) {
  while (true) {
    try {
      const n = writeSync(this.fd, chunk, 0, chunk.length);
      if (n !== chunk.length) {
        chunk = chunk.slice(0, n);
      } else {
        break;
      }
    } catch (err) {
      if (err.code !== 'EAGAIN') {
        cb(err);
        break;
      }
    }
  }
  cb();
  return true;
};

SyncWriteStream.prototype._destroy = function() {
  if (this.fd === null) // already destroy()ed
    return;

  if (this.autoClose)
    closeSync(this.fd);

  this.fd = null;
  return true;
};

SyncWriteStream.prototype.destroySoon =
SyncWriteStream.prototype.destroy = function() {
  this._destroy();
  this.emit('close');
  return true;
};

module.exports = SyncWriteStream;
