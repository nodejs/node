'use strict';
const common = require('../common');

const expectedValueWarning = 'Symbol()';
const expectedDeprecationWarning = 'Unhandled promise rejections are ' +
                                   'deprecated. In the future, promise ' +
                                   'rejections that are not handled will ' +
                                   'terminate the Node.js process with a ' +
                                   'non-zero exit code.';
const expectedPromiseWarning = 'Unhandled promise rejection. ' +
  'This error originated either by throwing ' +
  'inside of an async function without a catch ' +
  'block, or by rejecting a promise which was ' +
  'not handled with .catch(). (rejection id: 1)';

common.expectWarning({
  DeprecationWarning: expectedDeprecationWarning,
  UnhandledPromiseRejectionWarning: [
    expectedPromiseWarning,
    expectedValueWarning
  ],
});

// ensure this doesn't crash
Promise.reject(Symbol());
