'use strict';
require('../common');
const assert = require('assert');

const unexpectedValues = [
  undefined,
  null,
  1,
  {},
  () => {},
];
for (const it of unexpectedValues) {
  assert.throws(() => {
    process.setSourceMapsEnabled(it);
  }, /ERR_INVALID_ARG_TYPE/);
}
