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

// a duplex stream is just a stream that is both readable and writable.
// Since JS doesn't have multiple prototypal inheritance, this class
// prototypally inherits from Readable, and then parasitically from
// Writable.

'use strict';

const {
  ObjectDefineProperties,
  ObjectGetOwnPropertyDescriptor,
  ObjectKeys,
  ObjectSetPrototypeOf,
} = primordials;

module.exports = Duplex;

const Readable = require('_stream_readable');
const Writable = require('_stream_writable');

ObjectSetPrototypeOf(Duplex.prototype, Readable.prototype);
ObjectSetPrototypeOf(Duplex, Readable);

{
  // Allow the keys array to be GC'ed.
  for (const method of ObjectKeys(Writable.prototype)) {
    if (!Duplex.prototype[method])
      Duplex.prototype[method] = Writable.prototype[method];
  }
}

function Duplex(options) {
  if (!(this instanceof Duplex))
    return new Duplex(options);

  Readable.call(this, options);
  Writable.call(this, options);
  this.allowHalfOpen = true;

  if (options) {
    if (options.readable === false)
      this.readable = false;

    if (options.writable === false)
      this.writable = false;

    if (options.allowHalfOpen === false) {
      this.allowHalfOpen = false;
    }
  }
}

ObjectDefineProperties(Duplex.prototype, {
  writable: ObjectGetOwnPropertyDescriptor(Writable.prototype, 'writable'),

  destroyed: {
    get() {
      if (this._readableState === undefined ||
        this._writableState === undefined) {
        return false;
      }
      return this._readableState.destroyed && this._writableState.destroyed;
    },
    set(value) {
      // Backward compatibility, the user is explicitly
      // managing destroyed
      if (this._readableState && this._writableState) {
        this._readableState.destroyed = value;
        this._writableState.destroyed = value;
      }
    }
  },

  writableHighWaterMark: {
    get() {
      return this._writableState && this._writableState.highWaterMark;
    }
  },

  writableBuffer: {
    get() {
      return this._writableState && this._writableState.getBuffer();
    }
  },

  writableLength: {
    get() {
      return this._writableState && this._writableState.length;
    }
  },

  writableFinished: {
    get() {
      return this._writableState ? this._writableState.finished : false;
    }
  },

  writableCorked: {
    get() {
      return this._writableState ? this._writableState.corked : 0;
    }
  },

  writableEnded: {
    get() {
      return this._writableState ? this._writableState.ending : false;
    }
  }
});
