'use strict';

// This tests that the errors thrown from fs.close and fs.closeSync
// include the desired properties

require('../common');
const assert = require('assert');
const fs = require('fs');

['', false, null, undefined, {}, []].forEach((input) => {
  const errObj = {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError [ERR_INVALID_ARG_TYPE]',
    message: 'The "fd" argument must be of type number. ' +
             `Received type ${typeof input}`
  };
  assert.throws(() => fs.close(input), errObj);
  assert.throws(() => fs.closeSync(input), errObj);
});
