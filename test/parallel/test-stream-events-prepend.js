'use strict';
const common = require('../common');
const stream = require('stream');
const util = require('util');

function Writable() {
  this.writable = true;
  stream.Writable.call(this);
  this.prependListener = undefined;
}
util.inherits(Writable, stream.Writable);
Writable.prototype._write = function(chunk, end, cb) {
  cb();
};

function Readable() {
  this.readable = true;
  stream.Readable.call(this);
}
util.inherits(Readable, stream.Readable);
Readable.prototype._read = function() {
  this.push(null);
};

const w = new Writable();
w.on('pipe', common.mustCall());

const r = new Readable();
r.pipe(w);
