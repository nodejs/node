'use strict';
const common = require('../common');
const path = require('path');
const assert = require('assert');

const invalidInput = [
  111,
  'foo',
  undefined,
  () => {}
];
invalidInput.forEach((val) => {
  const err = common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "pathObject" argument must be of type Object. ' +
             `Received type ${typeof val}`
  });
  assert.throws(function() {
    path.format(val);
  }, err);
});
