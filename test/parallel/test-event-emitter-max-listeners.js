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
}, common.expectsError('ERR_INVALID_ARG_TYPE', TypeError));

assert.throws(function() {
  e.setMaxListeners(-1);
}, common.expectsError('ERR_INVALID_ARG_TYPE', TypeError));

assert.throws(function() {
  e.setMaxListeners('and even this');
}, common.expectsError('ERR_INVALID_ARG_TYPE', TypeError));

e.emit('maxListeners');
