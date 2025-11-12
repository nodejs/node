// Flags: --unhandled-rejections=none
'use strict';
const common = require('../common');

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

process.on('warning', common.mustNotCall());

// Ensure this doesn't crash
Promise.reject(thorny);
