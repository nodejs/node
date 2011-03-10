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

var common = require('../common');
var assert = require('assert');
var events = require('events');

var callbacks_called = [];

var e = new events.EventEmitter();

function callback1() {
  callbacks_called.push('callback1');
  e.addListener('foo', callback2);
  e.addListener('foo', callback3);
  e.removeListener('foo', callback1);
}

function callback2() {
  callbacks_called.push('callback2');
  e.removeListener('foo', callback2);
}

function callback3() {
  callbacks_called.push('callback3');
  e.removeListener('foo', callback3);
}

e.addListener('foo', callback1);
assert.equal(1, e.listeners('foo').length);

e.emit('foo');
assert.equal(2, e.listeners('foo').length);
assert.deepEqual(['callback1'], callbacks_called);

e.emit('foo');
assert.equal(0, e.listeners('foo').length);
assert.deepEqual(['callback1', 'callback2', 'callback3'], callbacks_called);

e.emit('foo');
assert.equal(0, e.listeners('foo').length);
assert.deepEqual(['callback1', 'callback2', 'callback3'], callbacks_called);

e.addListener('foo', callback1);
e.addListener('foo', callback2);
assert.equal(2, e.listeners('foo').length);
e.removeAllListeners('foo');
assert.equal(0, e.listeners('foo').length);

// Verify that removing callbacks while in emit allows emits to propagate to
// all listeners
callbacks_called = [];

e.addListener('foo', callback2);
e.addListener('foo', callback3);
assert.equal(2, e.listeners('foo').length);
e.emit('foo');
assert.deepEqual(['callback2', 'callback3'], callbacks_called);
assert.equal(0, e.listeners('foo').length);
