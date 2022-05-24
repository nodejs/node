'use strict';
const assert = require('assert');
const url = require('url');

const throwsObjsAndReportTypes = [
  undefined,
  null,
  true,
  false,
  0,
  function() {},
  Symbol('foo'),
];

for (const urlObject of throwsObjsAndReportTypes) {
  assert.throws(() => {
    url.format(urlObject);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  });
}
assert.strictEqual(url.format(''), '');
assert.strictEqual(url.format({}), '');
