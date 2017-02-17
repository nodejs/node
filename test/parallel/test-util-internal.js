'use strict';
// Flags: --expose_internals

const common = require('../common');
const path = require('path');
const assert = require('assert');

const binding = process.binding('util');
const kArrowMessagePrivateSymbolIndex = binding['arrow_message_private_symbol'];

function getHiddenValue(obj, index) {
  return function() {
    binding.getHiddenValue(obj, index);
  };
}

function setHiddenValue(obj, index, val) {
  return function() {
    binding.setHiddenValue(obj, index, val);
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
    binding.getHiddenValue({}, kArrowMessagePrivateSymbolIndex),
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
    binding.setHiddenValue(obj, kArrowMessagePrivateSymbolIndex, 'bar'),
    true);
assert.strictEqual(
    binding.getHiddenValue(obj, kArrowMessagePrivateSymbolIndex),
    'bar');

let arrowMessage;

try {
  require(path.join(common.fixturesDir, 'syntax', 'bad_syntax'));
} catch (err) {
  arrowMessage =
      binding.getHiddenValue(err, kArrowMessagePrivateSymbolIndex);
}

assert(/bad_syntax\.js:1/.test(arrowMessage));
