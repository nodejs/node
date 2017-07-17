'use strict';
// Flags: --expose_internals

require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const internalUtil = require('internal/util');
const spawnSync = require('child_process').spawnSync;

function getHiddenValue(obj, name) {
  return function() {
    internalUtil.getHiddenValue(obj, name);
  };
}

function setHiddenValue(obj, name, val) {
  return function() {
    internalUtil.setHiddenValue(obj, name, val);
  };
}

const errMessageObj = /obj must be an object/;
const errMessageStr = /name must be a string/;

assert.throws(getHiddenValue(), errMessageObj);
assert.throws(getHiddenValue(null, 'foo'), errMessageObj);
assert.throws(getHiddenValue(undefined, 'foo'), errMessageObj);
assert.throws(getHiddenValue('bar', 'foo'), errMessageObj);
assert.throws(getHiddenValue(85, 'foo'), errMessageObj);
assert.throws(getHiddenValue({}), errMessageStr);
assert.throws(getHiddenValue({}, null), errMessageStr);
assert.throws(getHiddenValue({}, []), errMessageStr);
assert.deepStrictEqual(internalUtil.getHiddenValue({}, 'foo'), undefined);

assert.throws(setHiddenValue(), errMessageObj);
assert.throws(setHiddenValue(null, 'foo'), errMessageObj);
assert.throws(setHiddenValue(undefined, 'foo'), errMessageObj);
assert.throws(setHiddenValue('bar', 'foo'), errMessageObj);
assert.throws(setHiddenValue(85, 'foo'), errMessageObj);
assert.throws(setHiddenValue({}), errMessageStr);
assert.throws(setHiddenValue({}, null), errMessageStr);
assert.throws(setHiddenValue({}, []), errMessageStr);
const obj = {};
assert.strictEqual(internalUtil.setHiddenValue(obj, 'foo', 'bar'), true);
assert.strictEqual(internalUtil.getHiddenValue(obj, 'foo'), 'bar');

let arrowMessage;

try {
  require(fixtures.path('syntax', 'bad_syntax'));
} catch (err) {
  arrowMessage = internalUtil.getHiddenValue(err, 'node:arrowMessage');
}

assert(/bad_syntax\.js:1/.test(arrowMessage));

const args = [
  '--expose-internals',
  '-e',
  "require('internal/util').error('foo %d', 5)"
];
const result = spawnSync(process.argv[0], args, { encoding: 'utf8' });
assert.strictEqual(result.stderr.indexOf('%'), -1);
assert(/foo 5/.test(result.stderr));
