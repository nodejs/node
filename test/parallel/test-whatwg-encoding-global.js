'use strict';

require('../common');
const assert = require('assert');
const { TextEncoder, TextDecoder } = require('util');

assert.deepStrictEqual(
  Object.getOwnPropertyDescriptor(global, 'TextEncoder'),
  {
    value: TextEncoder,
    writable: true,
    configurable: true,
    enumerable: false
  }
);

assert.deepStrictEqual(
  Object.getOwnPropertyDescriptor(global, 'TextDecoder'),
  {
    value: TextDecoder,
    writable: true,
    configurable: true,
    enumerable: false
  }
);