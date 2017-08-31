'use strict';

// We should always have the stacktrace of the oldest rejection.

require('../common');
const assert = require('assert');

new Promise(function(res, rej) {
  consol.log('One'); // eslint-disable-line no-undef
});

new Promise(function(res, rej) {
  consol.log('Two'); // eslint-disable-line no-undef
});

process.on('unhandledRejection', () => {}); // Ignore

process.on('uncaughtException', (err) =>
  assert.fail('Should not trigger uncaught exception'));

process.on('exit', () => process._rawDebug('exit event emitted'));
