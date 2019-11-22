'use strict';

const {
  ObjectSetPrototypeOf,
} = primordials;

const { Writable } = require('stream');
const { closeSync, writeSync } = require('fs');

function SyncWriteStream(fd, options) {
  Writable.call(this, { autoDestroy: true });

  options = options || {};

  this.fd = fd;
  this.readable = false;
  this.autoClose = options.autoClose === undefined ? true : options.autoClose;
}

ObjectSetPrototypeOf(SyncWriteStream.prototype, Writable.prototype);
ObjectSetPrototypeOf(SyncWriteStream, Writable);

SyncWriteStream.prototype._write = function(chunk, encoding, cb) {
  writeSync(this.fd, chunk, 0, chunk.length);
  cb();
  return true;
};

SyncWriteStream.prototype._destroy = function(err, cb) {
  if (this.fd === null) // already destroy()ed
    return cb(err);

  if (this.autoClose)
    closeSync(this.fd);

  this.fd = null;
  cb(err);
};

SyncWriteStream.prototype.destroySoon =
  SyncWriteStream.prototype.destroy;

module.exports = SyncWriteStream;
