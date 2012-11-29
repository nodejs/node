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

module.exports = Readable;
Readable.ReadableState = ReadableState;

var Stream = require('stream');
var util = require('util');
var assert = require('assert');
var StringDecoder;

util.inherits(Readable, Stream);

function ReadableState(options, stream) {
  options = options || {};

  // cast to an int
  this.bufferSize = ~~this.bufferSize;

  // the argument passed to this._read(n,cb)
  this.bufferSize = options.hasOwnProperty('bufferSize') ?
      options.bufferSize : 16 * 1024;

  // the point at which it stops calling _read() to fill the buffer
  this.highWaterMark = options.hasOwnProperty('highWaterMark') ?
      options.highWaterMark : 16 * 1024;

  // the minimum number of bytes to buffer before emitting 'readable'
  // default to pushing everything out as fast as possible.
  this.lowWaterMark = options.hasOwnProperty('lowWaterMark') ?
      options.lowWaterMark : 0;

  // cast to ints.
  assert(typeof this.bufferSize === 'number');
  assert(typeof this.lowWaterMark === 'number');
  assert(typeof this.highWaterMark === 'number');
  this.bufferSize = ~~this.bufferSize;
  this.lowWaterMark = ~~this.lowWaterMark;
  this.highWaterMark = ~~this.highWaterMark;
  assert(this.bufferSize >= 0);
  assert(this.lowWaterMark >= 0);
  assert(this.highWaterMark >= this.lowWaterMark,
         this.highWaterMark + '>=' + this.lowWaterMark);

  this.buffer = [];
  this.length = 0;
  this.pipes = null;
  this.pipesCount = 0;
  this.flowing = false;
  this.ended = false;
  this.endEmitted = false;
  this.reading = false;
  this.sync = false;
  this.onread = function(er, data) {
    onread(stream, er, data);
  };

  // whenever we return null, then we set a flag to say
  // that we're awaiting a 'readable' event emission.
  this.needReadable = false;
  this.emittedReadable = false;

  // when piping, we only care about 'readable' events that happen
  // after read()ing all the bytes and not getting any pushback.
  this.ranOut = false;

  // the number of writers that are awaiting a drain event in .pipe()s
  this.awaitDrain = 0;
  this.pipeChunkSize = null;

  this.decoder = null;
  if (options.encoding) {
    if (!StringDecoder)
      StringDecoder = require('string_decoder').StringDecoder;
    this.decoder = new StringDecoder(options.encoding);
  }
}

function Readable(options) {
  if (!(this instanceof Readable))
    return new Readable(options);

  this._readableState = new ReadableState(options, this);

  // legacy
  this.readable = true;

  Stream.apply(this);
}

// backwards compatibility.
Readable.prototype.setEncoding = function(enc) {
  if (!StringDecoder)
    StringDecoder = require('string_decoder').StringDecoder;
  this._readableState.decoder = new StringDecoder(enc);
};


function howMuchToRead(n, state) {
  if (state.length === 0 && state.ended)
    return 0;

  if (isNaN(n) || n === null)
    return state.length;

  if (n <= 0)
    return 0;

  // don't have that much.  return null, unless we've ended.
  if (n > state.length) {
    if (!state.ended) {
      state.needReadable = true;
      return 0;
    } else
      return state.length;
  }

  return n;
}

// you can override either this method, or _read(n, cb) below.
Readable.prototype.read = function(n) {
  var state = this._readableState;
  var nOrig = n;

  if (typeof n !== 'number' || n > 0)
    state.emittedReadable = false;

  n = howMuchToRead(n, state);

  // if we've ended, and we're now clear, then finish it up.
  if (n === 0 && state.ended) {
    endReadable(this);
    return null;
  }

  // All the actual chunk generation logic needs to be
  // *below* the call to _read.  The reason is that in certain
  // synthetic stream cases, such as passthrough streams, _read
  // may be a completely synchronous operation which may change
  // the state of the read buffer, providing enough data when
  // before there was *not* enough.
  //
  // So, the steps are:
  // 1. Figure out what the state of things will be after we do
  // a read from the buffer.
  //
  // 2. If that resulting state will trigger a _read, then call _read.
  // Note that this may be asynchronous, or synchronous.  Yes, it is
  // deeply ugly to write APIs this way, but that still doesn't mean
  // that the Readable class should behave improperly, as streams are
  // designed to be sync/async agnostic.
  // Take note if the _read call is sync or async (ie, if the read call
  // has returned yet), so that we know whether or not it's safe to emit
  // 'readable' etc.
  //
  // 3. Actually pull the requested chunks out of the buffer and return.

  // if we need a readable event, then we need to do some reading.
  var doRead = state.needReadable;

  // if we currently have less than the highWaterMark, then also read some
  if (state.length - n <= state.highWaterMark)
    doRead = true;

  // if we currently have *nothing*, then always try to get *something*
  // no matter what the high water mark says.
  if (state.length === 0)
    doRead = true;

  // however, if we've ended, then there's no point, and if we're already
  // reading, then it's unnecessary.
  if (state.ended || state.reading)
    doRead = false;

  if (doRead) {
    var sync = true;
    state.reading = true;
    state.sync = true;
    // call internal read method
    this._read(state.bufferSize, state.onread);
    state.sync = false;
  }

  // If _read called its callback synchronously, then `reading`
  // will be false, and we need to re-evaluate how much data we
  // can return to the user.
  if (doRead && !state.reading)
    n = howMuchToRead(nOrig, state);

  var ret;
  if (n > 0)
    ret = fromList(n, state.buffer, state.length, !!state.decoder);
  else
    ret = null;

  if (ret === null || ret.length === 0) {
    state.needReadable = true;
    n = 0;
  }

  state.length -= n;

  return ret;
};

