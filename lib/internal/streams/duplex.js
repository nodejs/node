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
// Since JS doesn't have multiple prototype inheritance, this class
// prototypically inherits from Readable, and then parasitically from
// Writable.

'use strict';

const {
  ObjectDefineProperties,
  ObjectGetOwnPropertyDescriptor,
  ObjectKeys,
  ObjectSetPrototypeOf,
} = primordials;

module.exports = Duplex;

const Readable = require('internal/streams/readable');
const Writable = require('internal/streams/writable');

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

  if (options) {
    this.allowHalfOpen = options.allowHalfOpen !== false;

    if (options.readable === false) {
      this._readableState.readable = false;
      this._readableState.ended = true;
      this._readableState.endEmitted = true;
    }

    if (options.writable === false) {
      this._writableState.writable = false;
      this._writableState.ending = true;
      this._writableState.ended = true;
      this._writableState.finished = true;
    }
  } else {
    this.allowHalfOpen = true;
  }
}

ObjectDefineProperties(Duplex.prototype, {
  writable:
    ObjectGetOwnPropertyDescriptor(Writable.prototype, 'writable'),
  writableHighWaterMark:
    ObjectGetOwnPropertyDescriptor(Writable.prototype, 'writableHighWaterMark'),
  writableObjectMode:
    ObjectGetOwnPropertyDescriptor(Writable.prototype, 'writableObjectMode'),
  writableBuffer:
    ObjectGetOwnPropertyDescriptor(Writable.prototype, 'writableBuffer'),
  writableLength:
    ObjectGetOwnPropertyDescriptor(Writable.prototype, 'writableLength'),
  writableFinished:
    ObjectGetOwnPropertyDescriptor(Writable.prototype, 'writableFinished'),
  writableCorked:
    ObjectGetOwnPropertyDescriptor(Writable.prototype, 'writableCorked'),
  writableEnded:
    ObjectGetOwnPropertyDescriptor(Writable.prototype, 'writableEnded'),
  writableNeedDrain:
    ObjectGetOwnPropertyDescriptor(Writable.prototype, 'writableNeedDrain'),

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
      // managing destroyed.
      if (this._readableState && this._writableState) {
        this._readableState.destroyed = value;
        this._writableState.destroyed = value;
      }
    }
  }
});
