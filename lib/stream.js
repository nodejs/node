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

const {
  ObjectDefineProperty,
} = primordials;

const {
  defineLazyProperties,
  promisify: { custom: customPromisify },
} = require('internal/util');

const { setDefaultHighWaterMark, getDefaultHighWaterMark } = require('internal/streams/state');
const { destroyer } = require('internal/streams/destroy');
const { eos } = require('internal/streams/end-of-stream');
const internalBuffer = require('internal/buffer');

let promises;
const utils = require('internal/streams/utils');
const { isArrayBufferView, isUint8Array } = require('internal/util/types');

const Stream = module.exports = require('internal/streams/legacy').Stream;

Stream.isDestroyed = utils.isDestroyed;
Stream.isDisturbed = utils.isDisturbed;
Stream.isErrored = utils.isErrored;
Stream.isReadable = utils.isReadable;
Stream.isWritable = utils.isWritable;

Stream.Readable = require('internal/streams/readable');
defineLazyProperties(Stream.Readable.prototype, 'internal/streams/operators', [
  'drop', 'filter', 'flatMap', 'map', 'take',
  'every', 'forEach', 'reduce', 'toArray', 'some', 'find',
], false);
Stream.Writable = require('internal/streams/writable');
defineLazyProperties(Stream, 'internal/streams/lazy', [
  'Duplex', 'Transform', 'PassThrough', 'duplexPair',
]);
defineLazyProperties(Stream, 'internal/streams/pipeline', ['pipeline']);
const { addAbortSignal } = require('internal/streams/add-abort-signal');
Stream.addAbortSignal = addAbortSignal;
Stream.finished = eos;
Stream.destroy = destroyer;
defineLazyProperties(Stream, 'internal/streams/lazy', ['compose']);
Stream.setDefaultHighWaterMark = setDefaultHighWaterMark;
Stream.getDefaultHighWaterMark = getDefaultHighWaterMark;

ObjectDefineProperty(Stream, 'promises', {
  __proto__: null,
  configurable: true,
  enumerable: true,
  get() {
    return promises ??= require('stream/promises');
  },
});

ObjectDefineProperty(eos, customPromisify, {
  __proto__: null,
  enumerable: true,
  get() {
    promises ??= require('stream/promises');
    return promises.finished;
  },
});

// Backwards-compat with node 0.4.x
Stream.Stream = Stream;

Stream._isArrayBufferView = isArrayBufferView;
Stream._isUint8Array = isUint8Array;
Stream._uint8ArrayToBuffer = function _uint8ArrayToBuffer(chunk) {
  return new internalBuffer.FastBuffer(chunk.buffer,
                                       chunk.byteOffset,
                                       chunk.byteLength);
};
