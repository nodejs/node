'use strict';

require('../common');
const assert = require('assert');
const EventEmitter = require('events');

const emitter = new EventEmitter();
emitter.on('foo', () => {});
emitter.on('foo', () => {});
emitter.on('baz', () => {});
// Allow any type
emitter.on(123, () => {});

assert.strictEqual(EventEmitter.listenerCount(emitter, 'foo'), 2);
assert.strictEqual(emitter.listenerCount('foo'), 2);
assert.strictEqual(emitter.listenerCount('bar'), 0);
assert.strictEqual(emitter.listenerCount('baz'), 1);
assert.strictEqual(emitter.listenerCount(123), 1);

assert.throws(() => {
  EventEmitter.listenerCount(emitter, () => {});
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
  message: 'The "type" argument must be one of type string or symbol. ' +
           'Received type function ([Function (anonymous)])'
});

assert.throws(() => {
  emitter.listenerCount(() => {});
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
  message: 'The "type" argument must be one of type string or symbol. ' +
           'Received type function ([Function (anonymous)])'
});
