'use strict';
const common = require('../common');
const assert = require('assert');

process.on('warning', common.mustCall((warning) => {
  assert.strictEqual(warning.name, 'Warning');
  assert.strictEqual(warning.message,
                     'Listening to SIGBUS, SIGFPE, SIGSEGV, SIGILL signals ' +
                     'is not safe and the listener may be called in an ' +
                     'infinite loop');
}));

process.on('SIGBUS', () => {});
