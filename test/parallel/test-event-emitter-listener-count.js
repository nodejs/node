'use strict';

require('../common');
const assert = require('assert');
const EventEmitter = require('events');
const emitter = new EventEmitter();

function noop() {}

emitter.on('foo', noop);
emitter.on('foo', noop);
emitter.on('baz', noop);
// Allow any type
emitter.on(123, noop);

assert.strictEqual(EventEmitter.listenerCount(emitter, 'foo'), 2);
assert.strictEqual(emitter.listenerCount('foo'), 2);
assert.strictEqual(emitter.listenerCount('bar'), 0);
assert.strictEqual(emitter.listenerCount('baz'), 1);
assert.strictEqual(emitter.listenerCount(123), 1);

// The inherited properties should not be counted towards the actual
// listeners count
assert.strictEqual(EventEmitter.listenerCount(emitter, 'toString'), 0);

// when we add a new listener with the name of an inherited property, it should
// accept it
emitter.on('toString', noop);
assert.strictEqual(EventEmitter.listenerCount(emitter, 'toString'), 1);

// after removing a listener with the name of an inherited property, the count
// should reduce by one
emitter.removeListener('toString', noop);
assert.strictEqual(EventEmitter.listenerCount(emitter, 'toString'), 0);
