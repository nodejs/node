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

{
  const ee = new EventEmitter();
  const events_new_listener_emitted = [];
  const listeners_new_listener_emitted = [];

  // Sanity check
  assert.strictEqual(ee.addListener, ee.on);

  ee.on('newListener', function(event, listener) {
    // Don't track newListener listeners.
    if (event === 'newListener')
      return;

    events_new_listener_emitted.push(event);
    listeners_new_listener_emitted.push(listener);
  });

  const hello = common.mustCall(function(a, b) {
    assert.strictEqual('a', a);
    assert.strictEqual('b', b);
  });

  ee.once('newListener', function(name, listener) {
    assert.strictEqual(name, 'hello');
    assert.strictEqual(listener, hello);
    assert.deepStrictEqual(this.listeners('hello'), []);
  });

  ee.on('hello', hello);
  ee.once('foo', assert.fail);
  assert.deepStrictEqual(['hello', 'foo'], events_new_listener_emitted);
  assert.deepStrictEqual([hello, assert.fail], listeners_new_listener_emitted);

  ee.emit('hello', 'a', 'b');
}

// just make sure that this doesn't throw:
{
  const f = new EventEmitter();

  f.setMaxListeners(0);
}

{
  const listen1 = () => {};
  const listen2 = () => {};
  const ee = new EventEmitter();

  ee.once('newListener', function() {
    assert.deepStrictEqual(ee.listeners('hello'), []);
    ee.once('newListener', function() {
      assert.deepStrictEqual(ee.listeners('hello'), []);
    });
    ee.on('hello', listen2);
  });
  ee.on('hello', listen1);
  // The order of listeners on an event is not always the order in which the
  // listeners were added.
  assert.deepStrictEqual(ee.listeners('hello'), [listen2, listen1]);
}

// Verify that the listener must be a function
common.expectsError(() => {
  const ee = new EventEmitter();
  ee.on('foo', null);
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: 'The "listener" argument must be of type Function'
});
