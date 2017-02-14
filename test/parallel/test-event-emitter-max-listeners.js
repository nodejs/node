'use strict';
const common = require('../common');
const assert = require('assert');
const events = require('events');
const e = new events.EventEmitter();

e.on('maxListeners', common.mustCall(function() {}));

// Should not corrupt the 'maxListeners' queue.
e.setMaxListeners(42);

assert.throws(function() {
  e.setMaxListeners(NaN);
}, /^TypeError: "n" argument must be a positive number$/);

assert.throws(function() {
  e.setMaxListeners(-1);
}, /^TypeError: "n" argument must be a positive number$/);

assert.throws(function() {
  e.setMaxListeners('and even this');
}, /^TypeError: "n" argument must be a positive number$/);

e.emit('maxListeners');
