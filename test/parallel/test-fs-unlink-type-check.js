'use strict';

const common = require('../common');

const assert = require('assert');
const fs = require('fs');

const expectedError = (e) => {
  assert.strictEqual(e.code, 'ERR_INVALID_ARG_TYPE');
  assert.ok(e instanceof TypeError);
};

[false, 1, {}, [], null, undefined].forEach((i) => {
  common.fsTest('unlink', [i, expectedError], { throws: true });
  common.expectsError(
    () => fs.unlinkSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
});
