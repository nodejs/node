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

for (const [obj, type] of throwsObjsAndReportTypes) {
  const error = common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "urlObj" argument must be of type object. ' +
             `Received type ${type}`
  });
  assert.throws(function() { url.format(obj); }, error);
}
assert.strictEqual(url.format(''), '');
assert.strictEqual(url.format({}), '');
