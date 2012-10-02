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

module.exports = Transform;

var Duplex = require('_stream_duplex');
var util = require('util');
util.inherits(Transform, Duplex);

function Transform(options) {
  Duplex.call(this, options);

  // bind output so that it can be passed around as a regular function.
  this._output = this._output.bind(this);

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
  this._transform(chunk, this._output, cb);
};

Transform.prototype._read = function(n, cb) {
  var ws = this._writableState;
  var rs = this._readableState;

  // basically a no-op, since the _transform will fill the
  // _readableState.buffer and emit 'readable' for us, and set ended
  // Usually, we want to just not call the cb, and set the reading
  // flag to false, so that another _read will happen next time,
  // but no state changes.
  rs.reading = false;

  // however, if the writable side has ended, and its buffer is clear,
  // then that means that the input has all been consumed, and no more
  // will ever be provide.  treat this as an EOF, and pass back 0 bytes.
  if ((ws.ended || ws.ending) && ws.length === 0)
    cb();
};

Transform.prototype._output = function(chunk) {
  if (!chunk || !chunk.length)
    return;

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

  rs.ended = true;
  // we may have gotten a 'null' read before, and since there is
  // no more data coming from the writable side, we need to emit
  // now so that the consumer knows to pick up the tail bits.
  if (rs.length && rs.needReadable)
    this.emit('readable');
  else if (rs.length === 0) {
    this.emit('end');
  }
}
