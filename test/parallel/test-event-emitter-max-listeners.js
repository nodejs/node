'use strict';
const common = require('../common');
const assert = require('assert');
const events = require('events');
const e = new events.EventEmitter();

e.on('maxListeners', common.mustCall());

// Should not corrupt the 'maxListeners' queue.
e.setMaxListeners(42);

const maxError = /^TypeError: "n" argument must be a positive number$/;

assert.throws(function() { e.setMaxListeners(NaN); }, maxError);
assert.throws(function() { e.setMaxListeners(-1); }, maxError);
assert.throws(function() { e.setMaxListeners('and even this'); }, maxError);

e.emit('maxListeners');
