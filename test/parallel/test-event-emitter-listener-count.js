'use strict';

const common = require('../common');
const assert = require('assert');
const EventEmitter = require('events');

const emitter = new EventEmitter();
emitter.on('foo', common.noop);
emitter.on('foo', common.noop);
emitter.on('baz', common.noop);
// Allow any type
emitter.on(123, common.noop);

assert.strictEqual(EventEmitter.listenerCount(emitter, 'foo'), 2);
assert.strictEqual(emitter.listenerCount('foo'), 2);
assert.strictEqual(emitter.listenerCount('bar'), 0);
assert.strictEqual(emitter.listenerCount('baz'), 1);
assert.strictEqual(emitter.listenerCount(123), 1);
