'use strict';
require('../common');
var assert = require('assert');
var EventEmitter = require('events');

var emitter = new EventEmitter();

assert.strictEqual(emitter.getMaxListeners(), EventEmitter.defaultMaxListeners);

emitter.setMaxListeners(0);
assert.strictEqual(emitter.getMaxListeners(), 0);

emitter.setMaxListeners(3);
assert.strictEqual(emitter.getMaxListeners(), 3);

// https://github.com/nodejs/node/issues/523 - second call should not throw.
var recv = {};
EventEmitter.prototype.on.call(recv, 'event', function() {});
EventEmitter.prototype.on.call(recv, 'event', function() {});
