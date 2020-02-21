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
  assert.throws(
    () => fs.lchmod(i, 0o777, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
  assert.throws(
    () => fs.lchmodSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
});

// Check mode
[false, null, undefined, {}, [], '', '123x'].forEach((input) => {
  const errObj = {
    code: 'ERR_INVALID_ARG_VALUE',
    name: 'TypeError',
    message: 'The argument \'mode\' must be a 32-bit unsigned integer or an ' +
             `octal string. Received ${util.inspect(input)}`
  };

  assert.rejects(promises.lchmod(f, input, () => {}), errObj);
  assert.throws(() => fs.lchmodSync(f, input), errObj);
});

[-1, 2 ** 32].forEach((input) => {
  const errObj = {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "mode" is out of range. It must be >= 0 && <= ' +
             `4294967295. Received ${input}`
  };

  assert.rejects(promises.lchmod(f, input, () => {}), errObj);
  assert.throws(() => fs.lchmodSync(f, input), errObj);
});
