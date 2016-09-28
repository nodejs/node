'use strict';

const Buffer = require('buffer').Buffer;
const Writable = require('stream').Writable;
const fs = require('fs');
const util = require('util');

function assertEncoding(encoding) {
  if (encoding && !Buffer.isEncoding(encoding)) {
    throw new Error(`Unknown encoding: ${encoding}`);
  }
}
exports.assertEncoding = assertEncoding;

// Temporary hack for process.stdout and process.stderr when piped to files.
function SyncWriteStream(fd, options) {
  Writable.call(this);

  options = options || {};

  this.fd = fd;
  this.readable = false;
  this.autoClose = options.autoClose === undefined ? true : options.autoClose;

  this.on('end', () => this._destroy());
}

util.inherits(SyncWriteStream, Writable);

SyncWriteStream.prototype._write = function(chunk, encoding, cb) {
  fs.writeSync(this.fd, chunk, 0, chunk.length);
  cb();
  return true;
};

SyncWriteStream.prototype._destroy = function() {
  if (this.fd === null) // already destroy()ed
    return;

  if (this.autoClose)
    fs.closeSync(this.fd);

  this.fd = null;
  return true;
};

SyncWriteStream.prototype.destroySoon =
SyncWriteStream.prototype.destroy = function() {
  this._destroy();
  this.emit('close');
  return true;
};

exports.SyncWriteStream = SyncWriteStream;
