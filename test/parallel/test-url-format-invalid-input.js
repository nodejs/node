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
  const error = common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "urlObject" argument must be one of type object or string. ' +
             `Received type ${type}`
  });
  assert.throws(function() { url.format(urlObject); }, error);
}
assert.strictEqual(url.format(''), '');
assert.strictEqual(url.format({}), '');
