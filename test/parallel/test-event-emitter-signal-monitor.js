'use strict';
const common = require('../common');
const assert = require('assert');
const EventEmitter = require('events');

// Test that EventEmitter.signalMonitor is not called on EE.
const EE = new EventEmitter();
EE.on('signalMonitor', common.mustNotCall());
EE.emit('SIGINT');

// Test that EventEmitter.signalMonitor is called on process.
process.on('signalMonitor', common.mustCall(function(signal) {
  assert.strictEqual(signal, 'SIGINT');
}, 1));
process.emit('SIGINT');

// Test that EventEmitter.signalMonitor is not called on other events.
process.emit('test');
