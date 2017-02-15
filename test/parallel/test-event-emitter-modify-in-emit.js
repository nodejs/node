'use strict';
require('../common');
const assert = require('assert');
const events = require('events');

let callbacks_called = [];

const e = new events.EventEmitter();

function callback1() {
  callbacks_called.push('callback1');
  e.on('foo', callback2);
  e.on('foo', callback3);
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

e.on('foo', callback1);
assert.strictEqual(e.listeners('foo').length, 1);

e.emit('foo');
assert.strictEqual(e.listeners('foo').length, 2);
assert.deepStrictEqual(['callback1'], callbacks_called);

e.emit('foo');
assert.strictEqual(e.listeners('foo').length, 0);
assert.deepStrictEqual(['callback1', 'callback2', 'callback3'],
                       callbacks_called);

e.emit('foo');
assert.strictEqual(e.listeners('foo').length, 0);
assert.deepStrictEqual(['callback1', 'callback2', 'callback3'],
                       callbacks_called);

e.on('foo', callback1);
e.on('foo', callback2);
assert.strictEqual(e.listeners('foo').length, 2);
e.removeAllListeners('foo');
assert.strictEqual(e.listeners('foo').length, 0);

// Verify that removing callbacks while in emit allows emits to propagate to
// all listeners
callbacks_called = [];

e.on('foo', callback2);
e.on('foo', callback3);
assert.strictEqual(2, e.listeners('foo').length);
e.emit('foo');
assert.deepStrictEqual(['callback2', 'callback3'], callbacks_called);
assert.strictEqual(0, e.listeners('foo').length);
