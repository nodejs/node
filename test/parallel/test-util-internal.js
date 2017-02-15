'use strict';
// Flags: --expose_internals

const common = require('../common');
const path = require('path');
const assert = require('assert');
const internalUtil = require('internal/util');
const spawnSync = require('child_process').spawnSync;

const binding = process.binding('util');
const kArrowMessagePrivateSymbolIndex = binding['arrow_message_private_symbol'];

function getHiddenValue(obj, index) {
  return function() {
    internalUtil.getHiddenValue(obj, index);
  };
}

function setHiddenValue(obj, index, val) {
  return function() {
    internalUtil.setHiddenValue(obj, index, val);
  };
}

assert.throws(getHiddenValue(), /obj must be an object/);
assert.throws(getHiddenValue(null, 'foo'), /obj must be an object/);
assert.throws(getHiddenValue(undefined, 'foo'), /obj must be an object/);
assert.throws(getHiddenValue('bar', 'foo'), /obj must be an object/);
assert.throws(getHiddenValue(85, 'foo'), /obj must be an object/);
assert.throws(getHiddenValue({}), /index must be an uint32/);
assert.throws(getHiddenValue({}, null), /index must be an uint32/);
assert.throws(getHiddenValue({}, []), /index must be an uint32/);
assert.deepStrictEqual(
    internalUtil.getHiddenValue({}, kArrowMessagePrivateSymbolIndex),
    undefined);

assert.throws(setHiddenValue(), /obj must be an object/);
assert.throws(setHiddenValue(null, 'foo'), /obj must be an object/);
assert.throws(setHiddenValue(undefined, 'foo'), /obj must be an object/);
assert.throws(setHiddenValue('bar', 'foo'), /obj must be an object/);
assert.throws(setHiddenValue(85, 'foo'), /obj must be an object/);
assert.throws(setHiddenValue({}), /index must be an uint32/);
assert.throws(setHiddenValue({}, null), /index must be an uint32/);
assert.throws(setHiddenValue({}, []), /index must be an uint32/);
const obj = {};
assert.strictEqual(
    internalUtil.setHiddenValue(obj, kArrowMessagePrivateSymbolIndex, 'bar'),
    true);
assert.strictEqual(
    internalUtil.getHiddenValue(obj, kArrowMessagePrivateSymbolIndex),
    'bar');

let arrowMessage;

try {
  require(path.join(common.fixturesDir, 'syntax', 'bad_syntax'));
} catch (err) {
  arrowMessage =
      internalUtil.getHiddenValue(err, kArrowMessagePrivateSymbolIndex);
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
