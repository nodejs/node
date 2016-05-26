'use strict';
// Flags: --expose_internals

const common = require('../common');
const path = require('path');
const assert = require('assert');
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

assert.throws(getHiddenValue(), /obj must be an object/);
assert.throws(getHiddenValue(null, 'foo'), /obj must be an object/);
assert.throws(getHiddenValue(undefined, 'foo'), /obj must be an object/);
assert.throws(getHiddenValue('bar', 'foo'), /obj must be an object/);
assert.throws(getHiddenValue(85, 'foo'), /obj must be an object/);
assert.throws(getHiddenValue({}), /name must be a string/);
assert.throws(getHiddenValue({}, null), /name must be a string/);
assert.throws(getHiddenValue({}, []), /name must be a string/);
assert.deepStrictEqual(internalUtil.getHiddenValue({}, 'foo'), undefined);

assert.throws(setHiddenValue(), /obj must be an object/);
assert.throws(setHiddenValue(null, 'foo'), /obj must be an object/);
assert.throws(setHiddenValue(undefined, 'foo'), /obj must be an object/);
assert.throws(setHiddenValue('bar', 'foo'), /obj must be an object/);
assert.throws(setHiddenValue(85, 'foo'), /obj must be an object/);
assert.throws(setHiddenValue({}), /name must be a string/);
assert.throws(setHiddenValue({}, null), /name must be a string/);
assert.throws(setHiddenValue({}, []), /name must be a string/);
const obj = {};
assert.strictEqual(internalUtil.setHiddenValue(obj, 'foo', 'bar'), true);
assert.strictEqual(internalUtil.getHiddenValue(obj, 'foo'), 'bar');

let arrowMessage;

try {
  require(path.join(common.fixturesDir, 'syntax', 'bad_syntax'));
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
