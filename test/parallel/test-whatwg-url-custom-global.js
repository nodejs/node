'use strict';

// Tests below are not from WPT.

require('../common');
const assert = require('assert');

assert.deepStrictEqual(
  Object.getOwnPropertyDescriptor(global, 'URL'),
  {
    value: URL,
    writable: true,
    configurable: true,
    enumerable: false
  }
);

assert.deepStrictEqual(
  Object.getOwnPropertyDescriptor(global, 'URLSearchParams'),
  {
    value: URLSearchParams,
    writable: true,
    configurable: true,
    enumerable: false
  }
);
