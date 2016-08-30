'use strict';

const Buffer = require('buffer').Buffer;
const Stream = require('stream').Stream;
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
  Stream.call(this);

  options = options || {};

  this.fd = fd;
  this.writable = true;
  this.readable = false;
  this.autoClose = options.autoClose === undefined ? true : options.autoClose;
}

util.inherits(SyncWriteStream, Stream);

SyncWriteStream.prototype.write = function(data, arg1, arg2) {
  var encoding, cb;

  // parse arguments
  if (arg1) {
    if (typeof arg1 === 'string') {
      encoding = arg1;
      cb = arg2;
    } else if (typeof arg1 === 'function') {
      cb = arg1;
    } else {
      throw new Error('Bad arguments');
    }
  }
  assertEncoding(encoding);

  // Change strings to buffers. SLOW
  if (typeof data === 'string') {
    data = Buffer.from(data, encoding);
  }

  fs.writeSync(this.fd, data, 0, data.length);

  if (cb) {
    process.nextTick(cb);
  }

  return true;
};


SyncWriteStream.prototype.end = function(data, arg1, arg2) {
  if (data) {
    this.write(data, arg1, arg2);
  }
  this.destroy();
};


SyncWriteStream.prototype.destroy = function() {
  if (this.autoClose)
    fs.closeSync(this.fd);
  this.fd = null;
  this.emit('close');
  return true;
};

SyncWriteStream.prototype.destroySoon = SyncWriteStream.prototype.destroy;

exports.SyncWriteStream = SyncWriteStream;
