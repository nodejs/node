'use strict';
const common = require('../common');


Promise.reject(new Error('oops'));

setImmediate(() => {
  common.fail('Should not reach Immediate');
});

process.on('beforeExit', () =>
    common.fail('beforeExit should not be reached'));

process.on('uncaughtException', (err) => {
  common.fail('Should not trigger uncaught exception');
});

process.on('exit', () => process._rawDebug('exit event emitted'));
