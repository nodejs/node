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

'use strict';

const Buffer = require('buffer').Buffer;

// Note: export Stream before Readable/Writable/Duplex/...
// to avoid a cross-reference(require) issues
const Stream = module.exports = require('internal/streams/legacy');

Stream.Readable = require('_stream_readable');
Stream.Writable = require('_stream_writable');
Stream.Duplex = require('_stream_duplex');
Stream.Transform = require('_stream_transform');
Stream.PassThrough = require('_stream_passthrough');

// Backwards-compat with node 0.4.x
Stream.Stream = Stream;

// Internal utilities
try {
  Stream._isUint8Array = process.binding('util').isUint8Array;
} catch (e) {
  // This throws for Node < 4.2.0 because there’s no util binding and
  // returns undefined for Node < 7.4.0.
}

if (!Stream._isUint8Array) {
  Stream._isUint8Array = function _isUint8Array(obj) {
    return Object.prototype.toString.call(obj) === '[object Uint8Array]';
  };
}

const version = process.version.substr(1).split('.');
if (version[0] === 0 && version[1] < 12) {
  Stream._uint8ArrayToBuffer = Buffer;
} else {
  try {
    const internalBuffer = require('internal/buffer');
    Stream._uint8ArrayToBuffer = function _uint8ArrayToBuffer(chunk) {
      return new internalBuffer.FastBuffer(chunk.buffer,
                                           chunk.byteOffset,
                                           chunk.byteLength);
    };
  } catch (e) {
  }

  if (!Stream._uint8ArrayToBuffer) {
    Stream._uint8ArrayToBuffer = function _uint8ArrayToBuffer(chunk) {
      return Buffer.prototype.slice.call(chunk);
    };
  }
}
