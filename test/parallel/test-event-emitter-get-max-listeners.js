var common = require('../common');
var assert = require('assert');
var EventEmitter = require('events');

var emitter = new EventEmitter();

assert.strictEqual(emitter.getMaxListeners(), EventEmitter.defaultMaxListeners);

emitter.setMaxListeners(0)
assert.strictEqual(emitter.getMaxListeners(), 0)

emitter.setMaxListeners(3)
assert.strictEqual(emitter.getMaxListeners(), 3)
