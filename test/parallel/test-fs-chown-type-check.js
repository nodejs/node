'use strict';

const common = require('../common');

const assert = require('assert');
const fs = require('fs');

[false, 1, {}, [], null, undefined].forEach((i) => {
  const checkErr = (e) => {
    assert.strictEqual(e.code, 'ERR_INVALID_ARG_TYPE');
    assert.ok(e instanceof TypeError);
    return true;
  };
  common.fsTest('chown', [i, 1, 1, checkErr], { throws: true });
  common.expectsError(
    () => fs.chownSync(i, 1, 1),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
});

[false, 'test', {}, [], null, undefined].forEach((i) => {
  const checkErr = (e) => {
    assert.strictEqual(e.code, 'ERR_INVALID_ARG_TYPE');
    assert.ok(e instanceof TypeError);
    return true;
  };
  common.fsTest(
    'chown', ['not_a_file_that_exists', i, 1, checkErr], { throws: true }
  );
  common.fsTest(
    'chown', ['not_a_file_that_exists', 1, i, checkErr], { throws: true }
  );

  common.expectsError(
    () => fs.chownSync('not_a_file_that_exists', i, 1),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.chownSync('not_a_file_that_exists', 1, i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
});
