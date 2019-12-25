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
const EventEmitter = require('events');

const e = new EventEmitter();

e.once('hello', common.mustCall());

e.emit('hello', 'a', 'b');
e.emit('hello', 'a', 'b');
e.emit('hello', 'a', 'b');
e.emit('hello', 'a', 'b');

function remove() {
  assert.fail('once->foo should not be emitted');
}

e.once('foo', remove);
e.removeListener('foo', remove);
e.emit('foo');

e.once('e', common.mustCall(function() {
  e.emit('e');
}));

e.once('e', common.mustCall());

e.emit('e');

// Verify that the listener must be a function
assert.throws(() => {
  const ee = new EventEmitter();
  ee.once('foo', null);
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
  message: 'The "listener" argument must be of type function. ' +
           'Received null'
});

{
  // once() has different code paths based on the number of arguments being
  // emitted. Verify that all of the cases are covered.
  const maxArgs = 4;

  for (let i = 0; i <= maxArgs; ++i) {
    const ee = new EventEmitter();
    const args = ['foo'];

    for (let j = 0; j < i; ++j)
      args.push(j);

    ee.once('foo', common.mustCall((...params) => {
      assert.deepStrictEqual(params, args.slice(1));
    }));

    EventEmitter.prototype.emit.apply(ee, args);
  }
}
