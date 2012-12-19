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

// A bit simpler than readable streams.
// Implement an async ._write(chunk, cb), and it'll handle all
// the drain event emission and buffering.

module.exports = Writable;
Writable.WritableState = WritableState;

var util = require('util');
var assert = require('assert');
var Stream = require('stream');

util.inherits(Writable, Stream);

function WritableState(options, stream) {
  options = options || {};

  // the point at which write() starts returning false
  this.highWaterMark = options.hasOwnProperty('highWaterMark') ?
      options.highWaterMark : 16 * 1024;

  // the point that it has to get to before we call _write(chunk,cb)
  // default to pushing everything out as fast as possible.
  this.lowWaterMark = options.hasOwnProperty('lowWaterMark') ?
      options.lowWaterMark : 0;

  // cast to ints.
  assert(typeof this.lowWaterMark === 'number');
  assert(typeof this.highWaterMark === 'number');
  this.lowWaterMark = ~~this.lowWaterMark;
  this.highWaterMark = ~~this.highWaterMark;
  assert(this.lowWaterMark >= 0);
  assert(this.highWaterMark >= this.lowWaterMark,
         this.highWaterMark + '>=' + this.lowWaterMark);

  this.needDrain = false;
  // at the start of calling end()
  this.ending = false;
  // when end() has been called, and returned
  this.ended = false;
  // when 'finish' has emitted
  this.finished = false;
  // when 'finish' is being emitted
  this.finishing = false;

  // should we decode strings into buffers before passing to _write?
  // this is here so that some node-core streams can optimize string
  // handling at a lower level.
  this.decodeStrings = options.hasOwnProperty('decodeStrings') ?
      options.decodeStrings : true;

  // not an actual buffer we keep track of, but a measurement
  // of how much we're waiting to get pushed to some underlying
  // socket or file.
  this.length = 0;

  // a flag to see when we're in the middle of a write.
  this.writing = false;

  // a flag to be able to tell if the onwrite cb is called immediately,
  // or on a later tick.
  this.sync = false;

  // a flag to know if we're processing previously buffered items, which
  // may call the _write() callback in the same tick, so that we don't
  // end up in an overlapped onwrite situation.
  this.bufferProcessing = false;

  // the callback that's passed to _write(chunk,cb)
  this.onwrite = function(er) {
    onwrite(stream, er);
  };

  // the callback that the user supplies to write(chunk,encoding,cb)
  this.writecb = null;

  // the amount that is being written when _write is called.
  this.writelen = 0;

  this.buffer = [];
}

function Writable(options) {
  // Writable ctor is applied to Duplexes, though they're not
  // instanceof Writable, they're instanceof Readable.
  if (!(this instanceof Writable) && !(this instanceof Stream.Duplex))
    return new Writable(options);

  this._writableState = new WritableState(options, this);

  // legacy.
  this.writable = true;

  Stream.call(this);
}

// Override this method or _write(chunk, cb)
Writable.prototype.write = function(chunk, encoding, cb) {
  var state = this._writableState;

  if (typeof encoding === 'function') {
    cb = encoding;
    encoding = null;
  }

  if (state.ended) {
    var er = new Error('write after end');
    if (typeof cb === 'function')
      cb(er);
    this.emit('error', er);
    return;
  }

  var l = chunk.length;
  if (false === state.decodeStrings)
    chunk = [chunk, encoding || 'utf8'];
  else if (typeof chunk === 'string' || encoding) {
    chunk = new Buffer(chunk + '', encoding);
    l = chunk.length;
  }

  state.length += l;

  var ret = state.length < state.highWaterMark;
  if (ret === false)
    state.needDrain = true;

  // if we're already writing something, then just put this
  // in the queue, and wait our turn.
  if (state.writing) {
    state.buffer.push([chunk, cb]);
    return ret;
  }

  state.writing = true;
  state.sync = true;
  state.writelen = l;
  state.writecb = cb;
  this._write(chunk, state.onwrite);
  state.sync = false;

  return ret;
};

function onwrite(stream, er) {
  var state = stream._writableState;
  var sync = state.sync;
  var cb = state.writecb;
  var l = state.writelen;

  state.writing = false;
  state.writelen = null;
  state.writecb = null;

  if (er) {
    if (cb) {
      // If _write(chunk,cb) calls cb() in this tick, we still defer
      // the *user's* write callback to the next tick.
      // Never present an external API that is *sometimes* async!
      if (sync)
        process.nextTick(function() {
          cb(er);
        });
      else
        cb(er);
    }

    // backwards compatibility.  still emit if there was a cb.
    stream.emit('error', er);
    return;
  }
  state.length -= l;

  if (cb) {
    // Don't call the cb until the next tick if we're in sync mode.
    if (sync)
      process.nextTick(cb);
    else
      cb();
  }

  if (state.length === 0 && (state.ended || state.ending) &&
      !state.finished && !state.finishing) {
    // emit 'finish' at the very end.
    state.finishing = true;
    stream.emit('finish');
    state.finished = true;
    return;
  }

  if (state.length <= state.lowWaterMark && state.needDrain) {
    // Must force callback to be called on nextTick, so that we don't
    // emit 'drain' before the write() consumer gets the 'false' return
    // value, and has a chance to attach a 'drain' listener.
    process.nextTick(function() {
      if (!state.needDrain)
        return;
      state.needDrain = false;
      stream.emit('drain');
    });
  }
}

Writable.prototype._write = function(chunk, cb) {
  process.nextTick(function() {
    cb(new Error('not implemented'));
  });
};

Writable.prototype.end = function(chunk, encoding) {
  var state = this._writableState;

  // ignore unnecessary end() calls.
  if (state.ending || state.ended || state.finished)
    return;

  state.ending = true;
  if (chunk)
    this.write(chunk, encoding);
  else if (state.length === 0 && !state.finishing && !state.finished) {
    state.finishing = true;
    this.emit('finish');
    state.finished = true;
  }
  state.ended = true;
};
