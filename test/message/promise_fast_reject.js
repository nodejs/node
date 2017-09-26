'use strict';

// We should always have the stacktrace of the oldest rejection.
// Theoretically the GC could handle this differently.

require('../common');
const assert = require('assert');

new Promise(function(res, rej) {
  throw new Error('One');
});

new Promise(function(res, rej) {
  throw new Error('Two');
});

process.on('uncaughtException', (err) =>
  assert.fail('Should not trigger uncaught exception'));

process.on('exit', () => process._rawDebug('exit event emitted'));
