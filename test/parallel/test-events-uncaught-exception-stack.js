'use strict';
const common = require('../common');
const assert = require('assert');
const EventEmitter = require('events');

// Tests that the error stack where the exception was emitted is appended.

process.on('uncaughtException', common.mustCall((err) => {
  assert.match(err.stack, /Emitted 'error' event at:/);
}));

new EventEmitter().emit('error', new Error());
