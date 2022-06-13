'use strict';
require('../common');
const assert = require('assert');
const util = require('util');

[
  undefined,
  null,
  false,
  5n,
  5,
  Symbol(),
].forEach((invalidOption) => {
  assert.throws(() => {
    util.colorText(invalidOption, 'test');
  }, {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => {
    util.colorText('red', invalidOption);
  }, {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

assert.throws(() => {
  util.colorText('red', undefined);
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  message: 'The "text" argument must be of type string. Received undefined'
});

assert.strictEqual(util.colorText('red', 'test'), '\u001b[31mtest\u001b[39m');
