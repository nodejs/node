'use strict';
const common = require('../common');
const assert = require('assert');
const sym = Symbol();

process.on('normal', common.mustCall((data) => {
  assert.strictEqual(data, 'normalData');
}));

process.on(sym, common.mustCall((data) => {
  assert.strictEqual(data, 'symbolData');
}));

process.on('SIGPIPE', common.mustCall((data) => {
  assert.strictEqual(data, 'signalData');
}));

process.emit('normal', 'normalData');
process.emit(sym, 'symbolData');
process.emit('SIGPIPE', 'signalData');

assert.strictEqual(Number.isNaN(process._eventsCount), false);
