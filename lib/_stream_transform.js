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


// a transform stream is a readable/writable stream where you do
// something with the data.  Sometimes it's called a "filter",
// but that's not a great name for it, since that implies a thing where
// some bits pass through, and others are simply ignored.  (That would
// be a valid example of a transform, of course.)
//
// While the output is causally related to the input, it's not a
// necessarily symmetric or synchronous transformation.  For example,
// a zlib stream might take multiple plain-text writes(), and then
// emit a single compressed chunk some time in the future.
//
// Here's how this works:
//
// The Transform stream has all the aspects of the readable and writable
// stream classes.  When you write(chunk), that calls _write(chunk,cb)
// internally, and returns false if there's a lot of pending writes
// buffered up.  When you call read(), that calls _read(n,cb) until
// there's enough pending readable data buffered up.
//
// In a transform stream, the written data is placed in a buffer.  When
// _read(n,cb) is called, it transforms the queued up data, calling the
// buffered _write cb's as it consumes chunks.  If consuming a single
// written chunk would result in multiple output chunks, then the first
// outputted bit calls the readcb, and subsequent chunks just go into
// the read buffer, and will cause it to emit 'readable' if necessary.
//
// This way, back-pressure is actually determined by the reading side,
// since _read has to be called to start processing a new chunk.  However,
// a pathological inflate type of transform can cause excessive buffering
// here.  For example, imagine a stream where every byte of input is
// interpreted as an integer from 0-255, and then results in that many
// bytes of output.  Writing the 4 bytes {ff,ff,ff,ff} would result in
// 1kb of data being output.  In this case, you could write a very small
// amount of input, and end up with a very large amount of output.  In
// such a pathological inflating mechanism, there'd be no way to tell
// the system to stop doing the transform.  A single 4MB write could
// cause the system to run out of memory.
//
// However, even in such a pathological case, only a single written chunk
// would be consumed, and then the rest would wait (un-transformed) until
// the results of the previous transformed chunk were consumed.  Because
// the transform happens on-demand, it will only transform as much as is
// necessary to fill the readable buffer to the specified lowWaterMark.

module.exports = Transform;

var Duplex = require('_stream_duplex');
var util = require('util');
util.inherits(Transform, Duplex);

function TransformState() {
  this.buffer = [];
  this.transforming = false;
  this.pendingReadCb = null;
}

function Transform(options) {
  if (!(this instanceof Transform))
    return new Transform(options);

  Duplex.call(this, options);

  // bind output so that it can be passed around as a regular function.
  this._output = this._output.bind(this);

  // the queue of _write chunks that are pending being transformed
  this._transformState = new TransformState();

  // when the writable side finishes, then flush out anything remaining.
  this.once('finish', function() {
    if ('function' === typeof this._flush)
      this._flush(this._output, done.bind(this));
    else
      done.call(this);
  });
}

// This is the part where you do stuff!
// override this function in implementation classes.
// 'chunk' is an input chunk.
//
// Call `output(newChunk)` to pass along transformed output
// to the readable side.  You may call 'output' zero or more times.
//
// Call `cb(err)` when you are done with this chunk.  If you pass
// an error, then that'll put the hurt on the whole operation.  If you
// never call cb(), then you'll never get another chunk.
Transform.prototype._transform = function(chunk, output, cb) {
  throw new Error('not implemented');
};

Transform.prototype._write = function(chunk, cb) {
  var ts = this._transformState;
  var rs = this._readableState;
  ts.buffer.push([chunk, cb]);

  // no need for auto-pull if already in the midst of one.
  if (ts.transforming)
    return;

  // now we have something to transform, if we were waiting for it.
  // kick off a _read to pull it in.
  if (ts.pendingReadCb) {
    var readcb = ts.pendingReadCb;
    ts.pendingReadCb = null;
    this._read(0, readcb);
  }

  // if we weren't waiting for it, but nothing is queued up, then
  // still kick off a transform, just so it's there when the user asks.
  var doRead = rs.needReadable || rs.length <= rs.highWaterMark;
  if (doRead && !rs.reading) {
    var ret = this.read(0);
    if (ret !== null)
      return cb(new Error('invalid stream transform state'));
  }
};

Transform.prototype._read = function(n, readcb) {
  var ws = this._writableState;
  var rs = this._readableState;
  var ts = this._transformState;

  if (ts.pendingReadCb)
    throw new Error('_read while _read already in progress');

  ts.pendingReadCb = readcb;

  // if there's nothing pending, then we just wait.
  // if we're already transforming, then also just hold on a sec.
  // we've already stashed the readcb, so we can come back later
  // when we have something to transform
  if (ts.buffer.length === 0 || ts.transforming)
    return;

  // go ahead and transform that thing, now that someone wants it
  var req = ts.buffer.shift();
  var chunk = req[0];
  var writecb = req[1];
  var output = this._output;
  ts.transforming = true;
  this._transform(chunk, output, function(er, data) {
    ts.transforming = false;
    if (data)
      output(data);
    writecb(er);
  }.bind(this));
};

Transform.prototype._output = function(chunk) {
  if (!chunk || !chunk.length)
    return;

  // if we've got a pending readcb, then just call that,
  // and let Readable take care of it.  If not, then we fill
  // the readable buffer ourselves, and emit whatever's needed.
  var ts = this._transformState;
  var readcb = ts.pendingReadCb;
  if (readcb) {
    ts.pendingReadCb = null;
    readcb(null, chunk);
    return;
  }

  // otherwise, it's up to us to fill the rs buffer.
  var state = this._readableState;
  var len = state.length;
  state.buffer.push(chunk);
  state.length += chunk.length;
  if (state.needReadable) {
    state.needReadable = false;
    this.emit('readable');
  }
};

function done(er) {
  if (er)
    return this.emit('error', er);

  // if there's nothing in the write buffer, then that means
  // that nothing more will ever be provided
  var ws = this._writableState;
  var rs = this._readableState;
  var ts = this._transformState;

  if (ws.length)
    throw new Error('calling transform done when ws.length != 0');

  if (ts.transforming)
    throw new Error('calling transform done when still transforming');

  // if we were waiting on a read, let them know that it isn't coming.
  var readcb = ts.pendingReadCb;
  if (readcb)
    return readcb();

  rs.ended = true;
  // we may have gotten a 'null' read before, and since there is
  // no more data coming from the writable side, we need to emit
  // now so that the consumer knows to pick up the tail bits.
  if (rs.length && rs.needReadable)
    this.emit('readable');
  else if (rs.length === 0)
    this.emit('end');
}
