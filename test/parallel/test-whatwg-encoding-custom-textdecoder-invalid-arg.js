'use strict';

// This tests that ERR_INVALID_ARG_TYPE are thrown when
// invalid arguments are passed to TextDecoder.

const common = require('../common');

{
  const notArrayBufferViewExamples = [false, {}, 1, '', new Error()];
  notArrayBufferViewExamples.forEach((invalidInputType) => {
    common.expectsError(() => {
      new TextDecoder(undefined, null).decode(invalidInputType);
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    });
  });
}
