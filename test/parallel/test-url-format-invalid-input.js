'use strict';
const common = require('../common');
const assert = require('assert');
const url = require('url');

const throwsObjsAndReportTypes = new Map([
  [undefined, 'undefined'],
  [null, 'null'],
  [true, 'boolean'],
  [false, 'boolean'],
  [0, 'number'],
  [function() {}, 'function'],
  [Symbol('foo'), 'symbol']
]);

for (const [urlObject, type] of throwsObjsAndReportTypes) {
  common.expectsError(function() {
    url.format(urlObject);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "urlObject" argument must be one of type Object or string. ' +
             `Received type ${type}`
  });
}
assert.strictEqual(url.format(''), '');
assert.strictEqual(url.format({}), '');
