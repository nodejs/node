'use strict';

require('../common');
const { deepStrictEqual, strictEqual, throws } = require('assert');

const b = Buffer.from([99, 1, 2, 3, 100]).subarray(1, 4);

deepStrictEqual(Buffer.createView(b), b);
strictEqual(Buffer.createView(b).buffer, b.buffer);

[
  [{}, 'object'],
  [[0, 1, 2, 3], 'object'],
  ['foo', 'string'],
].forEach(([input, actualType]) => {
  const errObj = {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The first argument must be one of type ' +
             'ArrayBuffer, ArrayBufferView, or Buffer. ' +
             `Received type ${actualType}`
  };
  throws(() => Buffer.createView(input), errObj);
});
