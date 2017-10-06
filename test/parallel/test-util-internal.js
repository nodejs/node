'use strict';
// Flags: --expose_internals

require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');

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

const errMessageObj = /obj must be an object/;
const errMessageIndex = /index must be an uint32/;

assert.throws(getHiddenValue(), errMessageObj);
assert.throws(getHiddenValue(null, 'foo'), errMessageObj);
assert.throws(getHiddenValue(undefined, 'foo'), errMessageObj);
assert.throws(getHiddenValue('bar', 'foo'), errMessageObj);
assert.throws(getHiddenValue(85, 'foo'), errMessageObj);
assert.throws(getHiddenValue({}), errMessageIndex);
assert.throws(getHiddenValue({}, null), errMessageIndex);
assert.throws(getHiddenValue({}, []), errMessageIndex);
assert.deepStrictEqual(
  binding.getHiddenValue({}, kArrowMessagePrivateSymbolIndex),
  undefined);

assert.throws(setHiddenValue(), errMessageObj);
assert.throws(setHiddenValue(null, 'foo'), errMessageObj);
assert.throws(setHiddenValue(undefined, 'foo'), errMessageObj);
assert.throws(setHiddenValue('bar', 'foo'), errMessageObj);
assert.throws(setHiddenValue(85, 'foo'), errMessageObj);
assert.throws(setHiddenValue({}), errMessageIndex);
assert.throws(setHiddenValue({}, null), errMessageIndex);
assert.throws(setHiddenValue({}, []), errMessageIndex);
const obj = {};
assert.strictEqual(
  binding.setHiddenValue(obj, kArrowMessagePrivateSymbolIndex, 'bar'),
  true);
assert.strictEqual(
  binding.getHiddenValue(obj, kArrowMessagePrivateSymbolIndex),
  'bar');

let arrowMessage;

try {
  require(fixtures.path('syntax', 'bad_syntax'));
} catch (err) {
  arrowMessage =
      binding.getHiddenValue(err, kArrowMessagePrivateSymbolIndex);
}

assert(/bad_syntax\.js:1/.test(arrowMessage));
