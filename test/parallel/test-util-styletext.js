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
  () => {},
  {},
  [],
].forEach((invalidOption) => {
  assert.throws(() => {
    util.styleText(invalidOption, 'test');
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
  });
  assert.throws(() => {
    util.styleText('red', invalidOption);
  }, {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

assert.throws(() => {
  util.styleText('invalid', 'text');
}, {
  code: 'ERR_INVALID_ARG_VALUE',
});

assert.strictEqual(util.styleText('red', 'test'), '\u001b[31mtest\u001b[39m');
