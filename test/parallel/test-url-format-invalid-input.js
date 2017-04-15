'use strict';
require('../common');
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
  const error = new RegExp('^TypeError: Parameter "urlObj" must be an object' +
                           `, not ${type}$`);
  assert.throws(function() { url.format(obj); }, error);
}
assert.strictEqual(url.format(''), '');
assert.strictEqual(url.format({}), '');
