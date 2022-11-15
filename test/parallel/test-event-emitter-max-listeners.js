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
const common = require('../common');
const assert = require('assert');
const events = require('events');

const throwInvalidType = [true, 'string'];
const throwOutOfRange = [-1, NaN];

// for EventEmitter.prototype.setMaxListeners
{
  const e = new events.EventEmitter();

  e.on('maxListeners', common.mustCall());

  // Should not corrupt the 'maxListeners' queue.
  e.setMaxListeners(42);

  for (const obj of throwInvalidType) {
    assert.throws(
      () => e.setMaxListeners(obj),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
      }
    );
  }

  for (const obj of throwOutOfRange) {
    assert.throws(
      () => e.setMaxListeners(obj),
      {
        code: 'ERR_OUT_OF_RANGE',
        name: 'RangeError',
      }
    );
  }

  e.emit('maxListeners');
}

// for EventEmitter.defaultMaxListeners
{
  for (const obj of throwInvalidType) {
    assert.throws(
      () => events.defaultMaxListeners = obj,
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
      }
    );
  }

  for (const obj of throwOutOfRange) {
    assert.throws(
      () => events.defaultMaxListeners = obj,
      {
        code: 'ERR_OUT_OF_RANGE',
        name: 'RangeError',
      }
    );
  }
}

// for EventEmitter.setMaxListeners
{
  const { EventEmitter, defaultMaxListeners } = events;
  for (const obj of throwInvalidType) {
    assert.throws(
      () => EventEmitter.setMaxListeners(obj),
      { code: 'ERR_INVALID_ARG_TYPE' }
    );
  }

  for (const obj of throwOutOfRange) {
    assert.throws(
      () => EventEmitter.setMaxListeners(obj),
      { code: 'ERR_OUT_OF_RANGE' }
    );
  }

  assert.throws(
    () => EventEmitter.setMaxListeners(defaultMaxListeners, 'INVALID_EMITTER'),
    { code: 'ERR_INVALID_ARG_TYPE' }
  );
}
