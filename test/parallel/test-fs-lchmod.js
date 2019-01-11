'use strict';

const common = require('../common');
const assert = require('assert');
const util = require('util');
const fs = require('fs');
const { promises } = fs;
const f = __filename;

// This test ensures that input for lchmod is valid, testing for valid
// inputs for path, mode and callback

if (!common.isOSX) {
  common.skip('lchmod is only available on macOS');
}

// Check callback
assert.throws(() => fs.lchmod(f), { code: 'ERR_INVALID_CALLBACK' });
assert.throws(() => fs.lchmod(), { code: 'ERR_INVALID_CALLBACK' });
assert.throws(() => fs.lchmod(f, {}), { code: 'ERR_INVALID_CALLBACK' });

// Check path
[false, 1, {}, [], null, undefined].forEach((i) => {
  common.expectsError(
    () => fs.lchmod(i, 0o777, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.lchmodSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
});

// Check mode
[false, null, undefined, {}, [], '', '123x'].forEach((input) => {
  const errObj = {
    code: 'ERR_INVALID_ARG_VALUE',
    name: 'TypeError [ERR_INVALID_ARG_VALUE]',
    message: 'The argument \'mode\' must be a 32-bit unsigned integer or an ' +
             `octal string. Received ${util.inspect(input)}`
  };

  promises.lchmod(f, input, () => {})
    .then(common.mustNotCall())
    .catch(common.expectsError(errObj));
  assert.throws(() => fs.lchmodSync(f, input), errObj);
});

[-1, 2 ** 32].forEach((input) => {
  const errObj = {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError [ERR_OUT_OF_RANGE]',
    message: 'The value of "mode" is out of range. It must be >= 0 && < ' +
             `4294967296. Received ${input}`
  };

  promises.lchmod(f, input, () => {})
    .then(common.mustNotCall())
    .catch(common.expectsError(errObj));
  assert.throws(() => fs.lchmodSync(f, input), errObj);
});
