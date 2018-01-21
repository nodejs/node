'use strict';

require('../common');
const assert = require('assert');
const { URL, URLSearchParams } = require('url');

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
