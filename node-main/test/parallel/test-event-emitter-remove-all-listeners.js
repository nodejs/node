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


function expect(expected) {
  const actual = [];
  process.on('exit', function() {
    assert.deepStrictEqual(actual.sort(), expected.sort());
  });
  function listener(name) {
    actual.push(name);
  }
  return common.mustCall(listener, expected.length);
}

{
  const ee = new events.EventEmitter();
  const noop = common.mustNotCall();
  ee.on('foo', noop);
  ee.on('bar', noop);
  ee.on('baz', noop);
  ee.on('baz', noop);
  const fooListeners = ee.listeners('foo');
  const barListeners = ee.listeners('bar');
  const bazListeners = ee.listeners('baz');
  ee.on('removeListener', expect(['bar', 'baz', 'baz']));
  ee.removeAllListeners('bar');
  ee.removeAllListeners('baz');
  assert.deepStrictEqual(ee.listeners('foo'), [noop]);
  assert.deepStrictEqual(ee.listeners('bar'), []);
  assert.deepStrictEqual(ee.listeners('baz'), []);
  // After calling removeAllListeners(),
  // the old listeners array should stay unchanged.
  assert.deepStrictEqual(fooListeners, [noop]);
  assert.deepStrictEqual(barListeners, [noop]);
  assert.deepStrictEqual(bazListeners, [noop, noop]);
  // After calling removeAllListeners(),
  // new listeners arrays is different from the old.
  assert.notStrictEqual(ee.listeners('bar'), barListeners);
  assert.notStrictEqual(ee.listeners('baz'), bazListeners);
}

{
  const ee = new events.EventEmitter();
  ee.on('foo', common.mustNotCall());
  ee.on('bar', common.mustNotCall());
  // Expect LIFO order
  ee.on('removeListener', expect(['foo', 'bar', 'removeListener']));
  ee.on('removeListener', expect(['foo', 'bar']));
  ee.removeAllListeners();
  assert.deepStrictEqual([], ee.listeners('foo'));
  assert.deepStrictEqual([], ee.listeners('bar'));
}

{
  const ee = new events.EventEmitter();
  ee.on('removeListener', common.mustNotCall());
  // Check for regression where removeAllListeners() throws when
  // there exists a 'removeListener' listener, but there exists
  // no listeners for the provided event type.
  ee.removeAllListeners.bind(ee, 'foo');
}

{
  const ee = new events.EventEmitter();
  let expectLength = 2;
  ee.on('removeListener', function(name, noop) {
    assert.strictEqual(expectLength--, this.listeners('baz').length);
  });
  ee.on('baz', common.mustNotCall());
  ee.on('baz', common.mustNotCall());
  ee.on('baz', common.mustNotCall());
  assert.strictEqual(ee.listeners('baz').length, expectLength + 1);
  ee.removeAllListeners('baz');
  assert.strictEqual(ee.listeners('baz').length, 0);
}

{
  const ee = new events.EventEmitter();
  assert.deepStrictEqual(ee, ee.removeAllListeners());
}

{
  const ee = new events.EventEmitter();
  ee._events = undefined;
  assert.strictEqual(ee, ee.removeAllListeners());
}

{
  const ee = new events.EventEmitter();
  const symbol = Symbol('symbol');
  const noop = common.mustNotCall();
  ee.on(symbol, noop);

  ee.on('removeListener', common.mustCall((...args) => {
    assert.deepStrictEqual(args, [symbol, noop]);
  }));

  ee.removeAllListeners();
}
