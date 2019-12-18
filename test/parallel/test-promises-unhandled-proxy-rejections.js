'use strict';
const common = require('../common');

common.disableCrashOnUnhandledRejection();

const expectedDeprecationWarning = ['Unhandled promise rejections are ' +
                                   'deprecated. In the future, promise ' +
                                   'rejections that are not handled will ' +
                                   'terminate the Node.js process with a ' +
                                   'non-zero exit code.', 'DEP0018'];
const expectedPromiseWarning = ['Unhandled promise rejection. ' +
  'This error originated either by throwing ' +
  'inside of an async function without a catch ' +
  'block, or by rejecting a promise which was ' +
  'not handled with .catch(). To terminate the ' +
  'node process on unhandled promise rejection, ' +
  'use the CLI flag `--unhandled-rejections=strict` (see ' +
  'https://nodejs.org/api/cli.html#cli_unhandled_rejections_mode). ' +
  '(rejection id: 1)'];

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

common.expectWarning({
  DeprecationWarning: expectedDeprecationWarning,
  UnhandledPromiseRejectionWarning: expectedPromiseWarning,
});

// Ensure this doesn't crash
Promise.reject(thorny);
