'use strict';

const common = require('../common');
const EventEmitter = require('events');
const assert = require('assert');

const EE = new EventEmitter();
const handler = common.mustCall(undefined, 3);
const anotherHandler = common.mustCall();

assert.strictEqual(EE.listenerCount('event'), 0);
assert.strictEqual(EE.listenerCount('event', handler), 0);
assert.strictEqual(EE.listenerCount('event', anotherHandler), 0);

EE.once('event', handler);

assert.strictEqual(EE.listenerCount('event'), 1);
assert.strictEqual(EE.listenerCount('event', handler), 1);
assert.strictEqual(EE.listenerCount('event', anotherHandler), 0);

EE.removeAllListeners('event');

assert.strictEqual(EE.listenerCount('event'), 0);
assert.strictEqual(EE.listenerCount('event', handler), 0);
assert.strictEqual(EE.listenerCount('event', anotherHandler), 0);

EE.on('event', handler);

assert.strictEqual(EE.listenerCount('event'), 1);
assert.strictEqual(EE.listenerCount('event', handler), 1);
assert.strictEqual(EE.listenerCount('event', anotherHandler), 0);

EE.once('event', anotherHandler);

assert.strictEqual(EE.listenerCount('event'), 2);
assert.strictEqual(EE.listenerCount('event', handler), 1);
assert.strictEqual(EE.listenerCount('event', anotherHandler), 1);

assert.strictEqual(EE.listenerCount('another-event'), 0);
assert.strictEqual(EE.listenerCount('another-event', handler), 0);
assert.strictEqual(EE.listenerCount('another-event', anotherHandler), 0);

EE.once('event', handler);

assert.strictEqual(EE.listenerCount('event'), 3);
assert.strictEqual(EE.listenerCount('event', handler), 2);
assert.strictEqual(EE.listenerCount('event', anotherHandler), 1);

EE.emit('event');

assert.strictEqual(EE.listenerCount('event'), 1);
assert.strictEqual(EE.listenerCount('event', handler), 1);
assert.strictEqual(EE.listenerCount('event', anotherHandler), 0);

EE.emit('event');

assert.strictEqual(EE.listenerCount('event'), 1);
assert.strictEqual(EE.listenerCount('event', handler), 1);
assert.strictEqual(EE.listenerCount('event', anotherHandler), 0);

EE.off('event', handler);

assert.strictEqual(EE.listenerCount('event'), 0);
assert.strictEqual(EE.listenerCount('event', handler), 0);
assert.strictEqual(EE.listenerCount('event', anotherHandler), 0);
