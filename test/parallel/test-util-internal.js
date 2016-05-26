'use strict';
// Flags: --expose_internals

const common = require('../common');
const path = require('path');
const assert = require('assert');
const internalUtil = require('internal/util');

function getHiddenValue(obj, name) {
  return function() {
    internalUtil.getHiddenValue(obj, name);
  };
}

assert.throws(getHiddenValue(), /obj must be an object/);
assert.throws(getHiddenValue(null, 'foo'), /obj must be an object/);
assert.throws(getHiddenValue(undefined, 'foo'), /obj must be an object/);
assert.throws(getHiddenValue('bar', 'foo'), /obj must be an object/);
assert.throws(getHiddenValue(85, 'foo'), /obj must be an object/);
assert.throws(getHiddenValue({}), /name must be a string/);
assert.throws(getHiddenValue({}, null), /name must be a string/);
assert.throws(getHiddenValue({}, []), /name must be a string/);
assert.deepEqual(internalUtil.getHiddenValue({}, 'foo'), undefined);

let arrowMessage;

try {
  require(path.join(common.fixturesDir, 'syntax', 'bad_syntax'));
} catch (err) {
  arrowMessage = internalUtil.getHiddenValue(err, 'arrowMessage');
}

assert(/bad_syntax\.js:1/.test(arrowMessage));