function onread(stream, er, chunk) {
  var state = stream._readableState;
  var sync = state.sync;

  state.reading = false;
  if (er)
    return stream.emit('error', er);

  if (!chunk || !chunk.length) {
    // eof
    state.ended = true;
    if (state.decoder) {
      chunk = state.decoder.end();
      if (chunk && chunk.length) {
        state.buffer.push(chunk);
        state.length += chunk.length;
      }
    }
    // if we've ended and we have some data left, then emit
    // 'readable' now to make sure it gets picked up.
    if (!sync) {
      if (state.length > 0) {
        state.needReadable = false;
        if (!state.emittedReadable) {
          state.emittedReadable = true;
          stream.emit('readable');
        }
      } else
        endReadable(stream);
    }
    return;
  }

  if (state.decoder)
    chunk = state.decoder.write(chunk);

  // update the buffer info.
  if (chunk) {
    state.length += chunk.length;
    state.buffer.push(chunk);
  }

  // if we haven't gotten enough to pass the lowWaterMark,
  // and we haven't ended, then don't bother telling the user
  // that it's time to read more data.  Otherwise, emitting 'readable'
  // probably will trigger another stream.read(), which can trigger
  // another _read(n,cb) before this one returns!
  if (state.length <= state.lowWaterMark) {
    state.reading = true;
    stream._read(state.bufferSize, state.onread);
    return;
  }

  if (state.needReadable && !sync) {
    state.needReadable = false;
    if (!state.emittedReadable) {
      state.emittedReadable = true;
      stream.emit('readable');
    }
  }
}

// abstract method.  to be overridden in specific implementation classes.
// call cb(er, data) where data is <= n in length.
// for virtual (non-string, non-buffer) streams, "length" is somewhat
// arbitrary, and perhaps not very meaningful.
Readable.prototype._read = function(n, cb) {
  process.nextTick(function() {
    cb(new Error('not implemented'));
  });
};

Readable.prototype.pipe = function(dest, pipeOpts) {
  var src = this;
  var state = this._readableState;

  switch (state.pipesCount) {
    case 0:
      state.pipes = dest;
      break;
    case 1:
      state.pipes = [ state.pipes, dest ];
      break;
    default:
      state.pipes.push(dest);
      break;
  }
  state.pipesCount += 1;

  if ((!pipeOpts || pipeOpts.end !== false) &&
      dest !== process.stdout &&
      dest !== process.stderr) {
    src.once('end', onend);
    dest.on('unpipe', function(readable) {
      if (readable === src)
        src.removeListener('end', onend);
    });
  }

  if (pipeOpts && pipeOpts.chunkSize)
    state.pipeChunkSize = pipeOpts.chunkSize;

  function onend() {
    dest.end();
  }

  // when the dest drains, it reduces the awaitDrain counter
  // on the source.  This would be more elegant with a .once()
  // handler in flow(), but adding and removing repeatedly is
  // too slow.
  var ondrain = pipeOnDrain(src);
  dest.on('drain', ondrain);
  dest.on('unpipe', function(readable) {
    if (readable === src)
      dest.removeListener('drain', ondrain);

    // if the reader is waiting for a drain event from this
    // specific writer, then it would cause it to never start
    // flowing again.
    // So, if this is awaiting a drain, then we just call it now.
    // If we don't know, then assume that we are waiting for one.
    if (!dest._writableState || dest._writableState.needDrain)
      ondrain();
  });

  // if the dest has an error, then stop piping into it.
  // however, don't suppress the throwing behavior for this.
  dest.once('error', function(er) {
    unpipe();
    if (dest.listeners('error').length === 0)
      dest.emit('error', er);
  });

  // if the dest emits close, then presumably there's no point writing
  // to it any more.
  dest.on('close', unpipe);
  dest.on('finish', function() {
    dest.removeListener('close', unpipe);
  });

  function unpipe() {
    src.unpipe(dest);
  }

  // tell the dest that it's being piped to
  dest.emit('pipe', src);

  // start the flow if it hasn't been started already.
  if (!state.flowing) {
    // the handler that waits for readable events after all
    // the data gets sucked out in flow.
    // This would be easier to follow with a .once() handler
    // in flow(), but that is too slow.
    this.on('readable', pipeOnReadable);

    state.flowing = true;
    process.nextTick(function() {
      flow(src);
    });
  }

  return dest;
};

