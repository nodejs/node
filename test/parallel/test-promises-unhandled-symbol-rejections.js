'use strict';
const common = require('../common');

common.disableCrashOnUnhandledRejection();

const expectedValueWarning = ['Symbol()'];
const expectedPromiseWarning = ['Unhandled promise rejection. ' +
  'This error originated either by throwing ' +
  'inside of an async function without a catch ' +
  'block, or by rejecting a promise which was ' +
  'not handled with .catch(). (rejection id: 1)'];

common.expectWarning({
  UnhandledPromiseRejectionWarning: [
    expectedValueWarning,
    expectedPromiseWarning,
  ],
});

// ensure this doesn't crash
Promise.reject(Symbol());
