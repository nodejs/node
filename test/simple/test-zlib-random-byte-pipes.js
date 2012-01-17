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

var crypto = require('crypto');
var stream = require('stream');
var Stream = stream.Stream;
var util = require('util');
var assert = require('assert');
var zlib = require('zlib');



// emit random bytes, and keep a shasum
function RandomReadStream(opt) {
  Stream.call(this);

  this.readable = true;
  this._paused = false;
  this._processing = false;

  this._hasher = crypto.createHash('sha1');
  opt = opt || {};

  // base block size.
  opt.block = opt.block || 256 * 1024;

  // total number of bytes to emit
  opt.total = opt.total || 256 * 1024 * 1024;
  this._remaining = opt.total;

  // how variable to make the block sizes
  opt.jitter = opt.jitter || 1024;

  this._opt = opt;

  this._process = this._process.bind(this);

  process.nextTick(this._process);
}

util.inherits(RandomReadStream, Stream);

RandomReadStream.prototype.pause = function() {
  this._paused = true;
  this.emit('pause');
};

RandomReadStream.prototype.resume = function() {
  // console.error("rrs resume");
  this._paused = false;
  this.emit('resume');
  this._process();
};

RandomReadStream.prototype._process = function() {
  if (this._processing) return;
  if (this._paused) return;

  this._processing = true;

  if (!this._remaining) {
    this._hash = this._hasher.digest('hex').toLowerCase().trim();
    this._processing = false;

    this.emit('end');
    return;
  }

  // figure out how many bytes to output
  // if finished, then just emit end.
  var block = this._opt.block;
  var jitter = this._opt.jitter;
  if (jitter) {
    block += Math.ceil(Math.random() * jitter - (jitter / 2));
  }
  block = Math.min(block, this._remaining);
  var buf = new Buffer(block);
  for (var i = 0; i < block; i++) {
    buf[i] = Math.random() * 256;
  }

  this._hasher.update(buf);

  this._remaining -= block;

  console.error('block=%d\nremain=%d\n', block, this._remaining);
  this._processing = false;

  this.emit('data', buf);
  process.nextTick(this._process);
};


// a filter that just verifies a shasum
function HashStream() {
  Stream.call(this);

  this.readable = this.writable = true;
  this._hasher = crypto.createHash('sha1');
}

util.inherits(HashStream, Stream);

HashStream.prototype.write = function(c) {
  // Simulate the way that an fs.ReadStream returns false
  // on *every* write like a jerk, only to resume a
  // moment later.
  this._hasher.update(c);
  process.nextTick(this.resume.bind(this));
  return false;
};

HashStream.prototype.resume = function() {
  this.emit('resume');
  process.nextTick(this.emit.bind(this, 'drain'));
};

HashStream.prototype.end = function(c) {
  if (c) {
    this.write(c);
  }
  this._hash = this._hasher.digest('hex').toLowerCase().trim();
  this.emit('data', this._hash);
  this.emit('end');
};




var inp = new RandomReadStream({ total: 1024, block: 256, jitter: 16 });
var out = new HashStream();
var gzip = zlib.createGzip();
var gunz = zlib.createGunzip();
inp.pipe(gzip).pipe(gunz).pipe(out);

var didSomething = false;
out.on('data', function(c) {
  didSomething = true;
  console.error('hash=%s', c);
  assert.equal(c, inp._hash, 'hashes should match');
});

process.on('exit', function() {
  assert(didSomething, 'should have done something');
});