function pipeOnDrain(src) {
  return function() {
    var dest = this;
    var state = src._readableState;
    state.awaitDrain --;
    if (state.awaitDrain === 0)
      flow(src);
  };
}

function flow(src) {
  var state = src._readableState;
  var chunk;
  state.awaitDrain = 0;

  function write(dest, i, list) {
    var written = dest.write(chunk);
    if (false === written) {
      state.awaitDrain++;
    }
  }

  while (state.pipesCount &&
         null !== (chunk = src.read(state.pipeChunkSize))) {

    if (state.pipesCount === 1)
      write(state.pipes, 0, null);
    else
      state.pipes.forEach(write);

    src.emit('data', chunk);

    // if anyone needs a drain, then we have to wait for that.
    if (state.awaitDrain > 0)
      return;
  }

  // if every destination was unpiped, either before entering this
  // function, or in the while loop, then stop flowing.
  //
  // NB: This is a pretty rare edge case.
  if (state.pipesCount === 0) {
    state.flowing = false;

    // if there were data event listeners added, then switch to old mode.
    if (src.listeners('data').length)
      emitDataEvents(src);
    return;
  }

  // at this point, no one needed a drain, so we just ran out of data
  // on the next readable event, start it over again.
  state.ranOut = true;
}

function pipeOnReadable() {
  if (this._readableState.ranOut) {
    this._readableState.ranOut = false;
    flow(this);
  }
}


Readable.prototype.unpipe = function(dest) {
  var state = this._readableState;

  // if we're not piping anywhere, then do nothing.
  if (state.pipesCount === 0)
    return this;

  // just one destination.  most common case.
  if (state.pipesCount === 1) {
    // passed in one, but it's not the right one.
    if (dest && dest !== state.pipes)
      return this;

    if (!dest)
      dest = state.pipes;

    // got a match.
    state.pipes = null;
    state.pipesCount = 0;
    this.removeListener('readable', pipeOnReadable);
    if (dest)
      dest.emit('unpipe', this);
    return this;
  }

  // slow case. multiple pipe destinations.

  if (!dest) {
    // remove all.
    var dests = state.pipes;
    var len = state.pipesCount;
    state.pipes = null;
    state.pipesCount = 0;
    this.removeListener('readable', pipeOnReadable);

    for (var i = 0; i < len; i++)
      dests[i].emit('unpipe', this);
    return this;
  }

  // try to find the right one.
  var i = state.pipes.indexOf(dest);
  if (i === -1)
    return this;

  state.pipes.splice(i, 1);
  state.pipesCount -= 1;
  if (state.pipesCount === 1)
    state.pipes = state.pipes[0];

  dest.emit('unpipe', this);

  return this;
};

// kludge for on('data', fn) consumers.  Sad.
// This is *not* part of the new readable stream interface.
// It is an ugly unfortunate mess of history.
Readable.prototype.on = function(ev, fn) {
  // https://github.com/isaacs/readable-stream/issues/16
  // if we're already flowing, then no need to set up data events.
  if (ev === 'data' && !this._readableState.flowing)
    emitDataEvents(this);

  return Stream.prototype.on.call(this, ev, fn);
};
Readable.prototype.addListener = Readable.prototype.on;

// pause() and resume() are remnants of the legacy readable stream API
// If the user uses them, then switch into old mode.
Readable.prototype.resume = function() {
  emitDataEvents(this);
  return this.resume();
};

Readable.prototype.pause = function() {
  emitDataEvents(this);
  return this.pause();
};

