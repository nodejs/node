'use strict';
const common = require('../common');
const assert = require('node:assert');
const EventEmitter = require('node:events');

class CustomEmitter extends EventEmitter {}

const ee = new EventEmitter();
const customEE = new CustomEmitter();

let monitorCount = 0;
let uncaughtCount = 0;

process.on('uncaughtExceptionMonitor', common.mustCall((err, origin) => {
  assert.strictEqual(origin, 'uncaughtException');
  monitorCount++;
  if (monitorCount === 1) {
    assert.match(err.stack, /Emitted 'error' event at:/);
    assert.match(err.stack, /at emitPlainError/);
  } else if (monitorCount === 2) {
    assert.match(err.stack, /Emitted 'error' event on CustomEmitter instance at:/);
    assert.match(err.stack, /at emitCustomClassError/);
  }
}, 2));

process.on('uncaughtException', common.mustCall((err, origin) => {
  assert.strictEqual(origin, 'uncaughtException');
  uncaughtCount++;
  if (uncaughtCount === 1) {
    assert.match(err.stack, /Emitted 'error' event at:/);
    assert.match(err.stack, /at emitPlainError/);
    process.nextTick(emitCustomClassError);
  } else if (uncaughtCount === 2) {
    assert.match(err.stack, /Emitted 'error' event on CustomEmitter instance at:/);
    assert.match(err.stack, /at emitCustomClassError/);
  }
}, 2));

function emitPlainError() {
  ee.emit('error', new Error('plain error'));
}

function emitCustomClassError() {
  customEE.emit('error', new Error('custom class error'));
}

emitPlainError();
