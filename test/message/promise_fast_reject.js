'use strict';

// We should always have the stacktrace of the oldest rejection.

const common = require('../common');

new Promise(function(res, rej) {
  consol.log('One'); // eslint-disable-line no-undef
});

new Promise(function(res, rej) {
  consol.log('Two'); // eslint-disable-line no-undef
});

process.on('uncaughtException', (err) =>
    common.fail('Should not trigger uncaught exception'));

process.on('exit', () => process._rawDebug('exit event emitted'));