function emitDataEvents(stream) {
  var state = stream._readableState;

  if (state.flowing) {
    // https://github.com/isaacs/readable-stream/issues/16
    throw new Error('Cannot switch to old mode now.');
  }

  var paused = false;
  var readable = false;

  // convert to an old-style stream.
  stream.readable = true;
  stream.pipe = Stream.prototype.pipe;
  stream.on = stream.addEventListener = Stream.prototype.on;

  stream.on('readable', function() {
    readable = true;
    var c;
    while (!paused && (null !== (c = stream.read())))
      stream.emit('data', c);

    if (c === null) {
      readable = false;
      stream._readableState.needReadable = true;
    }
  });

  stream.pause = function() {
    paused = true;
  };

  stream.resume = function() {
    paused = false;
    if (readable)
      stream.emit('readable');
  };

  // now make it start, just in case it hadn't already.
  process.nextTick(function() {
    stream.emit('readable');
  });
}

// wrap an old-style stream as the async data source.
// This is *not* part of the readable stream interface.
// It is an ugly unfortunate mess of history.
Readable.prototype.wrap = function(stream) {
  var state = this._readableState;
  var paused = false;

  var self = this;
  stream.on('end', function() {
    state.ended = true;
    if (state.decoder) {
      var chunk = state.decoder.end();
      if (chunk && chunk.length) {
        state.buffer.push(chunk);
        state.length += chunk.length;
      }
    }

    if (state.length > 0)
      self.emit('readable');
    else
      endReadable(self);
  });

  stream.on('data', function(chunk) {
    if (state.decoder)
      chunk = state.decoder.write(chunk);
    if (!chunk || !chunk.length)
      return;

    state.buffer.push(chunk);
    state.length += chunk.length;
    self.emit('readable');

    // if not consumed, then pause the stream.
    if (state.length > state.lowWaterMark && !paused) {
      paused = true;
      stream.pause();
    }
  });

  // proxy all the other methods.
  // important when wrapping filters and duplexes.
  for (var i in stream) {
    if (typeof stream[i] === 'function' &&
        typeof this[i] === 'undefined') {
      this[i] = function(method) { return function() {
        return stream[method].apply(stream, arguments);
      }}(i);
    }
  }

  // proxy certain important events.
  var events = ['error', 'close', 'destroy', 'pause', 'resume'];
  events.forEach(function(ev) {
    stream.on(ev, self.emit.bind(self, ev));
  });

  // consume some bytes.  if not all is consumed, then
  // pause the underlying stream.
  this.read = function(n) {
    if (state.length === 0) {
      state.needReadable = true;
      return null;
    }

    if (isNaN(n) || n <= 0)
      n = state.length;

    if (n > state.length) {
      if (!state.ended) {
        state.needReadable = true;
        return null;
      } else
        n = state.length;
    }

    var ret = fromList(n, state.buffer, state.length, !!state.decoder);
    state.length -= n;

    if (state.length <= state.lowWaterMark && paused) {
      stream.resume();
      paused = false;
    }

    if (state.length === 0 && state.ended)
      endReadable(this);

    return ret;
  };
};



// exposed for testing purposes only.
Readable._fromList = fromList;

// Pluck off n bytes from an array of buffers.
// Length is the combined lengths of all the buffers in the list.
function fromList(n, list, length, stringMode) {
  var ret;

  // nothing in the list, definitely empty.
  if (list.length === 0) {
    return null;
  }

  if (length === 0)
    ret = null;
  else if (!n || n >= length) {
    // read it all, truncate the array.
    if (stringMode)
      ret = list.join('');
    else
      ret = Buffer.concat(list, length);
    list.length = 0;
  } else {
    // read just some of it.
    if (n < list[0].length) {
      // just take a part of the first list item.
      // slice is the same for buffers and strings.
      var buf = list[0];
      ret = buf.slice(0, n);
      list[0] = buf.slice(n);
    } else if (n === list[0].length) {
      // first list is a perfect match
      ret = list.shift();
    } else {
      // complex case.
      // we have enough to cover it, but it spans past the first buffer.
      if (stringMode)
        ret = '';
      else
        ret = new Buffer(n);

      var c = 0;
      for (var i = 0, l = list.length; i < l && c < n; i++) {
        var buf = list[0];
        var cpy = Math.min(n - c, buf.length);

        if (stringMode)
          ret += buf.slice(0, cpy);
        else
          buf.copy(ret, c, 0, cpy);

        if (cpy < buf.length)
          list[0] = buf.slice(cpy);
        else
          list.shift();

        c += cpy;
      }
    }
  }

  return ret;
}

function endReadable(stream) {
  var state = stream._readableState;
  if (state.endEmitted)
    return;
  state.ended = true;
  state.endEmitted = true;
  process.nextTick(function() {
    stream.emit('end');
  });
}
