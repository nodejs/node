'use strict';
const common = require('../common');

const expectedDeprecationWarning = 'Unhandled promise rejections are ' +
                                   'deprecated. In the future, promise ' +
                                   'rejections that are not handled will ' +
                                   'terminate the Node.js process with a ' +
                                   'non-zero exit code.';
const expectedPromiseWarning = 'Unhandled promise rejection (rejection id: ' +
                               '1): Symbol()';

common.expectWarning({
  DeprecationWarning: expectedDeprecationWarning,
  UnhandledPromiseRejectionWarning: expectedPromiseWarning,
});

// ensure this doesn't crash
Promise.reject(Symbol());
