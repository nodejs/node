'use strict';

require('../common');
const assert = require('assert');
const EventEmitter = require('events');

const emitter = new EventEmitter();
emitter.on('foo', function() {});
emitter.on('foo', function() {});
emitter.on('baz', function() {});
// Allow any type
emitter.on(123, function() {});

assert.strictEqual(EventEmitter.listenerCount(emitter, 'foo'), 2);
assert.strictEqual(emitter.listenerCount('foo'), 2);
assert.strictEqual(emitter.listenerCount('bar'), 0);
assert.strictEqual(emitter.listenerCount('baz'), 1);
assert.strictEqual(emitter.listenerCount(123), 1);
