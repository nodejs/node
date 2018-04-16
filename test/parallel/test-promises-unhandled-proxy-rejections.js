'use strict';
const common = require('../common');

common.disableCrashOnUnhandledRejection();

const expectedPromiseWarning = ['Unhandled promise rejection. ' +
  'This error originated either by throwing ' +
  'inside of an async function without a catch ' +
  'block, or by rejecting a promise which was ' +
  'not handled with .catch(). (rejection id: 1)', common.noWarnCode];

function throwErr(name) {
  return function() {
    throw new Error(`Error from ${name}`);
  };
}

const thorny = new Proxy({}, {
  getPrototypeOf: throwErr('getPrototypeOf'),
  setPrototypeOf: throwErr('setPrototypeOf'),
  isExtensible: throwErr('isExtensible'),
  preventExtensions: throwErr('preventExtensions'),
  getOwnPropertyDescriptor: throwErr('getOwnPropertyDescriptor'),
  defineProperty: throwErr('defineProperty'),
  has: throwErr('has'),
  get: throwErr('get'),
  set: throwErr('set'),
  deleteProperty: throwErr('deleteProperty'),
  ownKeys: throwErr('ownKeys'),
  apply: throwErr('apply'),
  construct: throwErr('construct')
});

common.expectWarning({
  UnhandledPromiseRejectionWarning: expectedPromiseWarning,
});

// ensure this doesn't crash
Promise.reject(thorny);
