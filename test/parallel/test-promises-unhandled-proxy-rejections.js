'use strict';
const common = require('../common');

const expectedPromiseWarning = 'Unhandled promise rejection (rejection id: ' +
                               '1): [object Object]';

function throwErr() {
  throw new Error('Error from proxy');
}

const thorny = new Proxy({}, {
  getPrototypeOf: throwErr,
  setPrototypeOf: throwErr,
  isExtensible: throwErr,
  preventExtensions: throwErr,
  getOwnPropertyDescriptor: throwErr,
  defineProperty: throwErr,
  has: throwErr,
  get: throwErr,
  set: throwErr,
  deleteProperty: throwErr,
  ownKeys: throwErr,
  apply: throwErr,
  construct: throwErr
});

common.expectWarning('UnhandledPromiseRejectionWarning',
                     expectedPromiseWarning);

// ensure this doesn't crash
Promise.reject(thorny);
