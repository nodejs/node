'use strict';
const common = require('../common');
const assert = require('assert');
const url = require('url');

const throwsObjsAndReportTypes = [
  undefined,
  null,
  true,
  false,
  0,
  function() {},
  Symbol('foo')
];

for (const urlObject of throwsObjsAndReportTypes) {
  common.expectsError(function() {
    url.format(urlObject);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "urlObject" argument must be of type string or an instance ' +
             `of Object.${common.invalidArgTypeHelper(urlObject)}`
  });
}
assert.strictEqual(url.format(''), '');
assert.strictEqual(url.format({}), '');
