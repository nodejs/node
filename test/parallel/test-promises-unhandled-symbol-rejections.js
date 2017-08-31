'use strict';
const common = require('../common');

const expectedHandledWarning = 'Promise rejection was handled ' +
                               'asynchronously (rejection id: 1)';
const expectedPromiseWarning = 'Unhandled promise rejection (rejection id: ' +
                               '1): Symbol()';

common.expectWarning({
  UnhandledPromiseRejectionWarning: expectedPromiseWarning,
  PromiseRejectionHandledWarning: expectedHandledWarning
});

// ensure this doesn't crash
const p = Promise.reject(Symbol());
setTimeout(() => {
  p.catch(() => {});
}, 1);
