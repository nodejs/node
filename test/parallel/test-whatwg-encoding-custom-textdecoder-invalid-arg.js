'use strict';

// This tests that ERR_INVALID_ARG_TYPE are thrown when
// invalid arguments are passed to TextDecoder.

require('../common');
const assert = require('assert');

{
  const notArrayBufferViewExamples = [false, {}, 1, '', new Error()];
  notArrayBufferViewExamples.forEach((invalidInputType) => {
    assert.throws(() => {
      new TextDecoder(undefined, null).decode(invalidInputType);
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    });
  });
}
